#pragma once
#include "http_server.h"
#include "model.h"
#include "api_handler.h"
#include "logger.h"
#include "extra_data.h"
#include "db/record_manager.h"
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
    
    void SetRecordManager(std::shared_ptr<db::RecordManager> manager) {
        record_manager_ = manager;
        api_handler_.SetRecordManager(manager);
    }
    
    void LoadExtraData(const std::filesystem::path& config_path);

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());
        std::string method = std::string(req.method_string());

        auto query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            target = target.substr(0, query_pos);
        }
        
        std::string path = target;
        if (!path.empty() && path[0] == '/') {
            path = path.substr(1);
        }

        if (target.starts_with("/api/v") && !target.starts_with("/api/v1/")) {
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
            if (method != "GET" && method != "HEAD") {
                SendErrorWithAllow(std::move(req), send, http::status::method_not_allowed,
                          "invalidMethod", "Method not allowed", "GET, HEAD");
                return;
            }
            if (method == "HEAD") {
                SendResponse(std::move(req), send, http::status::ok, "application/json", "");
                return;
            }
            std::string body = SerializeMaps();
            SendResponse(std::move(req), send, http::status::ok, "application/json", body);
            return;
        }

        const std::string prefix = "/api/v1/maps/";
        if (target.starts_with(prefix)) {
            if (method != "GET" && method != "HEAD") {
                SendErrorWithAllow(std::move(req), send, http::status::method_not_allowed,
                          "invalidMethod", "Method not allowed", "GET, HEAD");
                return;
            }
            std::string map_id_str = target.substr(prefix.length());
            
            while (!map_id_str.empty() && map_id_str.back() == '/') {
                map_id_str.pop_back();
            }

            model::Map::Id map_id{std::move(map_id_str)};
            const model::Map* map = game_.FindMap(map_id);

            if (!map) {
                SendError(std::move(req), send, http::status::not_found,
                          "mapNotFound", "Map not found");
                return;
            }

            if (method == "HEAD") {
                SendResponse(std::move(req), send, http::status::ok, "application/json", "");
                return;
            }

            std::string body = SerializeMap(*map);
            SendResponse(std::move(req), send, http::status::ok, "application/json", body);
            return;
        }

        HandleStaticFile(std::move(req), std::forward<Send>(send), path);
    }

private:
    template <typename Body, typename Allocator, typename Send>
    void HandleStaticFile(http::request<Body, http::basic_fields<Allocator>>&& req,
                          Send&& send,
                          const std::string& path) {
        if (req.method() != http::verb::get) {
            SendError(std::move(req), send, http::status::method_not_allowed,
                      "badRequest", "Method not allowed");
            return;
        }
        
        std::string file_path = path.empty() ? "index.html" : path;
        
        if (file_path.find("..") != std::string::npos) {
            SendStaticNotFound(std::move(req), send);
            return;
        }
        
        std::filesystem::path full_path = std::filesystem::path(static_dir_) / file_path;
        
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
    void SendStaticNotFound(http::request<Body, http::basic_fields<Allocator>>&& req,
                            Send&& send) {
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
                      std::string_view content_type,
                      std::string_view body) {
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
                   std::string_view code,
                   std::string_view message) {
        std::string body = boost::json::serialize(
            boost::json::object{
                {"code", code},
                {"message", message}
            });
        
        http::response<http::string_body> response(status, req.version());
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = body;
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void SendErrorWithAllow(http::request<Body, http::basic_fields<Allocator>>&& req,
                            Send&& send,
                            http::status status,
                            std::string_view code,
                            std::string_view message,
                            std::string_view allow_methods) {
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

    std::string SerializeMaps() const;
    std::string SerializeMap(const model::Map& map) const;
    boost::json::array SerializeRoads(const model::Map& map) const;
    boost::json::array SerializeBuildings(const model::Map& map) const;
    boost::json::array SerializeOffices(const model::Map& map) const;

    model::Game& game_;
    ApiHandler api_handler_;
    std::string static_dir_;
    bool manual_tick_allowed_;
    extra_data::ExtraData extra_data_;
    std::shared_ptr<db::RecordManager> record_manager_;
};

} // namespace http_handler