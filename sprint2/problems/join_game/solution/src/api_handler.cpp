#include "api_handler.h"
#include "request_handler.h" // для SerializeMaps, SerializeMap

namespace http_handler {

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleJoin(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    if (req.method() != http::verb::post) {
        SendError(std::move(req), send, http::status::method_not_allowed, 
                  "invalidMethod", "Only POST method is expected");
        return;
    }

    boost::json::value json;
    try {
        json = boost::json::parse(req.body());
    }
    catch (...) {
        SendError(std::move(req), send, http::status::bad_request,
                  "invalidArgument", "Join game request parse error");
        return;
    }

    if (!json.is_object()) {
        SendError(std::move(req), send, http::status::bad_request,
                  "invalidArgument", "Join game request parse error");
        return;
    }

    auto& obj = json.as_object();

    if (!obj.contains("userName") || !obj.at("userName").is_string()) {
        SendError(std::move(req), send, http::status::bad_request,
                  "invalidArgument", "Invalid name");
        return;
    }

    std::string user_name = std::string(obj.at("userName").as_string());
    if (user_name.empty()) {
        SendError(std::move(req), send, http::status::bad_request,
                  "invalidArgument", "Invalid name");
        return;
    }

    if (!obj.contains("mapId") || !obj.at("mapId").is_string()) {
        SendError(std::move(req), send, http::status::bad_request,
                  "invalidArgument", "Invalid map ID");
        return;
    }

    model::Map::Id map_id{std::string(obj.at("mapId").as_string())};

    try {
        auto result = game_state_.JoinGame(user_name, map_id);
        
        boost::json::object response_body;
        response_body["authToken"] = *result.token;
        response_body["playerId"] = *result.player_id;

        SendResponse(std::move(req), send, http::status::ok,
                     "application/json", boost::json::serialize(response_body));
    }
    catch (const std::runtime_error& e) {
        if (std::string(e.what()) == "Map not found") {
            SendError(std::move(req), send, http::status::not_found,
                      "mapNotFound", "Map not found");
        } else {
            throw;
        }
    }
}

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleGetPlayers(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        SendError(std::move(req), send, http::status::method_not_allowed,
                  "invalidMethod", "Invalid method");
        return;
    }

    auto token = ExtractToken(req);
    
    if (!token) {
        SendError(std::move(req), send, http::status::unauthorized,
                  "invalidToken", "Authorization header is missing");
        return;
    }

    if (!game_state_.ValidateToken(*token)) {
        SendError(std::move(req), send, http::status::unauthorized,
                  "unknownToken", "Player token has not been found");
        return;
    }

    try {
        auto players = game_state_.GetPlayersOnMap(*token);

        if (req.method() == http::verb::head) {
            SendResponse(std::move(req), send, http::status::ok,
                         "application/json", "");
            return;
        }

        boost::json::object response_body;
        for (const auto& [id, name] : players) {
            boost::json::object player_obj;
            player_obj["name"] = name;
            response_body[id] = player_obj;
        }

        SendResponse(std::move(req), send, http::status::ok,
                     "application/json", boost::json::serialize(response_body));
    }
    catch (const std::exception& e) {
        SendError(std::move(req), send, http::status::unauthorized,
                  "unknownToken", "Player not found");
    }
}

// Остальные методы требуют доступа к RequestHandler::SerializeMaps и SerializeMap
// Поэтому их лучше оставить в RequestHandler или вынести в отдельный утилитный класс

} // namespace http_handler