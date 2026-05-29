#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "app.h"
#include "logger.h"
#include "command_line.h"
#include "ticker.h"

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
        // 1. Парсинг параметров командной строки
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            return EXIT_SUCCESS;
        }

        // 2. Инициализация логгера
        logger::InitLogger();

        // 3. Загрузка карты
        model::Game game = json_loader::LoadGame(args->config_file);

        // 4. Инициализация io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        auto api_strand = net::make_strand(ioc);

        // 5. Инициализация фасада приложения
        // Обязательно добавь args->randomize_spawn_points в конструктор Application!
        app::Application app{game, args->randomize_spawn_points};

        // 6. Инициализация обработчика запросов
        bool is_auto_tick = args->tick_period > 0;
        auto handler = std::make_shared<http_handler::RequestHandler>(
            app, args->www_root, api_strand, is_auto_tick
        );

        // 7. Настройка таймера
        if (is_auto_tick) {
            auto ticker = std::make_shared<Ticker>(
                api_strand, 
                std::chrono::milliseconds(args->tick_period),
                [&app](std::chrono::milliseconds delta) { 
                    app.Tick(delta); 
                }
            );
            ticker->Start();
        }

        // 8. Запуск HTTP-сервера
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler->operator()(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // 9. Обработка сигналов
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        // 10. Запуск пула потоков
        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}