#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

// Твои заголовочные файлы
#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "command_line.h"
#include "ticker.h"
#include "model.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

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
    try {
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            return EXIT_SUCCESS;
        }

        // Исправлено: теперь вызывается InitLogging
        logger::InitLogging();

        model::Game game = json_loader::LoadGame(args->config_file);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        auto api_strand = net::make_strand(ioc);

        bool is_auto_tick = args->tick_period > 0;
        
        // Исправлено: передаем 4 аргумента, как требует логика
        auto handler = std::make_shared<http_handler::RequestHandler>(
            game, args->www_root, api_strand, is_auto_tick
        );

        if (is_auto_tick) {
            auto ticker = std::make_shared<Ticker>(
                api_strand, 
                std::chrono::milliseconds(args->tick_period),
                [&game](std::chrono::milliseconds delta) { 
                    game.Tick(delta); 
                }
            );
            ticker->Start();
        }

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler->operator()(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}