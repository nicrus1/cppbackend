#include "model.h"
#include <stdexcept>

namespace model {

void Game::AddMap(Map map) {
    maps_.emplace_back(std::move(map));
}

const std::vector<Map>& Game::GetMaps() const noexcept {
    return maps_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    for (const auto& map : maps_) {
        if (map.GetId() == id) {
            return &map;
        }
    }
    return nullptr;
}

// РЕАЛИЗАЦИЯ ОБНОВЛЕНИЯ ВРЕМЕНИ
void Game::Tick(std::chrono::milliseconds delta) {
    // В этом методе тебе нужно пройтись по всем игровым сессиям 
    // и вызвать у них метод обновления позиций собак.
    // 
    // Примерно так:
    // for (auto& session : sessions_) {
    //     session->Tick(delta);
    // }
}

} // namespace model