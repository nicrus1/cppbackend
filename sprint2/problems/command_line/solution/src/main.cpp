#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"

namespace po = boost::program_options;
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

// Класс для периодического вызова обработчика
class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
        last_tick_ = Clock::now();
        net::dispatch(strand_, [self = shared_from_this()] {
            self->ScheduleTick();
        });
    }

private:
    void ScheduleTick() {
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](boost::system::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(boost::system::error_code ec) {
        using namespace std::chrono;
        
        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (...) {
                // Игнорируем исключения в обработчике
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

struct ProgramOptions {
    std::string config_file;
    std::string www_root = "/app/static";
    std::optional<std::chrono::milliseconds> tick_period;
    bool randomize_spawn_points = false;
    bool help = false;
};

ProgramOptions ParseCommandLine(int argc, const char* argv[]) {
    ProgramOptions opts;
    
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<int>(), "set tick period (milliseconds)")
        ("config-file,c", po::value<std::string>(), "set config file path")
        ("www-root,w", po::value<std::string>(), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions");
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    
    if (vm.count("help")) {
        opts.help = true;
        std::cout << desc << std::endl;
        return opts;
    }
    
    if (!vm.count("config-file")) {
        throw std::runtime_error("Config file is required");
    }
    opts.config_file = vm["config-file"].as<std::string>();
    
    if (vm.count("www-root")) {
        opts.www_root = vm["www-root"].as<std::string>();
    }
    
    if (vm.count("tick-period")) {
        opts.tick_period = std::chrono::milliseconds(vm["tick-period"].as<int>());
    }
    
    if (vm.count("randomize-spawn-points")) {
        opts.randomize_spawn_points = true;
    }
    
    return opts;
}

}  // namespace

int main(int argc, const char* argv[]) {
    logger::InitLogging();
    
    int exit_code = EXIT_SUCCESS;
    std::string exception_msg;
    
    try {
        ProgramOptions opts = ParseCommandLine(argc, argv);
        
        if (opts.help) {
            return EXIT_SUCCESS;
        }
        
        model::Game game = json_loader::LoadGame(opts.config_file);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // Создаём strand для синхронизации доступа к API
        auto api_strand = net::make_strand(ioc);
        
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code&, int) {
            ioc.stop();
        });

        // Создаём обработчик запросов с поддержкой случайных спавнов
        http_handler::RequestHandler handler{
            game, 
            opts.www_root,
            opts.randomize_spawn_points
        };

        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;
        
        // Если указан tick-period, запускаем таймер
        std::shared_ptr<Ticker> ticker;
        if (opts.tick_period.has_value()) {
            // В режиме реального времени отключаем ручной tick
            handler.DisableManualTick();
            
            ticker = std::make_shared<Ticker>(
                api_strand,
                *opts.tick_period,
                [&handler](std::chrono::milliseconds delta) {
                    handler.ProcessTick(delta);
                }
            );
            ticker->Start();
            logger::LogDebug("Auto tick enabled with period " + 
                           std::to_string(opts.tick_period->count()) + "ms");
        }
        
        logger::LogServerStarted(address.to_string(), port);
        
        http_server::ServeHttp(ioc, {address, port}, 
            [&handler, &api_strand](auto&& req, auto&& send) {
                // Запускаем обработку запроса в strand'е
                net::dispatch(api_strand, [&handler, req = std::move(req), send = std::move(send)]() mutable {
                    handler(std::move(req), std::move(send));
                });
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