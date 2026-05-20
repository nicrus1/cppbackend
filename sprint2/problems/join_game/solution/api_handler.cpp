#include "api_handler.h"
#include <regex>
#include "json.hpp"

using json = nlohmann::json;

ApiHandler::ApiHandler(GameSession& session, PlayerTokens& tokens)
    : gameSession_(session), playerTokens_(tokens) {}

std::string ApiHandler::jsonError(const std::string& code, const std::string& message) {
    json j = {{"code", code}, {"message", message}};
    return j.dump();
}

std::string ApiHandler::extractToken(const std::string& authHeader) {
    std::regex re(R"(Bearer\s+([a-f0-9]{32}))", std::regex::icase);
    std::smatch match;
    if (std::regex_match(authHeader, match, re) && match.size() == 2) {
        return match[1];
    }
    return "";
}

std::string ApiHandler::handleJoinPost(const std::string& body) {
    try {
        json j = json::parse(body);
        if (!j.contains("userName") || !j.contains("mapId")) {
            return jsonError("invalidArgument", "Missing userName or mapId");
        }

        std::string userName = j["userName"];
        std::string mapId = j["mapId"];

        if (userName.empty()) {
            return jsonError("invalidArgument", "Invalid name");
        }

        if (!gameSession_.isMapValid(mapId)) {
            return jsonError("mapNotFound", "Map not found");
        }

        // Создаём пса (упрощённо: dogId = время)
        int dogId = static_cast<int>(time(nullptr));
        int playerId = gameSession_.addPlayer(userName, mapId, dogId);
        
        Player* player = gameSession_.getPlayer(playerId);
        if (!player) {
            return jsonError("internalError", "Failed to create player");
        }

        Token token = playerTokens_.generateToken(player);

        json response;
        response["authToken"] = token;
        response["playerId"] = playerId;
        return response.dump();

    } catch (json::parse_error&) {
        return jsonError("invalidArgument", "Join game request parse error");
    }
}

std::string ApiHandler::handlePlayersGet(const std::string& authHeader) {
    if (authHeader.empty()) {
        return jsonError("invalidToken", "Authorization header is missing");
    }

    Token token = extractToken(authHeader);
    if (token.empty()) {
        return jsonError("invalidToken", "Invalid Authorization header format");
    }

    Player* currentPlayer = playerTokens_.findPlayerByToken(token);
    if (!currentPlayer) {
        return jsonError("unknownToken", "Player token has not been found");
    }

    const std::string& currentMapId = currentPlayer->getMapId();
    json playersJson;

    for (const auto& [id, player] : gameSession_.getAllPlayers()) {
        if (player->getMapId() == currentMapId) {
            playersJson[std::to_string(id)] = {{"name", player->getName()}};
        }
    }

    return playersJson.dump();
}