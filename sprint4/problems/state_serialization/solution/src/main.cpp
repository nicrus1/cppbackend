#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <optional>
#include <chrono>

// Предполагается, что эти заголовки уже есть в вашем проекте
#include "model.h"
#include "model_serialization.h"
#include "state_manager.h" 

namespace net = boost::asio;
namespace sys = boost::system;
namespace po = boost::program_options;

struct Args {
    std::string config_file;
    std::string www_root;
    std::optional<std::string> state_file;
    std::optional<int> save_state_period;
    std::optional<int> tick_period; // Период автоматического тика, если есть
};

// Функция разбора аргументов командной строки
Args ParseCommandLine(int argc, const char* argv[]) {
    po::options_description desc{"Allowed options"};
    desc.add_options()
        ("help,h", "produce help message")
        ("config-file,c", po::value<std::string>()->required(), "path to config file")
        ("www-root,w", po::value<std::string>()->required(), "path to www root")
        ("state-file", po::value<std::string>(), "path to game state save file")
        ("save-state-period", po::value<int>(), "state saving period in milliseconds")
        ("tick-period,t", po::value<int>(), "tick period in milliseconds");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    Args args;
    args.config_file = vm["config-file"].as<std::string>();
    args.www_root = vm["www-root"].as<std::string>();

    if (vm.count("state-file")) {
        args.state_file = vm["state-file"].as<std::string>();
    }
    if (vm.count("save-state-period")) {
        args.save_state_period = vm["save-state-period"].as<int>();
    }
    if (vm.count("tick-period")) {
        args.tick_period = vm["tick-period"].as<int>();
    }

    return args;
}

// Запуск многопоточного выполнения io_context
template <typename Fn>
void RunWorkers(unsigned num_threads, const Fn& fn) {
    std::vector<std::thread> v;
    v.reserve(num_threads - 1);
    for (auto i = num_threads - 1; i > 0; --i) {
        v.emplace_back(fn);
    }
    fn();
    for (auto& t : v) {
        t.join();
    }
}

int main(int argc, const char* argv[]) {
    try {
        // 1. Разбираем аргументы командной строки
        Args args;
        try {
            args = ParseCommandLine(argc, argv);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing command line: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        // 2. Инициализируем прикладной слой (Application) и модель игры
        // Здесь загружается ваш JSON-конфиг карт и настраивается окружение
        auto config = model::LoadConfig(args.config_file); 
        app::Application app(std::move(config));

        std::shared_ptr<infrastructure::StateManager> state_manager;

        // 3. Настройка инфраструктуры сохранения состояния, если передан --state-file
        if (args.state_file.has_value()) {
            std::optional<std::chrono::milliseconds> period;
            if (args.save_state_period.has_value()) {
                period = std::chrono::milliseconds(*args.save_state_period);
            }

            state_manager = std::make_shared<infrastructure::StateManager>(
                *args.state_file, period, app
            );

            // Восстановление состояния из файла при старте
            try {
                state_manager->Load();
                BOOST_LOG_TRIVIAL(info) << "Game state successfully initialized.";
            } catch (const std::exception& ex) {
                // ТЗ: При ошибке восстановления вывести в log и вернуть EXIT_FAILURE
                BOOST_LOG_TRIVIAL(error) << "Critical error restoring state: " << ex.what();
                return EXIT_FAILURE;
            }

            // Регистрируем лямбду в Application, которая будет дергать менеджер при каждом тике времени
            app.DoOnTick([state_manager](std::chrono::milliseconds delta) {
                state_manager->OnTick(delta);
            });
        }

        // 4. Инициализация асинхронного контекста Asio
        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        net::io_context ioc(num_threads);

        // 5. Подписка на системные сигналы SIGINT и SIGTERM для штатного завершения
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                BOOST_LOG_TRIVIAL(info) << "Shutdown signal received. Stopping server...";
                ioc.stop(); // Останавливаем цикл обработки событий Asio
            }
        });

        // 6. Инициализация и запуск сетевой подсистемы (HTTP API и статика)
        // Здесь должен быть ваш код создания HttpServer / API ручек, например:
        // auto handler = std::make_shared<http_handler::RequestHandler>(app, args.www_root);
        // http_server::ServeHttp(ioc, {address, port}, handler);

        BOOST_LOG_TRIVIAL(info) << "Game server started. Running workers...";

        // 7. Передаем управление воркерам (блокирующий вызов)
        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

        // --- ТОЧКА ШТАТНОГО ВЫХОДА ИЗ ЦИКЛА СОБЫТИЙ ---
        // Сюда мы попадаем, когда все асинхронные потоки завершили работу после ioc.stop()
        if (state_manager) {
            try {
                state_manager->Save();
                BOOST_LOG_TRIVIAL(info) << "Game state successfully saved on shutdown.";
            } catch (const std::exception& ex) {
                BOOST_LOG_TRIVIAL(error) << "Failed to save state during shutdown: " << ex.what();
                return EXIT_FAILURE;
            }
        }

    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Uncaught exception in main: " << ex.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}