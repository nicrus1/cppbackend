#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>

#include "game.h"
#include "request_handler.h"
#include "json_loader.h"
#include "http_server.h"
#include "logger.h"
#include "command_line.h"
#include "ticker.h"

namespace net = boost::asio;

std::shared_ptr<http_handler::RequestHandler> global_handler;
std::shared_ptr<model::Game> global_game;
std::atomic<bool> running{true};

void SignalHandler(int signal) {
    std::cout << "Received signal " << signal << std::endl;
    running = false;
    if (global_game) {
        global_game->Shutdown();
    }
}

int main(int argc, char* argv[]) {
    try {
        logger::InitLogging();
        
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            return 0;
        }

        // Загружаем игру из конфигурации
        model::Game game = json_loader::LoadGame(args->config_file);
        global_game = std::make_shared<model::Game>(std::move(game));

        // Создаем обработчик запросов
        bool manual_tick_allowed = (args->tick_period == 0);
        auto handler = std::make_shared<http_handler::RequestHandler>(
            *global_game, 
            args->www_root, 
            manual_tick_allowed
        );
        global_handler = handler;

        // Загружаем дополнительные данные из конфига
        handler->LoadExtraData(args->config_file);

        // Настраиваем сохранение состояния
        if (!args->state_file.empty()) {
            handler->SetStateFile(args->state_file);
            
            // Восстанавливаем состояние, если файл существует
            if (std::filesystem::exists(args->state_file)) {
                serialization::GameState state;
                if (serialization::StateSerializer::LoadFromFile(state, args->state_file)) {
                    handler->RestoreState(state);
                    logger::LogDebug("State restored from " + args->state_file);
                }
            }
            
            if (args->save_state_period > 0) {
                handler->SetSavePeriod(std::chrono::milliseconds(args->save_state_period));
            }
        }

        // Настраиваем обработку сигналов
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        // Настраиваем сеть
        net::io_context ioc(1);
        auto address = net::ip::make_address("0.0.0.0");
        unsigned short port = 8080;
        
        http_server::ServeHttp(ioc, {address, port}, [handler](auto&& req, auto&& send) {
            (*handler)(std::move(req), std::forward<decltype(send)>(send));
        });

        logger::LogServerStarted("0.0.0.0", port);
        std::cout << "Server started on port " << port << std::endl;
        std::cout << "Config file: " << args->config_file << std::endl;
        std::cout << "Static root: " << args->www_root << std::endl;

        // Запускаем таймер тиков, если задан период
        std::unique_ptr<Ticker> ticker;
        if (args->tick_period > 0) {
            auto strand = net::make_strand(ioc);
            ticker = std::make_unique<Ticker>(
                strand,
                std::chrono::milliseconds(args->tick_period),
                [handler](std::chrono::milliseconds delta) {
                    handler->Tick(delta);
                }
            );
            ticker->Start();
        }

        // Запускаем I/O контекст
        ioc.run();

        // Сохраняем состояние при завершении
        if (!args->state_file.empty()) {
            logger::LogDebug("Saving state on shutdown...");
            handler->SaveStateToFile();
        }

        logger::LogServerExited(0);
        return 0;
    } catch (const std::exception& e) {
        logger::LogError(0, e.what(), "main");
        logger::LogServerExited(EXIT_FAILURE, e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}