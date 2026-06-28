#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>
#include "model.h"
#include "app_listener.h"

namespace model {

class Game {
public:
    using ListenerPtr = std::shared_ptr<app::ApplicationListener>;
    
    void AddListener(ListenerPtr listener) {
        listeners_.push_back(listener);
    }
    
    void Tick(app::milliseconds delta) {
        // Обновляем состояние игры
        // ...
        
        // Уведомляем слушателей
        for (auto& listener : listeners_) {
            listener->OnTick(delta);
        }
    }
    
    void Shutdown() {
        for (auto& listener : listeners_) {
            listener->OnShutdown();
        }
    }
    
    // Методы для работы с собаками, предметами и т.д.
    void AddDog(std::shared_ptr<Dog> dog) {
        dogs_.push_back(dog);
    }
    
    const std::vector<std::shared_ptr<Dog>>& GetDogs() const {
        return dogs_;
    }
    
    // Методы для сохранения и восстановления состояния
    void SaveState(serialization::GameState& state) const {
        // Реализация сохранения
    }
    
    void RestoreState(const serialization::GameState& state) {
        // Реализация восстановления
    }

private:
    std::vector<std::shared_ptr<Dog>> dogs_;
    std::vector<ListenerPtr> listeners_;
    // Другие поля игры
};

}  // namespace model