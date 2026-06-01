#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

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
    
    auto args_opt = ParseCommandLine(argc, argv);
    
    if (!args_opt) {
        return EXIT_SUCCESS;  // --help was shown
    }
    
    const auto& args = *args_opt;
    
    int exit_code = EXIT_SUCCESS;
    std::string exception_msg;
    
    try {
        std::cerr << "Loading game config from: " << args.config_file << std::endl;
        model::Game game = json_loader::LoadGame(args.config_file);
        std::cerr << "Game config loaded successfully" << std::endl;

        const unsigned num_threads = std::thread::hardware_concurrency();
        std::cerr << "Using " << num_threads << " threads" << std::endl;
        
        net::io_context ioc(num_threads);
        
        auto api_strand = net::make_strand(ioc);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code&, int signal) {
            std::cerr << "Received signal " << signal << ", stopping server..." << std::endl;
            ioc.stop();
        });

        std::cerr << "Creating RequestHandler..." << std::endl;
        auto handler = std::make_shared<http_handler::RequestHandler>(game, args.www_root);
        std::cerr << "RequestHandler created" << std::endl;

        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;
        
        logger::LogServerStarted(address.to_string(), port);
        std::cerr << "Starting HTTP server on " << address.to_string() << ":" << port << std::endl;
        
        http_server::ServeHttp(ioc, {address, port}, 
            [handler](auto&& req, auto&& send) {
                (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
            }
        );
        
        // Setup ticker if tick period is specified
        std::shared_ptr<Ticker> ticker;
        if (args.tick_period > 0) {
            std::cerr << "Starting auto-tick mode with period " << args.tick_period << " ms" << std::endl;
            ticker = std::make_shared<Ticker>(
                api_strand,
                std::chrono::milliseconds(args.tick_period),
                [handler](std::chrono::milliseconds delta) {
                    handler->ProcessTick(delta.count());
                }
            );
            ticker->Start();
        } else {
            std::cerr << "Running in manual tick mode (use /api/v1/game/tick)" << std::endl;
        }

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