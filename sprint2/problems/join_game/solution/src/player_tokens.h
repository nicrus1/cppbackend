#pragma once
#include "player.h"
#include <random>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iomanip>
#include "logger.h"

namespace model {

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
    PlayerTokens() 
        : generator1_(std::random_device{}())
        , generator2_(std::random_device{}()) {
    }
    
    Token GenerateToken(const Player& player) {
        uint64_t part1 = generator1_();
        uint64_t part2 = generator2_();
        
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        ss << std::setw(16) << part1;
        ss << std::setw(16) << part2;
        
        Token token(ss.str());
        
        token_to_player_[token] = player.GetId();
        player_to_token_[player.GetId()] = token;
        
        return token;
    }
    
    PlayerId FindPlayerByToken(const Token& token) const {
        logger::LogRawData("Looking for token: '" + *token + "'");
    logger::LogRawData("Token length: " + std::to_string((*token).length()));
    
    // Выводим все сохраненные токены
    logger::LogRawData("Stored tokens (" + std::to_string(token_to_player_.size()) + "):");
    for (const auto& [t, pid] : token_to_player_) {
        logger::LogRawData("  Token: '" + *t + "', Player ID: " + std::to_string(*pid));
    }
    
    auto it = token_to_player_.find(token);
    if (it != token_to_player_.end()) {
        logger::LogRawData("Token found!");
        return it->second;
    }
    logger::LogRawData("Token NOT found!");
    return PlayerId{0};
    }
    
    bool IsValidToken(const Token& token) const {
        return token_to_player_.find(token) != token_to_player_.end();
    }

private:
    std::mt19937_64 generator1_;
    std::mt19937_64 generator2_;
    std::unordered_map<Token, PlayerId, util::TaggedHasher<Token>> token_to_player_;
    std::unordered_map<PlayerId, Token, util::TaggedHasher<PlayerId>> player_to_token_;
};

}  // namespace model