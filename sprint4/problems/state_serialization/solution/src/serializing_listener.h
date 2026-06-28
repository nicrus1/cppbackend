#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>

#include "app_listener.h"
#include "model_serialization.h"

namespace infrastructure {

class SerializingListener : public app::ApplicationListener {
public:
    SerializingListener(
        std::shared_ptr<model::Game> game,
        std::string state_file,
        std::chrono::milliseconds save_period = std::chrono::milliseconds(0)
    ) : game_(game)
      , state_file_(std::move(state_file))
      , save_period_(save_period)
      , last_save_time_(std::chrono::milliseconds(0))
      , is_shutting_down_(false) {
        
        if (state_file_.empty()) {
            throw std::runtime_error("State file path cannot be empty");
        }
    }

    void OnTick(app::milliseconds delta) override {
        if (is_shutting_down_) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        
        // Если период сохранения не задан (0), не сохраняем автоматически
        if (save_period_.count() == 0) {
            return;
        }
        
        elapsed_time_ += delta;
        
        // Проверяем, не прошло ли достаточно времени
        if (elapsed_time_ >= save_period_) {
            SaveState();
            elapsed_time_ = std::chrono::milliseconds(0);
        }
    }

    void OnShutdown() override {
        is_shutting_down_ = true;
        std::lock_guard<std::mutex> lock(mutex_);
        SaveState();
    }

private:
    void SaveState() {
        if (!game_) {
            return;
        }
        
        try {
            serialization::GameState state;
            
            // Сохраняем состояние игры
            // Здесь нужно заполнить state из game_
            // Это зависит от вашей реализации Game
            
            if (!serialization::StateSerializer::SaveToFile(state, state_file_)) {
                // Логируем ошибку
            }
        } catch (const std::exception& e) {
            // Логируем ошибку
        }
    }

    std::shared_ptr<model::Game> game_;
    std::string state_file_;
    std::chrono::milliseconds save_period_;
    std::chrono::milliseconds elapsed_time_{0};
    std::chrono::milliseconds last_save_time_;
    std::atomic<bool> is_shutting_down_;
    std::mutex mutex_;
};

}  // namespace infrastructure