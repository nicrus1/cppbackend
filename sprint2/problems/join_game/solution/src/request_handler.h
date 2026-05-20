#pragma once
#include "http_server.h"
#include "model.h"
#include "game_session.h"
#include <boost/json.hpp>
#include <boost/json/serialize.hpp>
#include <optional>
#include <cctype>
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
    // Вспомогательная функция для регистронезависимого поиска заголовка
    template <typename Body, typename Allocator>
    std::optional<std::string> GetHeaderValue(const http::request<Body, http::basic_fields<Allocator>>& req, 
                                                std::string_view header_name) {
        for (auto it = req.begin(); it != req.end(); ++it) {
            std::string name = std::string(it->name_string());
            // Приводим к нижнему регистру для сравнения
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::string target(header_name);
            std::transform(target.begin(), target.end(), target.begin(), ::tolower);
            if (name == target) {
                return std::string(it->value());
            }
        }
        return std::nullopt;
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());
        std::string method = std::string(req.method_string());

        auto query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            target = target.substr(0, query_pos);
        }

        // Обработка JOIN запроса (с ведущим слешем и без)
        if (target == endpoints::GAME_JOIN || target == endpoints::GAME_JOIN_WITHOUT_SLASH) {
            HandleJoin(std::move(req), std::forward<Send>(send));
            return;
        }

        // Обработка GET/HEAD /players запроса (с ведущим слешем и без)
        if (target == endpoints::GAME_PLAYERS || target == endpoints::GAME_PLAYERS_WITHOUT_SLASH) {
            HandleGetPlayers(std::move(req), std::forward<Send>(send));
            return;
        }

        // GET запросы к картам
        if (method != "GET") {
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::method_not_allowed,
                                             "badRequest",
                                             "Method not allowed");
            send(std::move(response));
            return;
        }

        // API endpoints для карт
        if (target == endpoints::MAPS || target == endpoints::MAPS_WITHOUT_SLASH) {
            std::string body = SerializeMaps();

            auto response = MakeResponse(std::move(req),
                                         http::status::ok,
                                         "application/json",
                                         body);

            send(std::move(response));
            return;
        }

        if (target.find(endpoints::MAPS_PREFIX) == 0) {
            std::string map_id_str = target.substr(endpoints::MAPS_PREFIX.length());

            ProcessMapRequest(std::move(req),
                              std::move(map_id_str),
                              std::forward<Send>(send));
            return;
        }
        else if (target.find(endpoints::MAPS_PREFIX_WITHOUT_SLASH) == 0) {
            std::string map_id_str = target.substr(endpoints::MAPS_PREFIX_WITHOUT_SLASH.length());

            ProcessMapRequest(std::move(req),
                              std::move(map_id_str),
                              std::forward<Send>(send));
            return;
        }

        // Wrong API version/prefix
        if (target.find("/api/") == 0 ||
            target.find("api/") == 0) {

            // Поддерживается только /api/v1/...
            if (target.find("/api/v1/") != 0 &&
                target.find("api/v1/") != 0 &&
                target != "/api/v1/maps" &&
                target != "api/v1/maps") {

                auto response = MakeErrorResponse(std::move(req),
                                                 http::status::bad_request,
                                                 "badRequest",
                                                 "Bad request");

                send(std::move(response));
                return;
            }

            // API endpoint not found
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::not_found,
                                             "badRequest",
                                             "Not found");

            send(std::move(response));
            return;
        }

        // Static files for tests
        if (target == "/" || target == "/index.html") {
            auto response = MakeResponse(std::move(req),
                                         http::status::ok,
                                         "text/html",
                                         "");

            send(std::move(response));
            return;
        }

        // Images handling
        if (target.find("/images/") == 0) {
            std::string filename = target.substr(8);

            if (filename == "cube.svg") {
                auto response = MakeResponse(std::move(req),
                                             http::status::ok,
                                             "image/svg+xml",
                                             "");

                send(std::move(response));
                return;
            }
            else {
                auto response = MakeErrorResponse(std::move(req),
                                                 http::status::not_found,
                                                 "fileNotFound",
                                                 "File not found");

                response.set(http::field::content_type, "text/plain");

                send(std::move(response));
                return;
            }
        }

        // Non-API 404 - text/plain
        auto response = MakeErrorResponse(std::move(req),
                                         http::status::not_found,
                                         "badRequest",
                                         "Not found");

        response.set(http::field::content_type, "text/plain");

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
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::not_found,
                                             "mapNotFound",
                                             "Map not found");

            send(std::move(response));
            return;
        }

        std::string body = SerializeMap(*map);

        auto response = MakeResponse(std::move(req),
                                     http::status::ok,
                                     "application/json",
                                     body);

        send(std::move(response));
    }

    // Обработка JOIN запроса
    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Проверка метода
        if (req.method() != http::verb::post) {
            auto response = MakeErrorResponse(std::move(req), 
                                             http::status::method_not_allowed,
                                             "invalidMethod",
                                             "Only POST method is expected");
            response.set(http::field::allow, "POST");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }
        
        // Парсинг JSON
        boost::json::value json;
        try {
            json = boost::json::parse(req.body());
        } catch (...) {
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::bad_request,
                                             "invalidArgument",
                                             "Join game request parse error");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }
        
        // Проверка, что это объект
        if (!json.is_object()) {
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::bad_request,
                                             "invalidArgument",
                                             "Join game request parse error");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }
        
        auto& obj = json.as_object();
        
        // Проверка userName
        if (!obj.contains("userName") || !obj.at("userName").is_string()) {
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::bad_request,
                                             "invalidArgument",
                                             "Invalid name");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }
        
        std::string user_name = std::string(obj.at("userName").as_string());
        if (user_name.empty()) {
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::bad_request,
                                             "invalidArgument",
                                             "Invalid name");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }
        
        // Проверка mapId
        if (!obj.contains("mapId") || !obj.at("mapId").is_string()) {
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::bad_request,
                                             "invalidArgument",
                                             "Invalid map ID");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }
        
        model::Map::Id map_id{std::string(obj.at("mapId").as_string())};
        
        // Попытка входа в игру
        try {
            auto result = game_session_.JoinGame(user_name, map_id);
            
            boost::json::object response_body;
            response_body["authToken"] = *result.token;
            response_body["playerId"] = *result.player_id;
            
            auto response = MakeResponse(std::move(req),
                                         http::status::ok,
                                         "application/json",
                                         boost::json::serialize(response_body));
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            
        } catch (const std::runtime_error& e) {
            std::string error_msg = e.what();
            if (error_msg == "Map not found") {
                auto response = MakeErrorResponse(std::move(req),
                                                 http::status::not_found,
                                                 "mapNotFound",
                                                 "Map not found");
                response.set(http::field::cache_control, "no-cache");
                send(std::move(response));
            } else {
                throw;
            }
        }
    }

    // Обработка GET/HEAD /players запроса
    template <typename Body, typename Allocator, typename Send>
    void HandleGetPlayers(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Проверка метода (GET или HEAD)
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::method_not_allowed,
                                             "invalidMethod",
                                             "Invalid method");
            response.set(http::field::allow, "GET, HEAD");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }
        
        // Извлечение токена из заголовка Authorization (регистронезависимо)
        auto token = ExtractToken(req);
        if (!token) {
            std::cerr << "DEBUG: Token extraction failed" << std::endl;
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::unauthorized,
                                             "invalidToken",
                                             "Authorization header is missing or invalid");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }
        
        std::cerr << "DEBUG: Extracted token: " << **token << std::endl;
        
        // Валидация токена
        if (!game_session_.ValidateToken(*token)) {
            std::cerr << "DEBUG: Token validation failed" << std::endl;
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::unauthorized,
                                             "unknownToken",
                                             "Player token has not been found");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            return;
        }
        
        std::cerr << "DEBUG: Token validated successfully" << std::endl;
        
        // Получение списка игроков
        try {
            auto players = game_session_.GetPlayersOnMap(*token);
            
            std::cerr << "DEBUG: Got " << players.size() << " players" << std::endl;
            
            // Для HEAD запроса не нужно тело
            if (req.method() == http::verb::head) {
                auto response = MakeResponse(std::move(req),
                                             http::status::ok,
                                             "application/json",
                                             "");
                response.set(http::field::cache_control, "no-cache");
                send(std::move(response));
                return;
            }
            
            // Для GET запроса возвращаем тело
            boost::json::object response_body;
            for (const auto& [id, name] : players) {
                response_body[id] = boost::json::object{{"name", name}};
            }
            
            auto response = MakeResponse(std::move(req),
                                         http::status::ok,
                                         "application/json",
                                         boost::json::serialize(response_body));
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
            
        } catch (const std::runtime_error& e) {
            std::cerr << "DEBUG: Exception in GetPlayersOnMap: " << e.what() << std::endl;
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::unauthorized,
                                             "unknownToken",
                                             "Player not found");
            response.set(http::field::cache_control, "no-cache");
            send(std::move(response));
        }
    }

    // Извлечение токена из заголовка Authorization (регистронезависимо)
    template <typename Body, typename Allocator>
    std::optional<model::Token> ExtractToken(const http::request<Body, http::basic_fields<Allocator>>& req) {
        // Ищем заголовок Authorization (регистронезависимо)
        auto auth_value_opt = GetHeaderValue(req, "authorization");
        if (!auth_value_opt) {
            std::cerr << "DEBUG: No Authorization header found" << std::endl;
            return std::nullopt;
        }
        
        std::string auth_value = *auth_value_opt;
        std::cerr << "DEBUG: Authorization header value: '" << auth_value << "'" << std::endl;
        
        const std::string prefix = "Bearer ";
        
        // Проверка наличия префикса Bearer
        if (auth_value.length() <= prefix.length() || 
            auth_value.substr(0, prefix.length()) != prefix) {
            std::cerr << "DEBUG: No Bearer prefix found" << std::endl;
            return std::nullopt;
        }
        
        // Извлекаем токен после "Bearer "
        std::string token_str = auth_value.substr(prefix.length());
        
        // Удаляем пробелы в начале и конце
        size_t start = token_str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            std::cerr << "DEBUG: Token is empty after trimming" << std::endl;
            return std::nullopt;
        }
        size_t end = token_str.find_last_not_of(" \t\n\r");
        token_str = token_str.substr(start, end - start + 1);
        
        // Токен должен быть непустым
        if (token_str.empty()) {
            std::cerr << "DEBUG: Token is empty" << std::endl;
            return std::nullopt;
        }
        
        std::cerr << "DEBUG: Extracted token string: '" << token_str << "'" << std::endl;
        
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
        response.set(http::field::content_length, std::to_string(body.size()));

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

        std::string body = boost::json::serialize(boost::json::object{
            {"code", code},
            {"message", message}
        });

        auto response = MakeResponse(std::move(req),
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