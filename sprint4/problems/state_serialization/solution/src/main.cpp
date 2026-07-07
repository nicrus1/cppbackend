#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <filesystem>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <sstream>
#include <random>

#include "game.h"
#include "serializing_listener.h"

namespace net = boost::asio;
namespace po = boost::program_options;
namespace pt = boost::property_tree;

std::shared_ptr<infrastructure::SerializingListener> global_listener;
std::shared_ptr<model::Game> global_game;
std::atomic<bool> running{true};
net::io_context* global_ioc = nullptr;

void SignalHandler(int signal) {
    std::cout << "Received signal " << signal << std::endl;
    running = false;
    if (global_ioc) {
        global_ioc->stop();
    }
}

// Функция для загрузки карт из конфигурационного файла
void LoadMapsFromConfig(const std::string& config_path, std::shared_ptr<model::Game> game) {
    try {
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Config file not found: " << config_path << std::endl;
            if (!game->HasMap("map1")) {
                game->AddMap("map1");
                std::cout << "Created default map1" << std::endl;
            }
            return;
        }
        
        pt::ptree root;
        pt::read_json(config_path, root);
        
        if (root.count("maps") > 0) {
            for (const auto& map_node : root.get_child("maps")) {
                const auto& map_data = map_node.second;
                std::string map_id = map_data.get<std::string>("id");
                std::cout << "Loading map: " << map_id << std::endl;
                
                if (!game->HasMap(map_id)) {
                    game->AddMap(map_id);
                }
            }
        }
        
        std::cout << "Loaded " << root.get_child("maps").size() << " maps from config" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        if (!game->HasMap("map1")) {
            game->AddMap("map1");
            std::cout << "Created default map1 due to config error" << std::endl;
        }
    }
}

// Простой HTTP обработчик
class HttpHandler {
public:
    HttpHandler(std::shared_ptr<model::Game> game) : game_(game), id_counter_(1) {}
    
    std::string HandleRequest(const std::string& request) {
        std::istringstream iss(request);
        std::string method, path, version;
        iss >> method >> path >> version;
        
        std::cout << "Handling " << method << " " << path << std::endl;
        
        if (method == "GET") {
            if (path.find("/api/v1/maps") == 0) {
                return HandleGetMaps();
            } else if (path.find("/api/v1/game/players") == 0) {
                return HandleGetPlayers();
            } else if (path.find("/api/v1/game/join") == 0) {
                return HandleJoin(path);
            }
        } else if (method == "POST") {
            if (path.find("/api/v1/game/join") == 0) {
                return HandleJoin(path);
            } else if (path.find("/api/v1/game/tick") == 0) {
                return HandleTick();
            } else if (path.find("/api/v1/game/action") == 0) {
                return HandleAction(path);
            }
        }
        
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    }
    
private:
    std::string HandleGetMaps() {
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        
        // Формируем JSON со списком карт
        std::string json = "[";
        bool first = true;
        for (const auto& map_pair : game_->GetMaps()) {
            if (!first) json += ",";
            first = false;
            json += "{\"id\":\"" + map_pair.first + "\"}";
        }
        json += "]";
        
        response += "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n";
        response += json;
        return response;
    }
    
    std::string HandleGetPlayers() {
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        
        std::string json = "[";
        bool first = true;
        for (const auto& player_pair : game_->GetPlayers()) {
            if (!first) json += ",";
            first = false;
            json += "{\"token\":\"" + player_pair.first + "\",";
            json += "\"user_id\":\"" + player_pair.second.user_id + "\"}";
        }
        json += "]";
        
        response += "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n";
        response += json;
        return response;
    }
    
    std::string HandleJoin(const std::string& path) {
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        
        // Парсим параметры из пути
        std::string name, map_id;
        size_t name_pos = path.find("name=");
        size_t map_pos = path.find("mapId=");
        
        if (name_pos != std::string::npos) {
            size_t end = path.find("&", name_pos);
            name = path.substr(name_pos + 5, end - name_pos - 5);
        }
        if (map_pos != std::string::npos) {
            size_t end = path.find("&", map_pos);
            if (end == std::string::npos) end = path.length();
            map_id = path.substr(map_pos + 6, end - map_pos - 6);
        }
        
        if (name.empty() || map_id.empty()) {
            return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        }
        
        // Создаем токен и собаку
        std::string token = "token_" + std::to_string(id_counter_++);
        auto dog = std::make_shared<model::Dog>(
            model::Dog::Id{id_counter_}, 
            name, 
            geom::Point2D{0, 0}, 
            10
        );
        
        game_->AddPlayer(token, name, map_id, dog);
        
        std::string json = "{\"authToken\":\"" + token + "\"}";
        response += "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n";
        response += json;
        return response;
    }
    
    std::string HandleTick() {
        game_->Tick(app::milliseconds(100));
        return "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}";
    }
    
    std::string HandleAction(const std::string& path) {
        std::string action;
        size_t action_pos = path.find("action=");
        if (action_pos != std::string::npos) {
            size_t end = path.find("&", action_pos);
            if (end == std::string::npos) end = path.length();
            action = path.substr(action_pos + 7, end - action_pos - 7);
        }
        
        // Обрабатываем действие
        if (!action.empty()) {
            // Обновляем скорость собаки в зависимости от действия
            for (auto& dog : game_->GetAllDogs()) {
                if (action == "move") {
                    dog->SetSpeed({1.0, 0});
                } else if (action == "stop") {
                    dog->SetSpeed({0, 0});
                }
            }
        }
        
        return "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}";
    }
    
    std::shared_ptr<model::Game> game_;
    int id_counter_;
};

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(net::ip::tcp::socket socket, std::shared_ptr<HttpHandler> handler)
        : socket_(std::move(socket)), handler_(handler) {}
    
    void Start() {
        do_read();
    }
    
private:
    void do_read() {
        auto self = shared_from_this();
        socket_.async_read_some(
            net::buffer(buffer_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::string request(buffer_.data(), length);
                    std::string response = handler_->HandleRequest(request);
                    do_write(response);
                }
            });
    }
    
    void do_write(const std::string& response) {
        auto self = shared_from_this();
        net::async_write(
            socket_, 
            net::buffer(response),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    do_read();
                }
            });
    }
    
    net::ip::tcp::socket socket_;
    std::shared_ptr<HttpHandler> handler_;
    enum { max_length = 1024 };
    char buffer_[max_length];
};

class Server {
public:
    Server(net::io_context& ioc, short port, std::shared_ptr<model::Game> game)
        : ioc_(ioc), 
          acceptor_(ioc, net::ip::tcp::endpoint(net::ip::tcp::v4(), port)),
          handler_(std::make_shared<HttpHandler>(game)) {}
    
    void Start() {
        do_accept();
    }
    
private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, net::ip::tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), handler_)->Start();
                }
                do_accept();
            });
    }
    
    net::io_context& ioc_;
    net::ip::tcp::acceptor acceptor_;
    std::shared_ptr<HttpHandler> handler_;
};

int main(int argc, char* argv[]) {
    try {
        std::cout << "Starting game server..." << std::endl;
        
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "Show help message")
            ("state-file", po::value<std::string>(), "Path to state file")
            ("save-state-period", po::value<int>(), "Auto-save period in milliseconds")
            ("tick-period", po::value<int>(), "Tick period in milliseconds")
            ("config-file", po::value<std::string>(), "Path to config file")
            ("www-root", po::value<std::string>(), "Path to www root");
        
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        bool has_state_file = vm.count("state-file") > 0;
        bool has_save_period = vm.count("save-state-period") > 0;
        bool has_config_file = vm.count("config-file") > 0;
        
        std::string state_file;
        std::chrono::milliseconds save_period(0);
        
        if (has_state_file) {
            state_file = vm["state-file"].as<std::string>();
            std::cout << "State file: " << state_file << std::endl;
            
            if (has_save_period) {
                int period_ms = vm["save-state-period"].as<int>();
                if (period_ms < 0) {
                    throw std::runtime_error("Save period must be non-negative");
                }
                save_period = std::chrono::milliseconds(period_ms);
                std::cout << "Save period: " << period_ms << " ms" << std::endl;
            }
        }

        auto game = std::make_shared<model::Game>();
        global_game = game;
        
        // Загружаем карты из конфигурационного файла
        if (has_config_file) {
            std::string config_path = vm["config-file"].as<std::string>();
            std::cout << "Config file: " << config_path << std::endl;
            LoadMapsFromConfig(config_path, game);
        } else {
            std::cout << "No config file specified, creating test map" << std::endl;
            if (!game->HasMap("map1")) {
                game->AddMap("map1");
            }
        }
        
        // Восстанавливаем состояние из файла если он существует
        if (!state_file.empty() && std::filesystem::exists(state_file)) {
            serialization::GameState loaded_state;
            if (serialization::StateSerializer::LoadFromFile(loaded_state, state_file)) {
                try {
                    game->RestoreState(loaded_state);
                    std::cout << "State restored from " << state_file << std::endl;
                    std::cout << "Restored " << game->GetMaps().size() << " maps" << std::endl;
                    std::cout << "Restored " << game->GetAllDogs().size() << " dogs" << std::endl;
                    std::cout << "Restored " << game->GetAllLootItems().size() << " loot items" << std::endl;
                    std::cout << "Restored " << game->GetPlayers().size() << " players" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Failed to restore state: " << e.what() << std::endl;
                    return EXIT_FAILURE;
                }
            } else {
                std::cerr << "Failed to restore state from " << state_file << std::endl;
                return EXIT_FAILURE;
            }
        } else if (!state_file.empty()) {
            std::cout << "Starting with clean state" << std::endl;
        }

        // Создаем слушатель для автоматического сохранения
        auto listener = std::make_shared<infrastructure::SerializingListener>(
            game, state_file, save_period
        );
        global_listener = listener;
        game->AddListener(listener);

        // Настраиваем обработчики сигналов
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        // Запускаем HTTP сервер
        net::io_context ioc;
        global_ioc = &ioc;
        Server server(ioc, 8080, game);
        server.Start();

        std::cout << "Server started on port 8080. Press Ctrl+C to stop." << std::endl;
        std::cout << "Game time: " << game->GetGameTime() << " ms" << std::endl;
        std::cout << "Number of maps: " << game->GetMaps().size() << std::endl;
        std::cout << "Number of dogs: " << game->GetAllDogs().size() << std::endl;
        
        // Основной игровой цикл с тиками
        int tick_period_ms = 50;
        if (vm.count("tick-period")) {
            tick_period_ms = vm["tick-period"].as<int>();
            if (tick_period_ms <= 0) {
                throw std::runtime_error("Tick period must be positive");
            }
        }
        std::cout << "Tick period: " << tick_period_ms << " ms" << std::endl;
        
        // Запускаем отдельный поток для тиков
        std::thread tick_thread([&]() {
            int tick_count = 0;
            while (running) {
                auto start = std::chrono::steady_clock::now();
                game->Tick(app::milliseconds(tick_period_ms));
                
                tick_count++;
                if (tick_count % 100 == 0) {
                    std::cout << "Tick " << tick_count << ", game time: " << game->GetGameTime() << " ms" << std::endl;
                }
                
                auto end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                
                if (elapsed.count() < tick_period_ms) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(tick_period_ms - elapsed.count()));
                }
            }
        });
        
        // Запускаем обработку асинхронных операций
        ioc.run();
        
        // Ждем завершения потока тиков
        tick_thread.join();
        
        // Сохраняем состояние при завершении
        if (!state_file.empty()) {
            std::cout << "Saving state on shutdown..." << std::endl;
            listener->OnShutdown();
        }

        std::cout << "Server stopped." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}