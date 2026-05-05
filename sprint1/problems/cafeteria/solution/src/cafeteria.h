#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <atomic>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        auto op = std::make_shared<OrderOperation>();
        op->cafeteria = this;
        op->handler = std::move(handler);
        op->sausage = store_.GetSausage();
        op->bread = store_.GetBread();
        op->completed = std::make_shared<std::atomic<int>>(0);
        
        // Начинаем приготовление сосиски
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
        
        // Начинаем выпекание булки
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
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};