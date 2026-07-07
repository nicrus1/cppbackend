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

namespace net = boost::asio;
namespace po = boost::program_options;
namespace pt = boost::property_tree;

std::shared_ptr<infrastructure::SerializingListener> global_listener;
std::shared_ptr<model::Game> global_game;
std::atomic<bool> running{true};

void SignalHandler(int signal) {
    std::cout << "Received signal " << signal << std::endl;
    running = false;
    if (global_listener) {
        global_listener->OnShutdown();
    }
}

// Функция для загрузки карт из конфигурационного файла
void LoadMapsFromConfig(const std::string& config_path, std::shared_ptr<model::Game> game) {
    try {
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Config file not found: " << config_path << std::endl;
            // Создаем тестовую карту для демонстрации
            if (!game->HasMap("map1")) {
                game->AddMap("map1");
                std::cout << "Created default map1" << std::endl;
            }
            return;
        }
        
        pt::ptree root;
        pt::read_json(config_path, root);
        
        // Парсим карты
        if (root.count("maps") > 0) {
            for (const auto& map_node : root.get_child("maps")) {
                const auto& map_data = map_node.second;
                
                std::string map_id = map_data.get<std::string>("id");
                std::string map_name = map_data.get<std::string>("name", map_id);
                
                std::cout << "Loading map: " << map_id << " (" << map_name << ")" << std::endl;
                
                // Добавляем карту в игру
                if (!game->HasMap(map_id)) {
                    game->AddMap(map_id);
                }
            }
        }
        
        std::cout << "Loaded " << root.get_child("maps").size() << " maps from config" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        // Создаем тестовую карту в случае ошибки
        if (!game->HasMap("map1")) {
            game->AddMap("map1");
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
            // Если конфиг не указан, создаем тестовую карту для демонстрации
            std::cout << "No config file specified, creating test map" << std::endl;
            if (!game->HasMap("map1")) {
                game->AddMap("map1");
            }
        }
        
        // Создаем тестовую собаку для каждой карты, если собаки нет
        for (const auto& map_pair : game->GetMaps()) {
            const std::string& map_id = map_pair.first;
            if (game->GetDogs(map_id).empty()) {
                auto dog = std::make_shared<model::Dog>(
                    model::Dog::Id{static_cast<uint32_t>(map_pair.first.length() + 1)}, 
                    "TestDog_" + map_id, 
                    geom::Point2D{10, 10}, 
                    5
                );
                game->AddDog(map_id, dog);
                std::cout << "Created test dog on map: " << map_id << std::endl;
            }
        }
        
        // Восстанавливаем состояние из файла если он существует
        if (!state_file.empty() && std::filesystem::exists(state_file)) {
            serialization::GameState loaded_state;
            if (serialization::StateSerializer::LoadFromFile(loaded_state, state_file)) {
                try {
                    game->RestoreState(loaded_state);
                    std::cout << "State restored from " << state_file << std::endl;
                    
                    // Выводим информацию о восстановленном состоянии
                    std::cout << "Restored " << game->GetMaps().size() << " maps" << std::endl;
                    std::cout << "Restored " << game->GetAllDogs().size() << " dogs" << std::endl;
                    std::cout << "Restored " << game->GetAllLootItems().size() << " loot items" << std::endl;
                    std::cout << "Restored " << game->GetPlayers().size() << " players" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Failed to restore state: " << e.what() << std::endl;
                }
            } else {
                std::cerr << "Failed to restore state from " << state_file << std::endl;
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

        std::cout << "Server started. Press Ctrl+C to stop." << std::endl;
        std::cout << "Game time: " << game->GetGameTime() << " ms" << std::endl;
        std::cout << "Number of maps: " << game->GetMaps().size() << std::endl;
        std::cout << "Number of dogs: " << game->GetAllDogs().size() << std::endl;
        
        // Основной игровой цикл
        int tick_period_ms = 50; // 50ms по умолчанию
        if (vm.count("tick-period")) {
            tick_period_ms = vm["tick-period"].as<int>();
            if (tick_period_ms <= 0) {
                throw std::runtime_error("Tick period must be positive");
            }
        }
        std::cout << "Tick period: " << tick_period_ms << " ms" << std::endl;
        
        int tick_count = 0;
        while (running) {
            auto start = std::chrono::steady_clock::now();
            
            // Выполняем тик игры
            game->Tick(app::milliseconds(tick_period_ms));
            
            // Логируем каждый 100-й тик
            tick_count++;
            if (tick_count % 100 == 0) {
                std::cout << "Tick " << tick_count << ", game time: " << game->GetGameTime() << " ms" << std::endl;
            }
            
            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            // Если тик выполнился быстрее, чем tick_period, ждем
            if (elapsed.count() < tick_period_ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(tick_period_ms - elapsed.count()));
            }
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