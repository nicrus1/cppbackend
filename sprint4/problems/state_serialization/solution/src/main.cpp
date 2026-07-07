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

#include "game.h"
#include "serializing_listener.h"
#include "request_handler.h"
#include "http_server.h"
#include "command_line.h"
#include "db/record_manager.h"

namespace net = boost::asio;
namespace po = boost::program_options;
namespace pt = boost::property_tree;

std::shared_ptr<infrastructure::SerializingListener> global_listener;
std::shared_ptr<model::Game> global_game;
std::shared_ptr<http_handler::RequestHandler> global_handler;
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

// Функция для загрузки карт из конфигурационного файла
void LoadMapsFromConfig(const std::string& config_path, model::Game& game) {
    try {
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Config file not found: " << config_path << std::endl;
            // Создаем тестовую карту для демонстрации
            if (!game.HasMap("map1")) {
                game.AddMap("map1", 3.0);
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
        
        // Парсим карты
        if (root.count("maps") > 0) {
            for (const auto& map_node : root.get_child("maps")) {
                const auto& map_data = map_node.second;
                game.LoadMapFromConfig(map_data);
                
                // Устанавливаем настройки генерации лута для каждой карты
                std::string map_id = map_data.get<std::string>("id");
                auto map_state = game.GetMapState(map_id);
                if (map_state) {
                    map_state->loot_period_ms = static_cast<uint64_t>(loot_period * 1000);
                    map_state->loot_probability = loot_probability;
                }
            }
        }
        
        std::cout << "Loaded " << root.get_child("maps").size() << " maps from config" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        if (!game.HasMap("map1")) {
            game.AddMap("map1", 3.0);
            std::cout << "Created default map1 due to config error" << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "Starting game server..." << std::endl;
        
        auto args_opt = ParseCommandLine(argc, argv);
        if (!args_opt) {
            return 0;
        }
        
        const auto& args = *args_opt;
        
        auto game = std::make_shared<model::Game>();
        global_game = game;
        
        // Загружаем карты из конфигурационного файла
        LoadMapsFromConfig(args.config_file, *game);
        
        // Восстанавливаем состояние из файла если он существует
        std::string state_file = "state";
        if (std::filesystem::exists(state_file)) {
            serialization::GameState loaded_state;
            if (serialization::StateSerializer::LoadFromFile(loaded_state, state_file)) {
                try {
                    game->RestoreState(loaded_state);
                    std::cout << "State restored from " << state_file << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Failed to restore state: " << e.what() << std::endl;
                }
            }
        }
        
        // Создаем обработчик запросов
        bool manual_tick_allowed = (args.tick_period == 0);
        auto handler = std::make_shared<http_handler::RequestHandler>(*game, args.www_root, manual_tick_allowed);
        global_handler = handler;
        
        // Загружаем дополнительные данные из конфига
        handler->LoadExtraData(std::filesystem::path(args.config_file));
        
        // Создаем слушатель для автоматического сохранения
        if (!state_file.empty()) {
            auto listener = std::make_shared<infrastructure::SerializingListener>(
                game, state_file, std::chrono::milliseconds(5000)
            );
            global_listener = listener;
            game->AddListener(listener);
        }
        
        // Настраиваем обработчики сигналов
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);
        
        // Запускаем HTTP сервер
        net::io_context ioc(1);
        global_ioc = &ioc;
        
        const auto address = net::ip::make_address("0.0.0.0");
        const auto port = static_cast<unsigned short>(8080);
        const net::ip::tcp::endpoint endpoint{address, port};
        
        http_server::ServeHttp(ioc, endpoint, [&handler](auto&& req, auto&& send) {
            (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        
        std::cout << "HTTP server started on port " << port << std::endl;
        std::cout << "Server started. Press Ctrl+C to stop." << std::endl;
        std::cout << "Number of maps: " << game->GetMaps().size() << std::endl;
        std::cout << "Number of dogs: " << game->GetAllDogs().size() << std::endl;
        
        // Запускаем отдельный поток для игровых тиков
        int tick_period_ms = args.tick_period > 0 ? args.tick_period : 50;
        std::cout << "Tick period: " << tick_period_ms << " ms" << std::endl;
        
        std::thread game_thread([&game, &handler, tick_period_ms]() {
            int tick_count = 0;
            while (running) {
                auto start = std::chrono::steady_clock::now();
                
                game->Tick(app::milliseconds(tick_period_ms));
                handler->Tick(std::chrono::milliseconds(tick_period_ms));
                
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
        
        // Запускаем io_context
        ioc.run();
        
        // Ждем завершения игрового потока
        if (game_thread.joinable()) {
            game_thread.join();
        }
        
        std::cout << "Server stopped." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}