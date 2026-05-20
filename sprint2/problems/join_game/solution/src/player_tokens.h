#pragma once
#include "player.h"
#include <random>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace model {

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
    PlayerTokens() {
        std::random_device rd;
        generator_ = std::mt19937_64(rd());
    }
    
    Token GenerateToken(const Player& player) {
        uint64_t value = generator_();
        
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << value;
        
        Token token(ss.str());
        
        token_to_player_[token] = player.GetId();
        player_to_token_[player.GetId()] = token;
        
        return token;
    }
    
    PlayerId FindPlayerByToken(const Token& token) const {
        auto it = token_to_player_.find(token);
        if (it != token_to_player_.end()) {
            return it->second;
        }
        return PlayerId{0};
    }
    
    bool IsValidToken(const Token& token) const {
        return token_to_player_.find(token) != token_to_player_.end();
    }

private:
    std::mt19937_64 generator_;
    std::unordered_map<Token, PlayerId, util::TaggedHasher<Token>> token_to_player_;
    std::unordered_map<PlayerId, Token, util::TaggedHasher<PlayerId>> player_to_token_;
};

}  // namespace model