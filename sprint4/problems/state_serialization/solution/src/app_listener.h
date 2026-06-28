#pragma once

#include <chrono>

namespace app {

using milliseconds = std::chrono::milliseconds;

// Интерфейс для слушателей событий приложения
class ApplicationListener {
public:
    virtual ~ApplicationListener() = default;
    
    // Вызывается при каждом тике игровых часов
    virtual void OnTick(milliseconds delta) = 0;
    
    // Вызывается перед завершением приложения
    virtual void OnShutdown() = 0;
};

}  // namespace app