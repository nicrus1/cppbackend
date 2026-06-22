#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <chrono>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "command_line.h"
#include "ticker.h"

using namespace std::literals;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    logger::InitLogging();
    
    std::cerr << "=== Game Server Starting ===" << std::endl;
    
    int exit_code = EXIT_SUCCESS;
    std::string exception_msg;
    
    try {
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            return EXIT_SUCCESS;
        }

        std::cerr << "Loading game config from: " << args->config_file << std::endl;
        model::Game game = json_loader::LoadGame(args->config_file);
        std::cerr << "Game config loaded successfully" << std::endl;

        const unsigned num_threads = std::thread::hardware_concurrency();
        std::cerr << "Using " << num_threads << " threads" << std::endl;
        
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code&, int signal) {
            std::cerr << "Received signal " << signal << ", stopping server..." << std::endl;
            ioc.stop();
        });

        auto api_strand = net::make_strand(ioc);

        std::cerr << "Creating RequestHandler..." << std::endl;
        bool manual_tick_allowed = (args->tick_period == 0);
        http_handler::RequestHandler handler{game, args->www_root, manual_tick_allowed};
        
        // Сначала загружаем экстра данные (типы лута)
        handler.LoadExtraData(args->config_file);
        
        // Теперь инициализируем менеджеры лута, когда типы уже загружены
        handler.InitializeLootManagers();
        
        std::cerr << "RequestHandler created" << std::endl;

        std::shared_ptr<Ticker> ticker;
        if (!manual_tick_allowed) {
            ticker = std::make_shared<Ticker>(
                api_strand,
                std::chrono::milliseconds(args->tick_period),
                [&handler](std::chrono::milliseconds delta) {
                    handler.Tick(delta);
                }
            );
            ticker->Start();
        }

        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;
        
        logger::LogServerStarted(address.to_string(), port);
        std::cerr << "Starting HTTP server on " << address.to_string() << ":" << port << std::endl;
        
        http_server::ServeHttp(ioc, {address, port}, [&handler, api_strand](auto&& req, auto&& send) {
            std::string target = std::string(req.target());

            if (target.find("/api/") == 0) {
                net::dispatch(api_strand, [&handler, req = std::move(req), send = std::move(send)]() mutable {
                    handler(std::move(req), std::move(send));
                });
            } else {
                handler(std::move(req), std::move(send));
            }
        });

        std::cerr << "Server is running. Press Ctrl+C to stop." << std::endl;
        
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
        std::cerr << "Server stopped" << std::endl;
        
    } catch (const std::exception& ex) {
        exit_code = EXIT_FAILURE;
        exception_msg = ex.what();
        std::cerr << "FATAL ERROR: " << exception_msg << std::endl;
    }
    
    logger::LogServerExited(exit_code, exception_msg);
    std::cerr << "Server exited with code " << exit_code << std::endl;
    
    return exit_code;
}