#pragma once
#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace model {

// Твои классы Map, Road, Building, Office, Dog, GameSession...

class Game {
public:
    // Базовые методы, которые у тебя уже должны быть
    void AddMap(Map map);
    const std::vector<Map>& GetMaps() const noexcept;
    const Map* FindMap(const Map::Id& id) const noexcept;

    // НОВЫЕ МЕТОДЫ ДЛЯ ЭТОГО СПРИНТА:
    
    // Включение случайного спавна
    void SetRandomizedSpawn(bool randomize) {
        randomize_spawn_points_ = randomize;
    }
    
    bool IsRandomizedSpawn() const {
        return randomize_spawn_points_;
    }

    // Метод обновления игрового времени
    void Tick(std::chrono::milliseconds delta);

private:
    std::vector<Map> maps_;
    bool randomize_spawn_points_ = false;
    // std::vector<std::shared_ptr<GameSession>> sessions_; // Твои сессии
};

} // namespace model