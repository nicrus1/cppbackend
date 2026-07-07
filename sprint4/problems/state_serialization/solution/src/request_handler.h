#pragma once

#include "http_server.h"
#include "model.h"
#include "api_handler.h"
#include "logger.h"
#include <boost/json.hpp>
#include <optional>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, const std::string& static_dir, bool manual_tick_allowed)
        : game_{game}
        , api_handler_{game}
        , static_dir_(static_dir)
        , manual_tick_allowed_{manual_tick_allowed} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    void Tick(std::chrono::milliseconds delta) {
        api_handler_.Tick(delta);
    }
    
    void SetLootGeneratorConfig(double period, double probability) {
        api_handler_.SetLootGeneratorConfig(period, probability);
    }
    
    void SetDogRetirementTime(double seconds) {
        api_handler_.SetDogRetirementTime(seconds);
    }
    
    void LoadExtraData(const std::filesystem::path& config_path) {
        // Упрощенная реализация
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());
        std::string method = std::string(req.method_string());

        auto query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            target = target.substr(0, query_pos);
        }

        if (target.find("/api/v") == 0 && target.find("/api/v1/") != 0) {
            SendError(std::move(req), send, http::status::bad_request,
                      "badRequest", "Invalid API version");
            return;
        }

        if (target == "/api/v1/game/join") {
            api_handler_.HandleJoin(std::move(req), std::forward<Send>(send));
            return;
        }

        if (target == "/api/v1/game/players") {
            api_handler_.HandleGetPlayers(std::move(req), std::forward<Send>(send));
            return;
        }

        if (target == "/api/v1/game/state") {
            api_handler_.HandleGameState(std::move(req), std::forward<Send>(send));
            return;
        }
        
        if (target == "/api/v1/game/player/action") {
            api_handler_.HandlePlayerAction(std::move(req), std::forward<Send>(send));
            return;
        }

        if (target == "/api/v1/game/tick") {
            if (!manual_tick_allowed_) {
                SendError(std::move(req), send, http::status::bad_request,
                          "badRequest", "Invalid endpoint");
                return;
            }
            api_handler_.HandleTick(std::move(req), std::forward<Send>(send));
            return;
        }

        if (target == "/api/v1/game/records") {
            api_handler_.HandleRecords(std::move(req), std::forward<Send>(send));
            return;
        }

        if (target == "/api/v1/maps") {
            HandleMaps(std::move(req), std::forward<Send>(send));
            return;
        }

        const std::string prefix = "/api/v1/maps/";
        if (target.find(prefix) == 0) {
            std::string map_id = target.substr(prefix.length());
            while (!map_id.empty() && map_id.back() == '/') {
                map_id.pop_back();
            }
            HandleMap(std::move(req), std::forward<Send>(send), map_id);
            return;
        }

        HandleStaticFile(std::move(req), std::forward<Send>(send));
    }

private:
    template <typename Body, typename Allocator, typename Send>
    void HandleMaps(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            SendErrorWithAllow(std::move(req), send, http::status::method_not_allowed,
                               "invalidMethod", "Method not allowed", "GET, HEAD");
            return;
        }
        
        if (req.method() == http::verb::head) {
            SendResponse(std::move(req), send, http::status::ok, "application/json", "");
            return;
        }
        
        boost::json::array maps_array;
        for (const auto& map_pair : game_.GetMaps()) {
            const auto& map_state = map_pair.second;
            boost::json::object map_obj;
            map_obj["id"] = map_state.map_id;
            map_obj["name"] = map_state.name;
            map_obj["dogSpeed"] = map_state.dog_speed;
            maps_array.push_back(map_obj);
        }
        
        SendResponse(std::move(req), send, http::status::ok, "application/json", 
                     boost::json::serialize(maps_array));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleMap(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send,
                   const std::string& map_id) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            SendErrorWithAllow(std::move(req), send, http::status::method_not_allowed,
                               "invalidMethod", "Method not allowed", "GET, HEAD");
            return;
        }

        auto map_state = game_.GetMapState(map_id);
        if (!map_state) {
            SendError(std::move(req), send, http::status::not_found,
                      "mapNotFound", "Map not found");
            return;
        }

        if (req.method() == http::verb::head) {
            SendResponse(std::move(req), send, http::status::ok, "application/json", "");
            return;
        }

        boost::json::object map_obj;
        map_obj["id"] = map_state->map_id;
        map_obj["name"] = map_state->name;
        
        // Дороги
        boost::json::array roads_array;
        for (const auto& road : map_state->roads) {
            boost::json::object road_obj;
            road_obj["x0"] = road.x0;
            road_obj["y0"] = road.y0;
            if (road.has_x1) road_obj["x1"] = road.x1;
            if (road.has_y1) road_obj["y1"] = road.y1;
            roads_array.push_back(road_obj);
        }
        map_obj["roads"] = roads_array;
        
        // Здания
        boost::json::array buildings_array;
        for (const auto& building : map_state->buildings) {
            boost::json::object building_obj;
            building_obj["x"] = building.x;
            building_obj["y"] = building.y;
            building_obj["w"] = building.w;
            building_obj["h"] = building.h;
            buildings_array.push_back(building_obj);
        }
        map_obj["buildings"] = buildings_array;
        
        // Офисы
        boost::json::array offices_array;
        for (const auto& office : map_state->offices) {
            boost::json::object office_obj;
            office_obj["id"] = office.id;
            office_obj["x"] = office.x;
            office_obj["y"] = office.y;
            office_obj["offsetX"] = office.offsetX;
            office_obj["offsetY"] = office.offsetY;
            offices_array.push_back(office_obj);
        }
        map_obj["offices"] = offices_array;
        
        // Типы лута
        boost::json::array loot_array;
        for (const auto& loot : map_state->loot_types) {
            boost::json::object loot_obj;
            loot_obj["name"] = loot.name;
            loot_obj["file"] = loot.file;
            loot_obj["type"] = loot.type;
            loot_obj["rotation"] = loot.rotation;
            loot_obj["color"] = loot.color;
            loot_obj["scale"] = loot.scale;
            loot_obj["value"] = loot.value;
            loot_array.push_back(loot_obj);
        }
        map_obj["lootTypes"] = loot_array;

        SendResponse(std::move(req), send, http::status::ok, "application/json",
                     boost::json::serialize(map_obj));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleStaticFile(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::get) {
            SendError(std::move(req), send, http::status::method_not_allowed,
                      "badRequest", "Method not allowed");
            return;
        }
        
        std::string path = std::string(req.target());
        if (!path.empty() && path[0] == '/') {
            path = path.substr(1);
        }
        if (path.empty()) {
            path = "index.html";
        }
        
        if (path.find("..") != std::string::npos) {
            SendStaticNotFound(std::move(req), send);
            return;
        }
        
        std::filesystem::path full_path = std::filesystem::path(static_dir_) / path;
        
        std::ifstream file(full_path, std::ios::binary);
        if (!file.is_open()) {
            SendStaticNotFound(std::move(req), send);
            return;
        }
        
        std::string content_type = "text/plain";
        std::string ext = full_path.extension().string();
        if (ext == ".html") content_type = "text/html";
        else if (ext == ".css") content_type = "text/css";
        else if (ext == ".js") content_type = "application/javascript";
        else if (ext == ".svg") content_type = "image/svg+xml";
        else if (ext == ".png") content_type = "image/png";
        else if (ext == ".jpg" || ext == ".jpeg") content_type = "image/jpeg";
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string body = buffer.str();
        
        http::response<http::string_body> response(http::status::ok, req.version());
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void SendStaticNotFound(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        http::response<http::string_body> response(http::status::not_found, req.version());
        response.set(http::field::content_type, "text/plain");
        response.body() = "404 Not Found";
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void SendResponse(http::request<Body, http::basic_fields<Allocator>>&& req,
                      Send&& send,
                      http::status status,
                      const std::string& content_type,
                      const std::string& body) {
        http::response<http::string_body> response(status, req.version());
        response.set(http::field::content_type, content_type);
        response.set(http::field::cache_control, "no-cache");
        response.body() = body;
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void SendError(http::request<Body, http::basic_fields<Allocator>>&& req,
                   Send&& send,
                   http::status status,
                   const std::string& code,
                   const std::string& message) {
        std::string body = boost::json::serialize(
            boost::json::object{
                {"code", code},
                {"message", message}
            });
        SendResponse(std::move(req), send, status, "application/json", body);
    }

    template <typename Body, typename Allocator, typename Send>
    void SendErrorWithAllow(http::request<Body, http::basic_fields<Allocator>>&& req,
                            Send&& send,
                            http::status status,
                            const std::string& code,
                            const std::string& message,
                            const std::string& allow_methods) {
        std::string body = boost::json::serialize(
            boost::json::object{
                {"code", code},
                {"message", message}
            });
        
        http::response<http::string_body> response(status, req.version());
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.set(http::field::allow, allow_methods);
        response.body() = body;
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

    model::Game& game_;
    ApiHandler api_handler_;
    std::string static_dir_;
    bool manual_tick_allowed_;
};

} // namespace http_handler