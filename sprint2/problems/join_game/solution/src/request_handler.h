#pragma once
#include "http_server.h"
#include "model.h"
#include "game_session.h"

#include <boost/json.hpp>
#include <boost/json/serialize.hpp>

#include <optional>
#include <cctype>
#include <algorithm>
#include <iostream>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

namespace endpoints {
    constexpr std::string_view MAPS = "/api/v1/maps";
    constexpr std::string_view MAPS_WITHOUT_SLASH = "api/v1/maps";
    constexpr std::string_view MAPS_PREFIX = "/api/v1/maps/";
    constexpr std::string_view MAPS_PREFIX_WITHOUT_SLASH = "api/v1/maps/";
    constexpr std::string_view API_PREFIX = "/api/";
    constexpr std::string_view API_PREFIX_WITHOUT_SLASH = "api/";
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
    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                       Send&& send) {

        std::string target = std::string(req.target());
        std::string method = std::string(req.method_string());

        auto query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            target = target.substr(0, query_pos);
        }

        // Обработка JOIN
        if (target == endpoints::GAME_JOIN ||
            target == endpoints::GAME_JOIN_WITHOUT_SLASH) {
            HandleJoin(std::move(req), std::forward<Send>(send));
            return;
        }

        // Обработка GET PLAYERS
        if (target == endpoints::GAME_PLAYERS ||
            target == endpoints::GAME_PLAYERS_WITHOUT_SLASH) {
            HandleGetPlayers(std::move(req), std::forward<Send>(send));
            return;
        }

        // Обработка GET MAPS
        if (method != "GET") {
            auto response = MakeErrorResponse(
                std::move(req),
                http::status::method_not_allowed,
                "badRequest",
                "Method not allowed");
            send(std::move(response));
            return;
        }

        if (target == endpoints::MAPS ||
            target == endpoints::MAPS_WITHOUT_SLASH) {
            std::string body = SerializeMaps();
            auto response = MakeResponse(
                std::move(req),
                http::status::ok,
                "application/json",
                body);
            send(std::move(response));
            return;
        }

        if (target.find(endpoints::MAPS_PREFIX) == 0 ||
            target.find(endpoints::MAPS_PREFIX_WITHOUT_SLASH) == 0) {
            std::string map_id_str;
            if (target.find(endpoints::MAPS_PREFIX) == 0) {
                map_id_str = target.substr(endpoints::MAPS_PREFIX.length());
            } else {
                map_id_str = target.substr(endpoints::MAPS_PREFIX_WITHOUT_SLASH.length());
            }
            ProcessMapRequest(std::move(req), std::move(map_id_str), std::forward<Send>(send));
            return;
        }

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
            std::string error_msg = e.what();

            if (error_msg == "Map not found") {
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

        if (req.method() != http::verb::get &&
            req.method() != http::verb::head) {

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
                "Authorization header is missing or invalid");

            response.set(http::field::cache_control, "no-cache");

            send(std::move(response));
            return;
        }

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
            response_body[id] = boost::json::object{
                {"name", name}
            };
        }

        auto response = MakeResponse(
            std::move(req),
            http::status::ok,
            "application/json",
            boost::json::serialize(response_body));

        response.set(http::field::cache_control, "no-cache");

        send(std::move(response));
    }

    template <typename Body, typename Allocator>
    std::optional<model::Token> ExtractToken(
        const http::request<Body, http::basic_fields<Allocator>>& req) {

        auto auth_it = req.find(http::field::authorization);

        if (auth_it == req.end()) {
            return std::nullopt;
        }

        std::string auth = std::string(auth_it->value());

        const std::string bearer = "Bearer ";

        if (auth.size() < bearer.size() ||
            auth.substr(0, bearer.size()) != bearer) {
            return std::nullopt;
        }

        std::string token = auth.substr(bearer.size());

        if (token.empty()) {
            return std::nullopt;
        }

        return model::Token{token};
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