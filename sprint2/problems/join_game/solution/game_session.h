#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include "player.h"

class GameSession {
public:
    int addPlayer(const std::string& name, const std::string& mapId, int dogId);
    Player* getPlayer(int id);
    const std::unordered_map<int, std::unique_ptr<Player>>& getAllPlayers() const { return players_; }
    bool isMapValid(const std::string& mapId) const; // проверка по конфигу

private:
    int nextId_ = 0;
    std::unordered_map<int, std::unique_ptr<Player>> players_;
    std::unordered_map<std::string, std::vector<int>> mapToPlayers_;
};