#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <atomic>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io}
        , strand_{net::make_strand(io_)} {
    }

    void OrderHotDog(HotDogHandler handler) {
        // Используем strand для безопасного получения ингредиентов из Store
        net::dispatch(strand_, [this, handler = std::move(handler)]() mutable {
            auto op = std::make_shared<OrderOperation>();
            op->cafeteria = this;
            op->handler = std::move(handler);
            op->sausage = store_.GetSausage();
            op->bread = store_.GetBread();
            op->completed = std::make_shared<std::atomic<int>>(0);
            
            // Запускаем приготовление (эти операции могут выполняться параллельно)
            op->sausage->StartFry(*gas_cooker_, [op]() {
                op->sausage_timer = std::make_shared<net::steady_timer>(op->cafeteria->io_);
                op->sausage_timer->expires_after(Milliseconds{1500});
                op->sausage_timer->async_wait([op](const sys::error_code& ec) {
                    if (!ec) {
                        op->sausage->StopFry();
                    }
                    if (++(*op->completed) == 2) {
                        op->AssembleHotDog();
                    }
                });
            });
            
            op->bread->StartBake(*gas_cooker_, [op]() {
                op->bread_timer = std::make_shared<net::steady_timer>(op->cafeteria->io_);
                op->bread_timer->expires_after(Milliseconds{1000});
                op->bread_timer->async_wait([op](const sys::error_code& ec) {
                    if (!ec) {
                        op->bread->StopBaking();
                    }
                    if (++(*op->completed) == 2) {
                        op->AssembleHotDog();
                    }
                });
            });
        });
    }

private:
    struct OrderOperation : public std::enable_shared_from_this<OrderOperation> {
        Cafeteria* cafeteria = nullptr;
        HotDogHandler handler;
        std::shared_ptr<Sausage> sausage;
        std::shared_ptr<Bread> bread;
        std::shared_ptr<std::atomic<int>> completed;
        std::shared_ptr<net::steady_timer> sausage_timer;
        std::shared_ptr<net::steady_timer> bread_timer;
        
        void AssembleHotDog() {
            try {
                static std::atomic<int> next_id{0};
                int hotdog_id = next_id++;
                
                HotDog hotdog{hotdog_id, sausage, bread};
                Result<HotDog> result{std::move(hotdog)};
                handler(std::move(result));
            } catch (const std::exception& e) {
                Result<HotDog> result{std::make_exception_ptr(e)};
                handler(std::move(result));
            } catch (...) {
                Result<HotDog> result{std::make_exception_ptr(std::runtime_error("Unknown error"))};
                handler(std::move(result));
            }
        }
    };
    
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};