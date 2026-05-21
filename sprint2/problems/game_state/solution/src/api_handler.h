#pragma once

#include "model.h"
#include "game_state.h"
#include "http_server.h"
#include "logger.h"

#include <boost/json.hpp>
#include <boost/beast/http.hpp>

#include <optional>
#include <string>
#include <functional>
#include <cctype>
#include <algorithm>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class ApiHandler {
public:
    explicit ApiHandler(game::GameState& game_state)
        : game_state_(game_state) {
    }

    // ========================= JOIN GAME =========================

    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {

        if (req.method() != http::verb::post) {

            SendErrorWithAllow(
                std::move(req),
                send,
                http::status::method_not_allowed,
                "invalidMethod",
                "Only POST method is expected",
                "POST"
            );

            return;
        }

        auto content_type = req.find(http::field::content_type);

        if (content_type == req.end() ||
            content_type->value() != "application/json") {

            SendError(
                std::move(req),
                send,
                http::status::bad_request,
                "invalidArgument",
                "Invalid Content-Type"
            );

            return;
        }

        boost::json::value json;

        try {
            json = boost::json::parse(req.body());
        }
        catch (...) {

            SendError(
                std::move(req),
                send,
                http::status::bad_request,
                "invalidArgument",
                "Join game request parse error"
            );

            return;
        }

        if (!json.is_object()) {

            SendError(
                std::move(req),
                send,
                http::status::bad_request,
                "invalidArgument",
                "Join game request parse error"
            );

            return;
        }

        auto& obj = json.as_object();

        if (!obj.contains("userName") ||
            !obj.at("userName").is_string()) {

            SendError(
                std::move(req),
                send,
                http::status::bad_request,
                "invalidArgument",
                "Invalid name"
            );

            return;
        }

        std::string user_name =
            std::string(obj.at("userName").as_string());

        if (user_name.empty()) {

            SendError(
                std::move(req),
                send,
                http::status::bad_request,
                "invalidArgument",
                "Invalid name"
            );

            return;
        }

        if (!obj.contains("mapId") ||
            !obj.at("mapId").is_string()) {

            SendError(
                std::move(req),
                send,
                http::status::bad_request,
                "invalidArgument",
                "Invalid map ID"
            );

            return;
        }

        model::Map::Id map_id{
            std::string(obj.at("mapId").as_string())
        };

        try {

            auto result =
                game_state_.JoinGame(user_name, map_id);

            boost::json::object response_body;

            response_body["authToken"] = *result.token;
            response_body["playerId"] = *result.player_id;

            SendResponseWithCache(
                std::move(req),
                send,
                http::status::ok,
                "application/json",
                boost::json::serialize(response_body)
            );
        }
        catch (const std::runtime_error& e) {

            if (std::string(e.what()) == "Map not found") {

                SendError(
                    std::move(req),
                    send,
                    http::status::not_found,
                    "mapNotFound",
                    "Map not found"
                );

            } else {
                throw;
            }
        }
    }

    // ========================= PLAYERS =========================

    template <typename Body, typename Allocator, typename Send>
    void HandleGetPlayers(http::request<Body, http::basic_fields<Allocator>>&& req,
                          Send&& send) {

        if (req.method() != http::verb::get &&
            req.method() != http::verb::head) {

            SendErrorWithAllow(
                std::move(req),
                send,
                http::status::method_not_allowed,
                "invalidMethod",
                "Invalid method",
                "GET, HEAD"
            );

            return;
        }

        auto token_opt = ExtractToken(req);

        if (!token_opt.has_value()) {

            SendError(
                std::move(req),
                send,
                http::status::unauthorized,
                "invalidToken",
                "Authorization header is missing"
            );

            return;
        }

        const model::Token& token = token_opt.value();

        if ((*token).empty()) {

            SendError(
                std::move(req),
                send,
                http::status::unauthorized,
                "invalidToken",
                "Authorization header is invalid"
            );

            return;
        }

        if (!game_state_.ValidateToken(token)) {

            SendError(
                std::move(req),
                send,
                http::status::unauthorized,
                "unknownToken",
                "Player token has not been found"
            );

            return;
        }

        try {

            auto players =
                game_state_.GetPlayersOnMap(token);

            if (req.method() == http::verb::head) {

                SendResponseWithCache(
                    std::move(req),
                    send,
                    http::status::ok,
                    "application/json",
                    ""
                );

                return;
            }

            boost::json::object response_body;

            for (const auto& [id, name] : players) {

                boost::json::object player_obj;
                player_obj["name"] = name;

                response_body[id] = player_obj;
            }

            SendResponseWithCache(
                std::move(req),
                send,
                http::status::ok,
                "application/json",
                boost::json::serialize(response_body)
            );
        }
        catch (...) {

            SendError(
                std::move(req),
                send,
                http::status::unauthorized,
                "unknownToken",
                "Player token has not been found"
            );
        }
    }

    // ========================= GAME STATE =========================

    template <typename Body, typename Allocator, typename Send>
    void HandleGameState(http::request<Body, http::basic_fields<Allocator>>&& req,
                         Send&& send) {

        if (req.method() != http::verb::get &&
            req.method() != http::verb::head) {

            SendErrorWithAllow(
                std::move(req),
                send,
                http::status::method_not_allowed,
                "invalidMethod",
                "Invalid method",
                "GET, HEAD"
            );

            return;
        }

        auto token_opt = ExtractToken(req);

        if (!token_opt.has_value()) {

            SendError(
                std::move(req),
                send,
                http::status::unauthorized,
                "invalidToken",
                "Authorization header is missing"
            );

            return;
        }

        const model::Token& token = token_opt.value();

        if ((*token).empty()) {

            SendError(
                std::move(req),
                send,
                http::status::unauthorized,
                "invalidToken",
                "Authorization header is invalid"
            );

            return;
        }

        if (!game_state_.ValidateToken(token)) {

            SendError(
                std::move(req),
                send,
                http::status::unauthorized,
                "unknownToken",
                "Player token has not been found"
            );

            return;
        }

        try {

            auto state =
                game_state_.GetGameState(token);

            if (req.method() == http::verb::head) {

                SendResponseWithCache(
                    std::move(req),
                    send,
                    http::status::ok,
                    "application/json",
                    ""
                );

                return;
            }

            SendResponseWithCache(
                std::move(req),
                send,
                http::status::ok,
                "application/json",
                boost::json::serialize(state)
            );
        }
        catch (...) {

            SendError(
                std::move(req),
                send,
                http::status::unauthorized,
                "unknownToken",
                "Player token has not been found"
            );
        }
    }

private:

    template <typename Body, typename Allocator>
    std::optional<model::Token> ExtractToken(
        const http::request<Body, http::basic_fields<Allocator>>& req) {

        auto it = req.find(http::field::authorization);

        if (it == req.end()) {
            return std::nullopt;
        }

        std::string auth_value =
            std::string(it->value());

        const std::string bearer_prefix = "Bearer ";

        if (auth_value.length() < bearer_prefix.length()) {
            return std::nullopt;
        }

        std::string auth_prefix =
            auth_value.substr(0, bearer_prefix.length());

        for (char& c : auth_prefix) {
            c = std::tolower(
                static_cast<unsigned char>(c)
            );
        }

        if (auth_prefix != "bearer ") {
            return std::nullopt;
        }

        std::string token_str =
            auth_value.substr(bearer_prefix.length());

        size_t start =
            token_str.find_first_not_of(" \t\n\r");

        if (start == std::string::npos) {
            return std::nullopt;
        }

        size_t end =
            token_str.find_last_not_of(" \t\n\r");

        token_str =
            token_str.substr(start, end - start + 1);

        if (token_str.empty()) {
            return std::nullopt;
        }

        return model::Token{std::move(token_str)};
    }

    template <typename Body, typename Allocator, typename Send>
    void SendResponseWithCache(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send,
        http::status status,
        std::string_view content_type,
        std::string_view body) {

        http::response<http::string_body> response(
            status,
            req.version()
        );

        response.set(
            http::field::content_type,
            content_type
        );

        response.set(
            http::field::cache_control,
            "no-cache"
        );

        response.body() = body;

        response.prepare_payload();

        response.keep_alive(req.keep_alive());

        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void SendError(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send,
        http::status status,
        std::string_view code,
        std::string_view message) {

        std::string body =
            boost::json::serialize(
                boost::json::object{
                    {"code", code},
                    {"message", message}
                }
            );

        http::response<http::string_body> response(
            status,
            req.version()
        );

        response.set(
            http::field::content_type,
            "application/json"
        );

        response.set(
            http::field::cache_control,
            "no-cache"
        );

        response.body() = body;

        response.prepare_payload();

        response.keep_alive(req.keep_alive());

        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void SendErrorWithAllow(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send,
        http::status status,
        std::string_view code,
        std::string_view message,
        std::string_view allow_methods) {

        std::string body =
            boost::json::serialize(
                boost::json::object{
                    {"code", code},
                    {"message", message}
                }
            );

        http::response<http::string_body> response(
            status,
            req.version()
        );

        response.set(
            http::field::content_type,
            "application/json"
        );

        response.set(
            http::field::cache_control,
            "no-cache"
        );

        response.set(
            http::field::allow,
            allow_methods
        );

        response.body() = body;

        response.prepare_payload();

        response.keep_alive(req.keep_alive());

        send(std::move(response));
    }

private:
    game::GameState& game_state_;
};

} // namespace http_handler