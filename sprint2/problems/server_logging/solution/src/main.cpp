#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"

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
    
    if (argc != 2) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    
    int exit_code = EXIT_SUCCESS;
    std::string exception_msg;
    
    try {
        std::cout << "Loading config from: " << argv[1] << std::endl;
        
        model::Game game = json_loader::LoadGame(argv[1]);

        std::cout << "Loaded " << game.GetMaps().size() << " map(s)" << std::endl;
        for (const auto& map : game.GetMaps()) {
            std::cout << "  Map id: '" << *map.GetId() << "', name: '" << map.GetName() << "'" << std::endl;
            std::cout << "    Roads: " << map.GetRoads().size() << std::endl;
            std::cout << "    Buildings: " << map.GetBuildings().size() << std::endl;
            std::cout << "    Offices: " << map.GetOffices().size() << std::endl;
        }

        const unsigned num_threads = std::thread::hardware_concurrency();
        std::cout << "Number of threads: " << num_threads << std::endl;
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code&, int) {
            std::cout << "Server shutting down..."sv << std::endl;
            ioc.stop();
        });

        http_handler::RequestHandler handler{game};

        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;
        
        std::cout << "Starting HTTP server on " << address << ":" << port << std::endl;
        
        // Логируем запуск сервера
        logger::LogServerStarted(address.to_string(), port);
        
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::cout << "Server has started..."sv << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        exit_code = EXIT_FAILURE;
        exception_msg = ex.what();
    }
    
    logger::LogServerExited(exit_code, exception_msg);
    
    return exit_code;
}