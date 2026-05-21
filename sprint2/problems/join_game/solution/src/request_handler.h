#pragma once
#include "http_server.h"
#include "model.h"
#include "game_session.h"
#include "logger.h"

#include <boost/json.hpp>
#include <boost/json/serialize.hpp>

#include <optional>
#include <cctype>
#include <algorithm>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

namespace endpoints {
    constexpr std::string_view MAPS = "/api/v1/maps";
    constexpr std::string_view MAPS_WITHOUT_SLASH = "api/v1/maps";
    constexpr std::string_view MAPS_PREFIX = "/api/v1/maps/";
    constexpr std::string_view MAPS_PREFIX_WITHOUT_SLASH = "api/v1/maps/";
    constexpr std::string_view GAME_JOIN = "/api/v1/game/join";
    constexpr std::string_view GAME_JOIN_WITHOUT_SLASH = "api/v1/game/join";
    constexpr std::string_view GAME_PLAYERS = "/api/v1/game/players";
    constexpr std::string_view GAME_PLAYERS_WITHOUT_SLASH = "api/v1/game/players";
}  // namespace endpoints

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game}
        , game_session_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        HandleRequest(std::move(req), std::forward<Send>(send));
    }

private:
    template <typename Body, typename Allocator>
    std::optional<std::string> GetHeaderValue(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        std::string_view header_name) {

        std::string target(header_name);
        std::transform(target.begin(), target.end(), target.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        for (auto it = req.begin(); it != req.end(); ++it) {
            std::string name = std::string(it->name_string());
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (name == target) {
                std::string value = std::string(it->value());
                size_t start = value.find_first_not_of(" \t\n\r");
                if (start == std::string::npos) {
                    return std::nullopt;
                }
                size_t end = value.find_last_not_of(" \t\n\r");
                return value.substr(start, end - start + 1);
            }
        }
        return std::nullopt;
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                       Send&& send) {

        std::string target = std::string(req.target());
        std::string method = std::string(req.method_string());

        auto query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            target = target.substr(0, query_pos);
        }

        // JOIN endpoint
        if (target == endpoints::GAME_JOIN || target == endpoints::GAME_JOIN_WITHOUT_SLASH) {
            HandleJoin(std::move(req), std::forward<Send>(send));
            return;
        }

        // PLAYERS endpoint
        if (target == endpoints::GAME_PLAYERS || target == endpoints::GAME_PLAYERS_WITHOUT_SLASH) {
            HandleGetPlayers(std::move(req), std::forward<Send>(send));
            return;
        }

        // MAPS endpoints
        if (target == endpoints::MAPS || target == endpoints::MAPS_WITHOUT_SLASH) {
            if (method != "GET") {
                auto response = MakeErrorResponse(
                    std::move(req),
                    http::status::method_not_allowed,
                    "badRequest",
                    "Method not allowed");
                send(std::move(response));
                return;
            }
            std::string body = SerializeMaps();
            auto response = MakeResponse(
                std::move(req),
                http::status::ok,
                "application/json",
                body);
            send(std::move(response));
            return;
        }

        // Single map endpoint
        if (target.find(endpoints::MAPS_PREFIX) == 0) {
            if (method != "GET") {
                auto response = MakeErrorResponse(
                    std::move(req),
                    http::status::method_not_allowed,
                    "badRequest",
                    "Method not allowed");
                send(std::move(response));
                return;
            }
            std::string map_id_str = target.substr(endpoints::MAPS_PREFIX.length());
            ProcessMapRequest(std::move(req), std::move(map_id_str), std::forward<Send>(send));
            return;
        }
        
        if (target.find(endpoints::MAPS_PREFIX_WITHOUT_SLASH) == 0) {
            if (method != "GET") {
                auto response = MakeErrorResponse(
                    std::move(req),
                    http::status::method_not_allowed,
                    "badRequest",
                    "Method not allowed");
                send(std::move(response));
                return;
            }
            std::string map_id_str = target.substr(endpoints::MAPS_PREFIX_WITHOUT_SLASH.length());
            ProcessMapRequest(std::move(req), std::move(map_id_str), std::forward<Send>(send));
            return;
        }

        // Not found
        auto response = MakeErrorResponse(
            std::move(req),
            http::status::not_found,
            "badRequest",
            "Not found");
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void ProcessMapRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                           std::string map_id_str,
                           Send&& send) {

        while (!map_id_str.empty() && map_id_str.back() == '/') {
            map_id_str.pop_back();
        }

        auto query_pos = map_id_str.find('?');
        if (query_pos != std::string::npos) {
            map_id_str = map_id_str.substr(0, query_pos);
        }

        model::Map::Id map_id{std::move(map_id_str)};
        const model::Map* map = game_.FindMap(map_id);

        if (!map) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::not_found,
                "mapNotFound",
                "Map not found");
            send(std::move(response));
            return;
        }

        std::string body = SerializeMap(*map);
        auto response = MakeResponse(
            std::move(req),
            http::status::ok,
            "application/json",
            body);
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {

        if (req.method() != http::verb::post) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::method_not_allowed,
                "invalidMethod",
                "Only POST method is expected");
            response.set(http::field::allow, "POST");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }

        boost::json::value json;
        try {
            json = boost::json::parse(req.body());
        }
        catch (...) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::bad_request,
                "invalidArgument",
                "Join game request parse error");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }

        if (!json.is_object()) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::bad_request,
                "invalidArgument",
                "Join game request parse error");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }

        auto& obj = json.as_object();

        if (!obj.contains("userName") || !obj.at("userName").is_string()) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::bad_request,
                "invalidArgument",
                "Invalid name");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }

        std::string user_name = std::string(obj.at("userName").as_string());
        if (user_name.empty()) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::bad_request,
                "invalidArgument",
                "Invalid name");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }

        if (!obj.contains("mapId") || !obj.at("mapId").is_string()) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::bad_request,
                "invalidArgument",
                "Invalid map ID");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }

        model::Map::Id map_id{std::string(obj.at("mapId").as_string())};

        try {
            auto result = game_session_.JoinGame(user_name, map_id);
            
            logger::LogDebug("JoinGame: generated token: " + *result.token + " for player " + std::to_string(*result.player_id));
            
            boost::json::object response_body;
            response_body["authToken"] = *result.token;
            response_body["playerId"] = *result.player_id;

            auto response = MakeResponse(
                std::move(req),
                http::status::ok,
                "application/json",
                boost::json::serialize(response_body));
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
        }
        catch (const std::runtime_error& e) {
            if (std::string(e.what()) == "Map not found") {
                auto response = MakeErrorResponse(
                    std::move(req),
                    http::status::not_found,
                    "mapNotFound",
                    "Map not found");
                response.set(http::field::cache_control, "no-cache");
                send(std::move(response));
            }
            else {
                throw;
            }
        }
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleGetPlayers(http::request<Body, http::basic_fields<Allocator>>&& req,
                          Send&& send) {

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::method_not_allowed,
                "invalidMethod",
                "Invalid method");
            response.set(http::field::allow, "GET, HEAD");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }

        auto token = ExtractToken(req);

        if (!token) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::unauthorized,
                "invalidToken",
                "Authorization header is missing");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }

        logger::LogDebug("HandleGetPlayers: extracted token: " + **token);
        logger::LogDebug("ValidateToken result: " + std::to_string(game_session_.ValidateToken(*token)));

        if (!game_session_.ValidateToken(*token)) {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::unauthorized,
                "unknownToken",
                "Player token has not been found");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }

        try {
            auto players = game_session_.GetPlayersOnMap(*token);

            if (req.method() == http::verb::head) {
                auto response = MakeResponse(
                    std::move(req),
                    http::status::ok,
                    "application/json",
                    "");
                response.set(http::field::cache_control, "no-cache");
                send(std::move(response));
                return;
            }

            boost::json::object response_body;
            for (const auto& [id, name] : players) {
                boost::json::object player_obj;
                player_obj["name"] = name;
                response_body[id] = player_obj;
            }

            auto response = MakeResponse(
                std::move(req),
                http::status::ok,
                "application/json",
                boost::json::serialize(response_body));
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
        }
        catch (const std::exception& e) {
            logger::LogDebug("Exception in GetPlayersOnMap: " + std::string(e.what()));
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::unauthorized,
                "unknownToken",
                "Player not found");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
        }
    }

    template <typename Body, typename Allocator>
    std::optional<model::Token> ExtractToken(
        const http::request<Body, http::basic_fields<Allocator>>& req) {

        auto auth_value_opt = GetHeaderValue(req, "authorization");
        if (!auth_value_opt) {
            logger::LogDebug("ExtractToken: No Authorization header found");
            return std::nullopt;
        }

        std::string auth_value = *auth_value_opt;
        logger::LogDebug("ExtractToken: Raw auth value: '" + auth_value + "'");

        const std::string bearer_prefix = "Bearer ";
        
        // Проверяем длину и префикс без учета регистра
        if (auth_value.length() < bearer_prefix.length()) {
            logger::LogDebug("ExtractToken: Auth value too short");
            return std::nullopt;
        }
        
        // Берем префикс нужной длины и приводим к нижнему регистру для сравнения
        std::string auth_prefix = auth_value.substr(0, bearer_prefix.length());
        std::transform(auth_prefix.begin(), auth_prefix.end(), auth_prefix.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        
        if (auth_prefix != "bearer ") {
            logger::LogDebug("ExtractToken: Invalid auth scheme: '" + auth_prefix + "'");
            return std::nullopt;
        }
        
        // Извлекаем токен (все что после "Bearer ")
        std::string token_str = auth_value.substr(bearer_prefix.length());
        
        // Удаляем пробелы в начале и конце
        size_t start = token_str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            logger::LogDebug("ExtractToken: Token string is empty after trimming");
            return std::nullopt;
        }
        
        size_t end = token_str.find_last_not_of(" \t\n\r");
        token_str = token_str.substr(start, end - start + 1);

        if (token_str.empty()) {
            logger::LogDebug("ExtractToken: Final token string is empty");
            return std::nullopt;
        }

        logger::LogDebug("ExtractToken: Extracted token: '" + token_str + "'");
        return model::Token{std::move(token_str)};
    }

    template <typename Body, typename Allocator>
    static http::response<http::string_body> MakeResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        http::status status,
        std::string_view content_type,
        std::string_view body) {

        http::response<http::string_body> response(status, req.version());
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        return response;
    }

    template <typename Body, typename Allocator>
    static http::response<http::string_body> MakeErrorResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        http::status status,
        std::string_view code,
        std::string_view message) {

        std::string body = boost::json::serialize(
            boost::json::object{
                {"code", code},
                {"message", message}
            });

        auto response = MakeResponse(
            std::move(req),
            status,
            "application/json",
            body);
        response.set(http::field::cache_control, "no-cache");
        return response;
    }

    std::string SerializeMaps() const;
    std::string SerializeMap(const model::Map& map) const;
    boost::json::array SerializeRoads(const model::Map& map) const;
    boost::json::array SerializeBuildings(const model::Map& map) const;
    boost::json::array SerializeOffices(const model::Map& map) const;

    model::Game& game_;
    game::GameSession game_session_;
};

}  // namespace http_handler