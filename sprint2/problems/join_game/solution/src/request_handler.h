#pragma once

#include "http_server.h"
#include "model.h"
#include "json_loader.h"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <string>
#include <string_view>
#include <unordered_map>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using StringResponse = http::response<http::string_body>;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game,
                            game::GameSession& session)
        : game_{game}
        , session_{session} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {
        HandleRequest(std::move(req), std::forward<Send>(send));
    }

private:
    model::Game& game_;
    game::GameSession& session_;

    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send) {

        const auto target = req.target();

        if (target == "/api/v1/maps") {
            HandleMaps(std::move(req), std::forward<Send>(send));
            return;
        }

        if (target.starts_with("/api/v1/maps/")) {
            HandleMap(std::move(req), std::forward<Send>(send));
            return;
        }

        if (target == "/api/v1/game/join") {
            HandleJoin(std::move(req), std::forward<Send>(send));
            return;
        }

        if (target == "/api/v1/game/players") {
            HandlePlayers(std::move(req), std::forward<Send>(send));
            return;
        }

        send(MakeErrorResponse(
            std::move(req),
            http::status::bad_request,
            "badRequest",
            "Bad request"));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleMaps(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send) {

        if (req.method() != http::verb::get &&
            req.method() != http::verb::head) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::method_not_allowed,
                "invalidMethod",
                "Only GET method is expected"));
            return;
        }

        json::array maps_json;

        for (const auto& map : game_.GetMaps()) {
            json::object obj;
            obj["id"] = *map.GetId();
            obj["name"] = map.GetName();
            maps_json.push_back(obj);
        }

        StringResponse response{http::status::ok, req.version()};
        response.set(http::field::content_type, "application/json");

        if (req.method() != http::verb::head) {
            response.body() = json::serialize(maps_json);
        }

        response.prepare_payload();
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleMap(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send) {

        if (req.method() != http::verb::get &&
            req.method() != http::verb::head) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::method_not_allowed,
                "invalidMethod",
                "Only GET method is expected"));
            return;
        }

        std::string path(req.target());

        const std::string prefix = "/api/v1/maps/";
        std::string map_id = path.substr(prefix.size());

        const auto* map = game_.FindMap(model::Map::Id(map_id));

        if (!map) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::not_found,
                "mapNotFound",
                "Map not found"));
            return;
        }

        json::object obj;
        obj["id"] = *map->GetId();
        obj["name"] = map->GetName();

        StringResponse response{http::status::ok, req.version()};
        response.set(http::field::content_type, "application/json");

        if (req.method() != http::verb::head) {
            response.body() = json::serialize(obj);
        }

        response.prepare_payload();
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send) {

        if (req.method() != http::verb::post) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::method_not_allowed,
                "invalidMethod",
                "Only POST method is expected"));
            return;
        }

        json::value parsed;

        try {
            parsed = json::parse(req.body());
        } catch (...) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::bad_request,
                "invalidArgument",
                "Join game request parse error"));
            return;
        }

        auto obj = parsed.as_object();

        if (!obj.contains("userName") || !obj.contains("mapId")) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::bad_request,
                "invalidArgument",
                "Invalid join game request"));
            return;
        }

        std::string user_name =
            json::value_to<std::string>(obj["userName"]);

        std::string map_id =
            json::value_to<std::string>(obj["mapId"]);

        if (user_name.empty()) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::bad_request,
                "invalidArgument",
                "Invalid name"));
            return;
        }

        const auto* map = game_.FindMap(model::Map::Id(map_id));

        if (!map) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::not_found,
                "mapNotFound",
                "Map not found"));
            return;
        }

        auto token = session_.AddPlayer(user_name);

        json::object response_json;
        response_json["authToken"] = token;
        response_json["playerId"] = 0;

        StringResponse response{http::status::ok, req.version()};
        response.set(http::field::content_type, "application/json");
        response.body() = json::serialize(response_json);
        response.prepare_payload();

        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandlePlayers(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send) {

        if (req.method() != http::verb::get &&
            req.method() != http::verb::head) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::method_not_allowed,
                "invalidMethod",
                "Invalid method"));
            return;
        }

        if (!req.has_header(http::field::authorization)) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::unauthorized,
                "invalidToken",
                "Authorization header is missing"));
            return;
        }

        std::string auth =
            std::string(req[http::field::authorization]);

        const std::string bearer = "Bearer ";

        if (!auth.starts_with(bearer)) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::unauthorized,
                "invalidToken",
                "Authorization header is invalid"));
            return;
        }

        std::string token = auth.substr(bearer.size());

        if (!session_.HasPlayer(token)) {
            send(MakeErrorResponse(
                std::move(req),
                http::status::unauthorized,
                "unknownToken",
                "Player token has not been found"));
            return;
        }

        json::object players;

        StringResponse response{http::status::ok, req.version()};
        response.set(http::field::content_type, "application/json");

        if (req.method() != http::verb::head) {
            response.body() = json::serialize(players);
        }

        response.prepare_payload();

        send(std::move(response));
    }

    template <typename Body, typename Allocator>
    static StringResponse MakeErrorResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        http::status status,
        std::string_view code,
        std::string_view message) {

        json::object body;
        body["code"] = std::string(code);
        body["message"] = std::string(message);

        StringResponse response{status, req.version()};
        response.set(http::field::content_type, "application/json");

        response.body() = json::serialize(body);
        response.prepare_payload();

        return response;
    }
};

}  // namespace http_handler