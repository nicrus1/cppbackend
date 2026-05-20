#include "player_tokens.h"
#include <sstream>
#include <iomanip>

PlayerTokens::PlayerTokens() : gen1_(rd_()), gen2_(rd_()) {}

Token PlayerTokens::generateRandomToken() {
    uint64_t part1 = gen1_();
    uint64_t part2 = gen2_();
    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(16) << part1
       << std::setw(16) << part2;
    return ss.str();
}

Token PlayerTokens::generateToken(Player* player) {
    Token token = generateRandomToken();
    tokenToPlayer_[token] = player;
    return token;
}

Player* PlayerTokens::findPlayerByToken(const Token& token) {
    auto it = tokenToPlayer_.find(token);
    return it != tokenToPlayer_.end() ? it->second : nullptr;
}

void PlayerTokens::removeToken(const Token& token) {
    tokenToPlayer_.erase(token);
}