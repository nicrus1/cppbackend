#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <filesystem>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <sstream>

#include "game.h"
#include "serializing_listener.h"

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
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
    if (global_listener) {
        global_listener->OnShutdown();
    }
}

// Генерация случайного токена
std::string GenerateToken() {
    static const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, chars.size() - 1);
    
    std::string token;
    token.reserve(32);
    for (int i = 0; i < 32; ++i) {
        token.push_back(chars[dist(rng)]);
    }
    return token;
}

// Обработчик HTTP запросов
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(net::ip::tcp::socket socket, std::shared_ptr<model::Game> game)
        : socket_(std::move(socket))
        , game_(game) {
    }
    
    void Run() {
        ReadRequest();
    }
    
private:
    void ReadRequest() {
        auto self = shared_from_this();
        http::async_read(socket_, buffer_, request_,
            [self](beast::error_code ec, size_t) {
                if (!ec) {
                    self->ProcessRequest();
                } else {
                    std::cerr << "Read error: " << ec.message() << std::endl;
                }
            });
    }
    
    void ProcessRequest() {
        response_.version(request_.version());
        response_.keep_alive(false);
        response_.set(http::field::server, "Game Server");
        response_.set(http::field::content_type, "application/json");
        response_.set(http::field::access_control_allow_origin, "*");
        
        try {
            std::string target = request_.target().to_string();
            std::cout << "Request: " << request_.method_string() << " " << target << std::endl;
            
            if (request_.method() == http::verb::post && target == "/api/v1/game/join") {
                HandleJoin();
            } else if (request_.method() == http::verb::get && target == "/api/v1/game/maps") {
                HandleGetMaps();
            } else if (request_.method() == http::verb::get && target == "/api/v1/game/state") {
                HandleGetState();
            } else if (request_.method() == http::verb::post && target == "/api/v1/game/tick") {
                HandleTick();
            } else if (request_.method() == http::verb::get && target == "/api/v1/game/players") {
                HandleGetPlayers();
            } else {
                std::cout << "Not found: " << target << std::endl;
                response_.result(http::status::not_found);
                response_.body() = R"({"error":"Not found"})";
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing request: " << e.what() << std::endl;
            response_.result(http::status::internal_server_error);
            response_.body() = R"({"error":")" + std::string(e.what()) + R"("})";
        }
        
        WriteResponse();
    }
    
    void HandleJoin() {
        try {
            std::string body = request_.body();
            std::cout << "Join body: " << body << std::endl;
            
            // Проверяем, что тело не пустое
            if (body.empty()) {
                std::cerr << "Empty body in join request" << std::endl;
                response_.result(http::status::bad_request);
                response_.body() = R"({"error":"Empty body"})";
                return;
            }
            
            pt::ptree root;
            std::stringstream ss(body);
            pt::read_json(ss, root);
            
            std::string user_name = root.get<std::string>("userName");
            std::string map_id = root.get<std::string>("mapId");
            
            std::cout << "User: " << user_name << ", Map: " << map_id << std::endl;
            
            // Проверяем, что карта существует
            auto map_state = game_->GetMapState(map_id);
            if (!map_state) {
                std::cerr << "Map not found: " << map_id << std::endl;
                response_.result(http::status::not_found);
                response_.body() = R"({"error":"Map not found"})";
                return;
            }
            
            std::cout << "Map found: " << map_state->name << std::endl;
            std::cout << "Offices count: " << map_state->offices.size() << std::endl;
            
            // Генерируем токен
            std::string token = GenerateToken();
            std::cout << "Generated token: " << token << std::endl;
            
            // Создаем собаку
            uint32_t dog_id_num = game_->GetAllDogs().size() + 1;
            auto dog_id = model::Dog::Id{dog_id_num};
            std::cout << "Dog ID: " << dog_id_num << std::endl;
            
            // Выбираем стартовую позицию из офиса
            geom::Point2D pos(5.0, 5.0);
            if (!map_state->offices.empty()) {
                const auto& office = map_state->offices[0];
                pos.x = office.x + office.offsetX;
                pos.y = office.y + office.offsetY;
                std::cout << "Start position from office: (" << pos.x << ", " << pos.y << ")" << std::endl;
            } else {
                std::cout << "No offices, using default position" << std::endl;
            }
            
            auto dog = std::make_shared<model::Dog>(dog_id, user_name, pos, 3);
            std::cout << "Dog created" << std::endl;
            
            // Добавляем игрока
            game_->AddPlayer(token, user_name, map_id, dog);
            std::cout << "Player added" << std::endl;
            
            // Формируем ответ
            pt::ptree response_root;
            response_root.put("authToken", token);
            response_root.put("playerId", dog_id_num);
            
            pt::ptree map_info;
            map_info.put("id", map_id);
            map_info.put("name", map_state->name);
            response_root.add_child("map", map_info);
            
            std::stringstream response_ss;
            pt::write_json(response_ss, response_root);
            std::string response_body = response_ss.str();
            std::cout << "Response body: " << response_body << std::endl;
            
            response_.body() = response_body;
            response_.result(http::status::ok);
            
        } catch (const pt::ptree_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            response_.result(http::status::bad_request);
            response_.body() = R"({"error":"Invalid JSON"})";
        } catch (const std::exception& e) {
            std::cerr << "Join error: " << e.what() << std::endl;
            response_.result(http::status::bad_request);
            response_.body() = R"({"error":")" + std::string(e.what()) + R"("})";
        }
    }
    
    void HandleGetMaps() {
        pt::ptree response_root;
        pt::ptree maps_array;
        
        std::cout << "GetMaps: " << game_->GetMaps().size() << " maps" << std::endl;
        
        for (const auto& map_pair : game_->GetMaps()) {
            const auto& map_state = map_pair.second;
            pt::ptree map_node;
            map_node.put("id", map_state.map_id);
            map_node.put("name", map_state.name);
            map_node.put("dogSpeed", map_state.dog_speed);
            maps_array.push_back(std::make_pair("", map_node));
            std::cout << "  Map: " << map_state.map_id << " (" << map_state.name << ")" << std::endl;
        }
        
        response_root.add_child("maps", maps_array);
        
        std::stringstream ss;
        pt::write_json(ss, response_root);
        std::string body = ss.str();
        std::cout << "GetMaps response: " << body << std::endl;
        
        response_.body() = body;
        response_.result(http::status::ok);
    }
    
    void HandleGetState() {
        pt::ptree response_root;
        
        // Информация об игроках
        pt::ptree players_array;
        for (const auto& player_pair : game_->GetPlayers()) {
            const auto& player = player_pair.second;
            if (player.dog) {
                pt::ptree player_node;
                player_node.put("token", player.token);
                player_node.put("userId", player.user_id);
                player_node.put("mapId", player.map_id);
                player_node.put("dogId", *player.dog->GetId());
                player_node.put("score", player.dog->GetScore());
                
                pt::ptree pos_node;
                pos_node.put("x", player.dog->GetPosition().x);
                pos_node.put("y", player.dog->GetPosition().y);
                player_node.add_child("position", pos_node);
                
                players_array.push_back(std::make_pair("", player_node));
            }
        }
        response_root.add_child("players", players_array);
        
        // Информация о предметах
        pt::ptree loot_array;
        for (const auto& item : game_->GetAllLootItems()) {
            pt::ptree loot_node;
            loot_node.put("id", item.id);
            loot_node.put("type", item.type);
            
            pt::ptree pos_node;
            pos_node.put("x", item.position.x);
            pos_node.put("y", item.position.y);
            loot_node.add_child("position", pos_node);
            
            loot_array.push_back(std::make_pair("", loot_node));
        }
        response_root.add_child("loot", loot_array);
        
        response_root.put("gameTime", game_->GetGameTime());
        
        std::stringstream ss;
        pt::write_json(ss, response_root);
        response_.body() = ss.str();
        response_.result(http::status::ok);
    }
    
    void HandleTick() {
        try {
            std::string body = request_.body();
            pt::ptree root;
            std::stringstream ss(body);
            pt::read_json(ss, root);
            
            int time_delta = root.get<int>("timeDelta", 0);
            std::cout << "Tick: " << time_delta << " ms" << std::endl;
            
            if (time_delta > 0) {
                game_->Tick(app::milliseconds(time_delta));
            }
            
            response_.result(http::status::ok);
            response_.body() = R"({"status":"ok"})";
            
        } catch (const std::exception& e) {
            std::cerr << "Tick error: " << e.what() << std::endl;
            response_.result(http::status::bad_request);
            response_.body() = R"({"error":")" + std::string(e.what()) + R"("})";
        }
    }
    
    void HandleGetPlayers() {
        pt::ptree response_root;
        pt::ptree players_array;
        
        for (const auto& player_pair : game_->GetPlayers()) {
            const auto& player = player_pair.second;
            if (player.dog) {
                pt::ptree player_node;
                player_node.put("token", player.token);
                player_node.put("userId", player.user_id);
                player_node.put("mapId", player.map_id);
                player_node.put("dogId", *player.dog->GetId());
                player_node.put("score", player.dog->GetScore());
                
                pt::ptree pos_node;
                pos_node.put("x", player.dog->GetPosition().x);
                pos_node.put("y", player.dog->GetPosition().y);
                player_node.add_child("position", pos_node);
                
                players_array.push_back(std::make_pair("", player_node));
            }
        }
        
        response_root.add_child("players", players_array);
        
        std::stringstream ss;
        pt::write_json(ss, response_root);
        response_.body() = ss.str();
        response_.result(http::status::ok);
    }
    
    void WriteResponse() {
        auto self = shared_from_this();
        http::async_write(socket_, response_,
            [self](beast::error_code ec, size_t) {
                if (ec) {
                    std::cerr << "Write error: " << ec.message() << std::endl;
                }
                self->socket_.shutdown(net::ip::tcp::socket::shutdown_send, ec);
            });
    }
    
    net::ip::tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    http::response<http::string_body> response_;
    std::shared_ptr<model::Game> game_;
};

// Функция для загрузки карт из конфигурационного файла
void LoadMapsFromConfig(const std::string& config_path, std::shared_ptr<model::Game> game) {
    try {
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Config file not found: " << config_path << std::endl;
            if (!game->HasMap("map1")) {
                game->AddMap("map1", 3.0);
                std::cout << "Created default map1" << std::endl;
            }
            return;
        }
        
        pt::ptree root;
        pt::read_json(config_path, root);
        
        // Загружаем настройки генерации лута
        double loot_period = root.get<double>("lootGeneratorConfig.period", 5.0);
        double loot_probability = root.get<double>("lootGeneratorConfig.probability", 0.5);
        double default_dog_speed = root.get<double>("defaultDogSpeed", 3.0);
        
        std::cout << "Loot config: period=" << loot_period << ", probability=" << loot_probability << std::endl;
        
        // Парсим карты
        if (root.count("maps") > 0) {
            for (const auto& map_node : root.get_child("maps")) {
                const auto& map_data = map_node.second;
                
                std::string map_id = map_data.get<std::string>("id");
                std::string map_name = map_data.get<std::string>("name", map_id);
                double dog_speed = map_data.get<double>("dogSpeed", default_dog_speed);
                
                std::cout << "Loading map: " << map_id << " (" << map_name << ")" << std::endl;
                
                // Добавляем карту
                if (!game->HasMap(map_id)) {
                    game->AddMap(map_id, dog_speed);
                    std::cout << "  Created map: " << map_id << std::endl;
                }
                
                auto map_state = game->GetMapState(map_id);
                if (map_state) {
                    map_state->name = map_name;
                    map_state->dog_speed = dog_speed;
                    map_state->default_dog_speed = dog_speed;
                    
                    // Загружаем типы лута
                    if (map_data.count("lootTypes") > 0) {
                        for (const auto& loot_node : map_data.get_child("lootTypes")) {
                            model::LootType loot_type;
                            loot_type.name = loot_node.second.get<std::string>("name");
                            loot_type.file = loot_node.second.get<std::string>("file");
                            loot_type.type = loot_node.second.get<std::string>("type");
                            loot_type.rotation = loot_node.second.get<double>("rotation", 0);
                            loot_type.color = loot_node.second.get<std::string>("color");
                            loot_type.scale = loot_node.second.get<double>("scale", 0.01);
                            loot_type.value = loot_node.second.get<uint32_t>("value", 0);
                            map_state->loot_types.push_back(loot_type);
                            std::cout << "  Loot type: " << loot_type.name << " (value=" << loot_type.value << ")" << std::endl;
                        }
                    }
                    
                    // Загружаем дороги
                    if (map_data.count("roads") > 0) {
                        for (const auto& road_node : map_data.get_child("roads")) {
                            model::RoadSegment road;
                            road.x0 = road_node.second.get<double>("x0", 0);
                            road.y0 = road_node.second.get<double>("y0", 0);
                            
                            if (road_node.second.count("x1") > 0) {
                                road.x1 = road_node.second.get<double>("x1");
                                road.has_x1 = true;
                            }
                            if (road_node.second.count("y1") > 0) {
                                road.y1 = road_node.second.get<double>("y1");
                                road.has_y1 = true;
                            }
                            map_state->roads.push_back(road);
                        }
                        std::cout << "  Roads: " << map_state->roads.size() << std::endl;
                    }
                    
                    // Загружаем здания
                    if (map_data.count("buildings") > 0) {
                        for (const auto& building_node : map_data.get_child("buildings")) {
                            model::Building building;
                            building.x = building_node.second.get<double>("x", 0);
                            building.y = building_node.second.get<double>("y", 0);
                            building.w = building_node.second.get<double>("w", 0);
                            building.h = building_node.second.get<double>("h", 0);
                            map_state->buildings.push_back(building);
                        }
                        std::cout << "  Buildings: " << map_state->buildings.size() << std::endl;
                    }
                    
                    // Загружаем офисы
                    if (map_data.count("offices") > 0) {
                        for (const auto& office_node : map_data.get_child("offices")) {
                            model::Office office;
                            office.id = office_node.second.get<std::string>("id");
                            office.x = office_node.second.get<double>("x", 0);
                            office.y = office_node.second.get<double>("y", 0);
                            office.offsetX = office_node.second.get<double>("offsetX", 0);
                            office.offsetY = office_node.second.get<double>("offsetY", 0);
                            map_state->offices.push_back(office);
                            std::cout << "  Office: " << office.id << " at (" << office.x << ", " << office.y << ")" << std::endl;
                        }
                    }
                    
                    // Устанавливаем настройки генерации лута
                    map_state->loot_period_ms = static_cast<uint64_t>(loot_period * 1000);
                    map_state->loot_probability = loot_probability;
                    
                    std::cout << "  Loot period: " << map_state->loot_period_ms << " ms" << std::endl;
                    std::cout << "  Loot probability: " << map_state->loot_probability << std::endl;
                    std::cout << "  Loot types: " << map_state->loot_types.size() << std::endl;
                }
            }
        }
        
        std::cout << "Loaded " << root.get_child("maps").size() << " maps from config" << std::endl;
        std::cout << "Total maps in game: " << game->GetMaps().size() << std::endl;
        
        // Выводим все загруженные карты
        for (const auto& map_pair : game->GetMaps()) {
            std::cout << "Map in game: " << map_pair.first << " (" << map_pair.second.name << ")" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        if (!game->HasMap("map1")) {
            game->AddMap("map1", 3.0);
            std::cout << "Created default map1 due to config error" << std::endl;
        }
    }
}

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
            ("www-root", po::value<std::string>(), "Path to www root")
            ("port", po::value<int>()->default_value(8080), "HTTP server port");
        
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
        int port = vm["port"].as<int>();
        
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
                game->AddMap("map1", 3.0);
                std::cout << "Created default map1" << std::endl;
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
        net::io_context ioc(1);
        global_ioc = &ioc;
        
        net::ip::tcp::acceptor acceptor(ioc, net::ip::tcp::endpoint(net::ip::tcp::v4(), port));
        
        std::cout << "HTTP server started on port " << port << std::endl;
        std::cout << "Server started. Press Ctrl+C to stop." << std::endl;
        std::cout << "Game time: " << game->GetGameTime() << " ms" << std::endl;
        std::cout << "Number of maps: " << game->GetMaps().size() << std::endl;
        
        // Выводим информацию о картах
        for (const auto& map_pair : game->GetMaps()) {
            std::cout << "  Map: " << map_pair.first << " (" << map_pair.second.name << ")" << std::endl;
        }
        
        std::cout << "Number of dogs: " << game->GetAllDogs().size() << std::endl;
        
        // Запускаем отдельный поток для игровых тиков
        int tick_period_ms = 50;
        if (vm.count("tick-period")) {
            tick_period_ms = vm["tick-period"].as<int>();
            if (tick_period_ms <= 0) {
                throw std::runtime_error("Tick period must be positive");
            }
        }
        std::cout << "Tick period: " << tick_period_ms << " ms" << std::endl;
        
        std::thread game_thread([&game, tick_period_ms]() {
            int tick_count = 0;
            while (running) {
                auto start = std::chrono::steady_clock::now();
                
                game->Tick(app::milliseconds(tick_period_ms));
                
                tick_count++;
                if (tick_count % 100 == 0) {
                    std::cout << "Tick " << tick_count << ", game time: " 
                              << game->GetGameTime() << " ms" << std::endl;
                }
                
                auto end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                
                if (elapsed.count() < tick_period_ms) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(tick_period_ms - elapsed.count()));
                }
            }
        });
        
        // Асинхронный accept
        auto do_accept = [&](auto&& self_ref) -> void {
            acceptor.async_accept(
                [&, self_ref](beast::error_code ec, net::ip::tcp::socket socket) {
                    if (!ec && running) {
                        std::cout << "New connection accepted" << std::endl;
                        auto session = std::make_shared<HttpSession>(std::move(socket), game);
                        session->Run();
                    }
                    if (running) {
                        self_ref(self_ref);
                    }
                });
        };
        
        do_accept(do_accept);
        
        // Запускаем ioc в отдельном потоке
        std::thread ioc_thread([&ioc]() {
            ioc.run();
        });
        
        // Ждем завершения
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Останавливаем ioc
        ioc.stop();
        
        // Ждем завершения потоков
        if (ioc_thread.joinable()) {
            ioc_thread.join();
        }
        
        if (game_thread.joinable()) {
            game_thread.join();
        }
        
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