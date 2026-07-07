#pragma once

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <chrono>

#include "model_serialization.h"

namespace app {
// Предварительное объявление вашего класса приложения
class Application; 
}

namespace infrastructure {

class StateManager {
public:
    StateManager(std::filesystem::path state_file,
                 std::optional<std::chrono::milliseconds> save_period,
                 app::Application& app)
        : state_file_(std::move(state_file))
        , save_period_(save_period)
        , app_(app) {}

    // Выполняет загрузку состояния сервера при старте
    void Load() {
        if (!std::filesystem::exists(state_file_)) {
            // Файла нет — это нормальная ситуация, стартуем с чистого листа
            return; 
        }

        std::ifstream in(state_file_, std::ios::binary);
        if (!in.is_open()) {
            throw std::runtime_error("Failed to open state file for reading: " + state_file_.string());
        }

        serialization::GameStateRepr repr;
        boost::archive::text_iarchive ia(in);
        ia >> repr;

        RestoreGameState(repr);
    }

    // Выполняет атомарное сохранение состояния во временный файл, а затем переименовывает его
    void Save() {
        serialization::GameStateRepr repr = GatherGameState();

        std::filesystem::path tmp_file = state_file_.string() + ".tmp";
        {
            std::ofstream out(tmp_file, std::ios::binary);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to open temporary state file for writing: " + tmp_file.string());
            }
            boost::archive::text_oarchive oa(out);
            oa << repr;
        } // Поток out закрывается здесь, освобождая файл перед rename

        // Атомарное переименование
        std::filesystem::rename(tmp_file, state_file_);
    }

    // Слушатель хода игровых часов (OnTick)
    void OnTick(std::chrono::milliseconds delta) {
        if (!save_period_.has_value()) {
            return; // Автосохранение по периоду выключено
        }

        time_since_last_save_ += delta;
        if (time_since_last_save_ >= *save_period_) {
            Save();
            time_since_last_save_ = std::chrono::milliseconds{0};
        }
    }

private:
    // Вспомогательный метод сбора данных из вашего Application в DTO структуру
    serialization::GameStateRepr GatherGameState() {
        serialization::GameStateRepr repr;
        
        // Примерная реализация (замените на вызовы ваших реальных методов):
        /*
        for (const auto& session : app_.GetSessions()) {
            std::vector<serialization::DogRepr> dogs;
            for (const auto& dog : session.GetDogs()) {
                dogs.emplace_back(dog);
            }
            
            std::vector<serialization::LostObjectRepr> lost_objects;
            for (const auto& obj : session.GetLostObjects()) {
                lost_objects.emplace_back(obj.GetId(), obj.GetType(), obj.GetPosition());
            }
            
            repr.sessions_.emplace_back(session.GetMapId(), std::move(dogs), std::move(lost_objects));
        }

        for (const auto& [token, player] : app_.GetPlayerTokens().GetMap()) {
            repr.players_.emplace_back(*token, *player.GetDog().GetId(), player.GetSession().GetMapId());
        }
        */
        
        return repr;
    }

    // Вспомогательный метод распаковки данных обратно в сущности доменной модели
    void RestoreGameState(const serialization::GameStateRepr& repr) {
        // Примерная реализация:
        /*
        // 1. Сначала восстанавливаем сессии, собак и лут на картах
        for (const auto& session_repr : repr.sessions_) {
            auto& session = app_.OrCreateSession(session_repr.GetMapId());
            
            for (const auto& dog_repr : session_repr.GetDogs()) {
                session.AddDog(dog_repr.Restore());
            }
            
            for (const auto& obj_repr : session_repr.GetLostObjects()) {
                session.AddLostObject(model::LostObject{
                    model::FoundObject::Id{obj_repr.GetId()}, 
                    obj_repr.GetType(), 
                    obj_repr.GetPosition()
                });
            }
        }

        // 2. Затем привязываем игроков и их токены к уже восстановленным собакам
        for (const auto& player_repr : repr.players_) {
            auto& session = app_.GetSession(player_repr.GetMapId());
            auto dog_ptr = session.FindDog(player_repr.GetDogId());
            
            app_.RestorePlayer(player_repr.GetToken(), dog_ptr, session);
        }
        */
    }

    std::filesystem::path state_file_;
    std::optional<std::chrono::milliseconds> save_period_;
    app::Application& app_;
    std::chrono::milliseconds time_since_last_save_{0};
};

}  // namespace infrastructure