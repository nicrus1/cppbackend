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

#include "command_line.h"
#include "http_server.h"
#include "request_handler.h"
#include "game.h"
#include "serializing_listener.h"
#include "model_serialization.h"

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
        // 1. Парсим аргументы через правильный модуль
        auto args_opt = ParseCommandLine(argc, argv);
        if (!args_opt) return 0;
        auto args = *args_opt;

        auto game = std::make_shared<model::Game>();
        global_game = game;
        
        // 2. Загружаем конфигурацию
        LoadMapsFromConfig(args.config_file, game);

        // 3. Восстанавливаем состояние из файла, если он указан и существует
        if (!args.state_file.empty() && std::filesystem::exists(args.state_file)) {
            serialization::GameState loaded_state;
            if (serialization::StateSerializer::LoadFromFile(loaded_state, args.state_file)) {
                game->RestoreState(loaded_state);
                std::cout << "State restored from " << args.state_file << std::endl;
            }
        }

        // 4. Подключаем слушателя автосохранения
        auto listener = std::make_shared<infrastructure::SerializingListener>(
            game, args.state_file, std::chrono::milliseconds(args.save_state_period)
        );
        global_listener = listener;
        game->AddListener(listener);

        // 5. Настраиваем сигналы
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        // 6. Инициализируем Asio и запускаем поток таймера (если tick_period > 0)
        net::io_context ioc(1);
        global_ioc = &ioc;

        if (args.tick_period > 0) {
            std::thread game_thread([&game, tick_period = args.tick_period]() {
                while (running) {
                    auto start = std::chrono::steady_clock::now();
                    game->Tick(app::milliseconds(tick_period));
                    auto end = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                    if (elapsed.count() < tick_period) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(tick_period - elapsed.count()));
                    }
                }
            });
            game_thread.detach();
        }

        // 7. Создаем правильный RequestHandler
        bool manual_tick = (args.tick_period == 0);
        auto handler = std::make_shared<http_handler::RequestHandler>(*game, args.www_root, manual_tick);

        // 8. Запускаем HTTP-сервер через готовую архитектуру!
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        
        http_server::ServeHttp(ioc, {address, port}, [handler](auto&& req, auto&& send) {
            (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::cout << "Server started on port " << port << std::endl;
        std::cout << "Game time: " << game->GetGameTime() << " ms" << std::endl;
        std::cout << "Number of maps: " << game->GetMaps().size() << std::endl;
        std::cout << "Number of dogs: " << game->GetAllDogs().size() << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl;
        
        ioc.run();

        // Сохраняем состояние при штатном завершении
        if (!args.state_file.empty()) {
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