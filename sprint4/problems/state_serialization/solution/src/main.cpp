#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <csignal>
#include <thread>
#include <chrono>

#include "game.h"
#include "serializing_listener.h"

namespace net = boost::asio;
namespace sys = boost::system;
namespace po = boost::program_options;

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

int main(int argc, char* argv[]) {
    try {
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
        
        std::string state_file;
        std::chrono::milliseconds save_period(0);
        
        if (has_state_file) {
            state_file = vm["state-file"].as<std::string>();
            
            if (has_save_period) {
                int period_ms = vm["save-state-period"].as<int>();
                if (period_ms < 0) {
                    throw std::runtime_error("Save period must be non-negative");
                }
                save_period = std::chrono::milliseconds(period_ms);
            }
        }

        auto game = std::make_shared<model::Game>();
        global_game = game;
        
        // Создаем тестовую карту для демонстрации
        if (!game->HasMap("map1")) {
            game->AddMap("map1");
        }
        
        // Добавляем тестовую собаку
        if (game->GetDogs("map1").empty()) {
            auto dog = std::make_shared<model::Dog>(
                model::Dog::Id{1}, 
                "TestDog", 
                geom::Point2D{10, 10}, 
                5
            );
            game->AddDog("map1", dog);
        }
        
        if (!state_file.empty() && std::filesystem::exists(state_file)) {
            serialization::GameState loaded_state;
            if (serialization::StateSerializer::LoadFromFile(loaded_state, state_file)) {
                game->RestoreState(loaded_state);
                std::cout << "State restored from " << state_file << std::endl;
            } else {
                std::cerr << "Failed to restore state from " << state_file << std::endl;
            }
        } else if (!state_file.empty()) {
            std::cout << "Starting with clean state" << std::endl;
        }

        auto listener = std::make_shared<infrastructure::SerializingListener>(
            game, state_file, save_period
        );
        global_listener = listener;

        game->AddListener(listener);

        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        std::cout << "Server started. Press Ctrl+C to stop." << std::endl;
        
        // Основной игровой цикл
        int tick_period_ms = 50; // 50ms по умолчанию
        if (vm.count("tick-period")) {
            tick_period_ms = vm["tick-period"].as<int>();
        }
        
        while (running) {
            auto start = std::chrono::steady_clock::now();
            
            game->Tick(app::milliseconds(tick_period_ms));
            
            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            if (elapsed.count() < tick_period_ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(tick_period_ms - elapsed.count()));
            }
        }
        
        if (!state_file.empty()) {
            listener->OnShutdown();
        }

        std::cout << "Server stopped." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}