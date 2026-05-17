#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <memory>
#include <chrono>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"

using namespace std::literals;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    // Инициализируем логгер
    logger::InitLogging();
    
    if (argc != 2) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    
    int exit_code = EXIT_SUCCESS;
    std::string exception_msg;
    
    try {
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(argv[1]);
        
        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        
        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code&, int) {
            // Логирование остановки сервера будет выполнено после выхода из ioc.run()
            ioc.stop();
        });
        
        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        http_handler::RequestHandler handler{game};
        
        // 5. Создаём обёртку с логированием для обработчика
        http_handler::LoggingSessionHandler<http_handler::RequestHandler> logging_handler{handler};
        
        // 6. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;
        
        // Логируем запуск сервера
        logger::LogServerStarted(address.to_string(), port);
        
        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
            // Создаём временный объект для обработки запроса с логированием
            // В реальном коде нужно передать IP клиента
            logging_handler.HandleRequest(std::forward<decltype(req)>(req));
            // Отправка будет выполнена внутри HandleRequest через callback
        });
        
        // Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        exit_code = EXIT_FAILURE;
        exception_msg = ex.what();
    }
    
    // Логируем остановку сервера
    logger::LogServerExited(exit_code, exception_msg);
    
    return exit_code;
}