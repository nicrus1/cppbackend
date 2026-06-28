#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <csignal>

#include "game.h"
#include "serializing_listener.h"

namespace net = boost::asio;
namespace sys = boost::system;
namespace po = boost::program_options;

// Глобальные переменные для обработки сигналов
std::shared_ptr<infrastructure::SerializingListener> global_listener;

void SignalHandler(int signal) {
    if (global_listener) {
        global_listener->OnShutdown();
    }
    exit(signal);
}

int main(int argc, char* argv[]) {
    try {
        // Парсинг параметров командной строки
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "Show help message")
            ("state-file", po::value<std::string>(), "Path to state file")
            ("save-state-period", po::value<int>(), "Auto-save period in milliseconds")
            ("tick-period", po::value<int>(), "Tick period in milliseconds")
            ("config-file", po::value<std::string>(), "Path to config file");
        
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        // Проверяем параметры
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

        // Инициализация игры
        auto game = std::make_shared<model::Game>();
        
        // Если есть файл состояния, восстанавливаем состояние
        if (!state_file.empty() && std::filesystem::exists(state_file)) {
            serialization::GameState loaded_state;
            if (serialization::StateSerializer::LoadFromFile(loaded_state, state_file)) {
                // Восстанавливаем состояние игры из loaded_state
                // Это зависит от вашей реализации Game
                // game->RestoreState(loaded_state);
                std::cout << "State restored from " << state_file << std::endl;
            } else {
                std::cerr << "Failed to restore state from " << state_file << std::endl;
                return EXIT_FAILURE;
            }
        } else if (!state_file.empty()) {
            std::cout << "Starting with clean state" << std::endl;
        }

        // Создаем слушатель для сериализации
        auto listener = std::make_shared<infrastructure::SerializingListener>(
            game, state_file, save_period
        );
        global_listener = listener;

        // Подключаем слушатель к игре
        // game->AddListener(listener);

        // Настраиваем обработчики сигналов
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        // Запускаем основной цикл
        net::io_context ioc;
        
        // ... остальной код сервера ...
        
        // В конце, если есть state_file, сохраняем состояние
        if (!state_file.empty()) {
            listener->OnShutdown();
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}