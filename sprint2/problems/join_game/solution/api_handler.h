#pragma once
#include <string>
#include "game_session.h"
#include "player_tokens.h"

class ApiHandler {
public:
    ApiHandler(GameSession& session, PlayerTokens& tokens);
    std::string handleJoinPost(const std::string& body);
    std::string handlePlayersGet(const std::string& authHeader);

private:
    GameSession& gameSession_;
    PlayerTokens& playerTokens_;
    std::string jsonError(const std::string& code, const std::string& message);
    std::string extractToken(const std::string& authHeader);
    void loadConfig(const std::string& configPath); // загрузка карт
};