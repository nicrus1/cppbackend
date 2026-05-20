#include "game_session.h"

int GameSession::addPlayer(const std::string& name, const std::string& mapId, int dogId) {
    int id = nextId_++;
    players_[id] = std::make_unique<Player>(id, name, mapId, dogId);
    mapToPlayers_[mapId].push_back(id);
    return id;
}

Player* GameSession::getPlayer(int id) {
    auto it = players_.find(id);
    return it != players_.end() ? it->second.get() : nullptr;
}

bool GameSession::isMapValid(const std::string& mapId) const {
    // Будет заполнено из config.json
    return mapId == "map1"; // временно
}