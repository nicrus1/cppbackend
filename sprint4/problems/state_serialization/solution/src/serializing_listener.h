#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <iostream>
#include <filesystem>

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
      , elapsed_time_(std::chrono::milliseconds(0))
      , is_shutting_down_(false) {
        
        if (state_file_.empty()) {
            std::cerr << "Warning: State file path is empty, state will not be saved" << std::endl;
        }
    }

    void OnTick(app::milliseconds delta) override {
        if (is_shutting_down_ || state_file_.empty()) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        
        if (save_period_.count() == 0) {
            return;
        }
        
        elapsed_time_ += delta;
        
        if (elapsed_time_ >= save_period_) {
            SaveState();
            elapsed_time_ = std::chrono::milliseconds(0);
        }
    }

    void OnShutdown() override {
        is_shutting_down_ = true;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!state_file_.empty()) {
            SaveState();
        }
    }

private:
    void SaveState() {
        if (!game_) {
            std::cerr << "Error: Game is null, cannot save state" << std::endl;
            return;
        }
        
        try {
            serialization::GameState state;
            game_->SaveState(state);
            
            // Сохраняем с атомарным переименованием
            if (serialization::StateSerializer::SaveToFile(state, state_file_)) {
                std::cout << "State saved to " << state_file_ << std::endl;
            } else {
                std::cerr << "Failed to save state to " << state_file_ << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception during state save: " << e.what() << std::endl;
        }
    }

    std::shared_ptr<model::Game> game_;
    std::string state_file_;
    std::chrono::milliseconds save_period_;
    std::chrono::milliseconds elapsed_time_;
    std::atomic<bool> is_shutting_down_;
    std::mutex mutex_;
};

}  // namespace infrastructure