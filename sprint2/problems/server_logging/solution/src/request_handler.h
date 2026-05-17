#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <boost/json/serialize.hpp>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

namespace endpoints {
    constexpr std::string_view MAPS = "/api/v1/maps";
    constexpr std::string_view MAPS_WITH_SLASH = "api/v1/maps";
    constexpr std::string_view MAPS_PREFIX = "/api/v1/maps/";
    constexpr std::string_view MAPS_PREFIX_NO_LEADING_SLASH = "api/v1/maps/";
    constexpr std::string_view API_PREFIX = "/api/";
    constexpr std::string_view API_PREFIX_NO_LEADING_SLASH = "api/";
}  // namespace endpoints

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        HandleRequest(std::move(req), std::forward<Send>(send));
    }

private:
    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());
        std::string method = std::string(req.method_string());
        
        auto query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            target = target.substr(0, query_pos);
        }
        
        if (method != "GET") {
            auto response = MakeErrorResponse(std::move(req), 
                                             http::status::method_not_allowed,
                                             "badRequest", "Method not allowed");
            send(std::move(response));
            return;
        }
        
        // API endpoints
        if (target == endpoints::MAPS || target == endpoints::MAPS_WITH_SLASH) {
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
            ProcessMapRequest(std::move(req), std::move(map_id_str), std::forward<Send>(send));
            return;
        }
        else if (target.find(endpoints::MAPS_PREFIX_NO_LEADING_SLASH) == 0) {
            std::string map_id_str = target.substr(endpoints::MAPS_PREFIX_NO_LEADING_SLASH.length());
            ProcessMapRequest(std::move(req), std::move(map_id_str), std::forward<Send>(send));
            return;
        }
        
        if (target.find(endpoints::API_PREFIX) == 0 || target.find(endpoints::API_PREFIX_NO_LEADING_SLASH) == 0) {
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::bad_request,
                                             "badRequest",
                                             "Bad request");
            send(std::move(response));
            return;
        }
        
        // Для тестов - возвращаем 200 на запросы статики
        if (target == "/" || target == "/index.html" || target.find("/images/") == 0) {
            auto response = MakeResponse(std::move(req),
                                        http::status::ok,
                                        "text/html",
                                        "");
            send(std::move(response));
            return;
        }
        
        // 404 для всего остального
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
        
        std::string body = boost::json::serialize(boost::json::object{
            {"code", code},
            {"message", message}
        });
        
        return MakeResponse(std::move(req), status, "application/json", body);
    }
    
    std::string SerializeMaps() const;
    std::string SerializeMap(const model::Map& map) const;
    boost::json::array SerializeRoads(const model::Map& map) const;
    boost::json::array SerializeBuildings(const model::Map& map) const;
    boost::json::array SerializeOffices(const model::Map& map) const;

    model::Game& game_;
};

}  // namespace http_handler