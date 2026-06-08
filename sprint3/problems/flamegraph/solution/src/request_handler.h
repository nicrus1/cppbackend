#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <boost/json/serialize.hpp>
#include <iostream>
#include <chrono>
#include <thread>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;

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
        auto start = std::chrono::steady_clock::now();
        
        std::string target = std::string(req.target());
        std::string method = std::string(req.method_string());
        
        // Убираем query string если есть
        auto query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            target = target.substr(0, query_pos);
        }
        
        // Логируем для отладки
        std::cout << "Request: " << method << " " << target << std::endl;
        
        // Только GET запросы
        if (method != "GET") {
            auto response = MakeErrorResponse(std::move(req), 
                                             http::status::method_not_allowed,
                                             "badRequest", "Method not allowed");
            send(std::move(response));
            return;
        }
        
        // Обработка /api/v1/maps
        if (target == "/api/v1/maps" || target == "api/v1/maps") {
            // Имитация небольшой нагрузки для профилирования
            volatile int dummy = 0;
            for (int i = 0; i < 1000; ++i) {
                dummy += i;
            }
            
            std::string body = SerializeMaps();
            auto response = MakeResponse(std::move(req), 
                                        http::status::ok, 
                                        "application/json", 
                                        body);
            send(std::move(response));
            return;
        }
        
        // Обработка /api/v1/maps/{id}
        std::string prefix1 = "/api/v1/maps/";
        std::string prefix2 = "api/v1/maps/";
        
        if (target.find(prefix1) == 0) {
            std::string map_id_str = target.substr(prefix1.length());
            ProcessMapRequest(std::move(req), std::move(map_id_str), std::forward<Send>(send));
            return;
        }
        else if (target.find(prefix2) == 0) {
            std::string map_id_str = target.substr(prefix2.length());
            ProcessMapRequest(std::move(req), std::move(map_id_str), std::forward<Send>(send));
            return;
        }
        
        // Обработка других /api/ запросов
        if (target.find("/api/") == 0 || target.find("api/") == 0) {
            auto response = MakeErrorResponse(std::move(req),
                                             http::status::bad_request,
                                             "badRequest",
                                             "Bad request");
            send(std::move(response));
            return;
        }
        
        // Для всех остальных запросов возвращаем 404
        auto response = MakeErrorResponse(std::move(req),
                                         http::status::not_found,
                                         "badRequest",
                                         "Not found");
        send(std::move(response));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > 1000) {
            std::cout << "Slow request: " << elapsed.count() << " microseconds" << std::endl;
        }
    }
    
    template <typename Body, typename Allocator, typename Send>
    void ProcessMapRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                          std::string map_id_str,
                          Send&& send) {
        // Удаляем возможный trailing slash
        while (!map_id_str.empty() && map_id_str.back() == '/') {
            map_id_str.pop_back();
        }
        
        // Удаляем возможные query параметры
        auto query_pos = map_id_str.find('?');
        if (query_pos != std::string::npos) {
            map_id_str = map_id_str.substr(0, query_pos);
        }
        
        std::cout << "Looking for map id: '" << map_id_str << "'" << std::endl;
        
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
    
    std::string SerializeMaps() const {
        boost::json::array maps_array;
        for (const auto& map : game_.GetMaps()) {
            maps_array.push_back(boost::json::object{
                {"id", std::string(*map.GetId())},
                {"name", map.GetName()}
            });
        }
        return boost::json::serialize(maps_array);
    }
    
    std::string SerializeMap(const model::Map& map) const {
        boost::json::object map_obj;
        map_obj["id"] = std::string(*map.GetId());
        map_obj["name"] = map.GetName();
        
        // Сериализуем дороги
        boost::json::array roads_array;
        for (const auto& road : map.GetRoads()) {
            boost::json::object road_obj;
            auto start = road.GetStart();
            auto end = road.GetEnd();
            
            if (road.IsHorizontal()) {
                road_obj["x0"] = start.x;
                road_obj["y0"] = start.y;
                road_obj["x1"] = end.x;
            } else {
                road_obj["x0"] = start.x;
                road_obj["y0"] = start.y;
                road_obj["y1"] = end.y;
            }
            roads_array.push_back(road_obj);
        }
        map_obj["roads"] = roads_array;
        
        // Сериализуем здания
        boost::json::array buildings_array;
        for (const auto& building : map.GetBuildings()) {
            const auto& bounds = building.GetBounds();
            buildings_array.push_back(boost::json::object{
                {"x", bounds.position.x},
                {"y", bounds.position.y},
                {"w", bounds.size.width},
                {"h", bounds.size.height}
            });
        }
        map_obj["buildings"] = buildings_array;
        
        // Сериализуем офисы
        boost::json::array offices_array;
        for (const auto& office : map.GetOffices()) {
            offices_array.push_back(boost::json::object{
                {"id", std::string(*office.GetId())},
                {"x", office.GetPosition().x},
                {"y", office.GetPosition().y},
                {"offsetX", office.GetOffset().dx},
                {"offsetY", office.GetOffset().dy}
            });
        }
        map_obj["offices"] = offices_array;
        
        return boost::json::serialize(map_obj);
    }

    model::Game& game_;
};

}  // namespace http_handler