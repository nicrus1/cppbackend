#pragma once

#include "game_state.h"

#include <boost/json.hpp>
#include <boost/beast/http.hpp>

#include <string>
#include <optional>

namespace http_handler {

namespace json = boost::json;
namespace http = boost::beast::http;

class ApiHandler {
public:
    explicit ApiHandler(game::GameState& game_state)
        : game_state_(game_state) {
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {

        json::value body = json::parse(req.body());

        auto& obj = body.as_object();

        if (!obj.contains("userName") || !obj.contains("mapId")) {

            http::response<http::string_body> response{
                http::status::bad_request,
                req.version()
            };

            response.set(http::field::content_type,
                         "application/json");

            response.body() =
                R"({"code":"invalidArgument","message":"Invalid request"})";

            response.prepare_payload();

            return send(std::move(response));
        }

        std::string user_name =
            obj["userName"].as_string().c_str();

        std::string map_id_str =
            obj["mapId"].as_string().c_str();

        model::Map::Id map_id{map_id_str};

        auto result =
            game_state_.JoinGame(user_name, map_id);

        json::object response_body;

        response_body["authToken"] = result.token;
        response_body["playerId"] = result.player_id;

        http::response<http::string_body> response{
            http::status::ok,
            req.version()
        };

        response.set(http::field::content_type,
                     "application/json");

        response.body() = json::serialize(response_body);

        response.prepare_payload();

        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleGetPlayers(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send) {

        auto token_opt = ExtractToken(req);

        if (!token_opt) {

            http::response<http::string_body> response{
                http::status::unauthorized,
                req.version()
            };

            response.set(http::field::content_type,
                         "application/json");

            response.body() =
                R"({"code":"invalidToken","message":"Authorization header is missing"})";

            response.prepare_payload();

            return send(std::move(response));
        }

        const app::Token& token = token_opt.value();

        if (!game_state_.ValidateToken(token)) {

            http::response<http::string_body> response{
                http::status::unauthorized,
                req.version()
            };

            response.set(http::field::content_type,
                         "application/json");

            response.body() =
                R"({"code":"unknownToken","message":"Player token has not been found"})";

            response.prepare_payload();

            return send(std::move(response));
        }

        auto players =
            game_state_.GetPlayersOnMap(token);

        json::object players_json;

        for (const auto& [id, name] : players) {

            json::object player_obj;

            player_obj["name"] = name;

            players_json[id] = player_obj;
        }

        http::response<http::string_body> response{
            http::status::ok,
            req.version()
        };

        response.set(http::field::content_type,
                     "application/json");

        response.body() = json::serialize(players_json);

        response.prepare_payload();

        send(std::move(response));
    }

private:
template <typename Body, typename Allocator, typename Send>
void HandleGameState(
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send) {

    auto token_opt = ExtractToken(req);

    if (!token_opt) {

        http::response<http::string_body> response{
            http::status::unauthorized,
            req.version()
        };

        response.set(http::field::content_type,
                     "application/json");

        response.body() =
            R"({"code":"invalidToken","message":"Authorization header is missing"})";

        response.prepare_payload();

        return send(std::move(response));
    }

    const app::Token& token = token_opt.value();

    if (!game_state_.ValidateToken(token)) {

        http::response<http::string_body> response{
            http::status::unauthorized,
            req.version()
        };

        response.set(http::field::content_type,
                     "application/json");

        response.body() =
            R"({"code":"unknownToken","message":"Player token has not been found"})";

        response.prepare_payload();

        return send(std::move(response));
    }

    auto players =
        game_state_.GetGameState(token);

    json::object players_json;

    for (const auto& [id, player] : players) {

        json::object player_obj;

        json::array pos;
        pos.push_back(0);
        pos.push_back(0);

        json::array speed;
        speed.push_back(0);
        speed.push_back(0);

        player_obj["pos"] = pos;
        player_obj["speed"] = speed;
        player_obj["dir"] = "U";

        players_json[id] = player_obj;
    }

    json::object result;
    result["players"] = players_json;

    http::response<http::string_body> response{
        http::status::ok,
        req.version()
    };

    response.set(http::field::content_type,
                 "application/json");

    response.body() = json::serialize(result);

    response.prepare_payload();

    send(std::move(response));
}
    template <typename Body, typename Allocator>
    std::optional<app::Token> ExtractToken(
        const http::request<Body,
        http::basic_fields<Allocator>>& req) {

        if (!req.count(http::field::authorization)) {
            return std::nullopt;
        }

        std::string auth =
    std::string(req[http::field::authorization]);

        const std::string bearer = "Bearer ";

        if (auth.substr(0, bearer.size()) != bearer) {
            return std::nullopt;
        }

        return auth.substr(bearer.size());
    }

    game::GameState& game_state_;
};

}  // namespace http_handler