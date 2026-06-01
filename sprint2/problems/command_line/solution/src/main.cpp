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

    int exit_code = EXIT_SUCCESS;
    std::string exception_msg;

    try {
        auto args_opt = ParseCommandLine(argc, argv);

        if (!args_opt) {
            return EXIT_SUCCESS;
        }

        const Args args = *args_opt;

        model::Game game = json_loader::LoadGame(args.config_file);

        const unsigned num_threads = std::thread::hardware_concurrency();

        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code&, int) {
            ioc.stop();
        });

        http_handler::RequestHandler handler{
            game,
            args.www_root
        };

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        logger::LogServerStarted(address.to_string(), port);

        http_server::ServeHttp(
            ioc,
            {address, port},
            [&handler](auto&& req, auto&& send) {
                handler(
                    std::forward<decltype(req)>(req),
                    std::forward<decltype(send)>(send)
                );
            });

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

    } catch (const std::exception& ex) {
        exit_code = EXIT_FAILURE;
        exception_msg = ex.what();
    }

    logger::LogServerExited(exit_code, exception_msg);

    return exit_code;
}