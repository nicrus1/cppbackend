#pragma once
#include <string>
#include <unordered_map>
#include <random>
#include "player.h"

using Token = std::string;

class PlayerTokens {
public:
    PlayerTokens();
    Token generateToken(Player* player);
    Player* findPlayerByToken(const Token& token);
    void removeToken(const Token& token);

private:
    std::random_device rd_;
    std::mt19937_64 gen1_;
    std::mt19937_64 gen2_;
    Token generateRandomToken();
    std::unordered_map<Token, Player*> tokenToPlayer_;
};