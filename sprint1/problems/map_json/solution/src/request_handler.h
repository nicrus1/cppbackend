#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <boost/json/serialize.hpp>

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
        std::string_view target = req.target();
        std::string_view method = req.method_string();
        
        // Только GET запросы
        if (method != "GET") {
            auto response = MakeErrorResponse(std::move(req), 
                                             http::status::method_not_allowed,
                                             "badRequest", "Method not allowed");
            send(std::move(response));
            return;
        }
        
        // Обработка /api/v1/maps
        if (target == "/api/v1/maps") {
            std::string body = SerializeMaps();
            auto response = MakeResponse(std::move(req), 
                                        http::status::ok, 
                                        "application/json", 
                                        body);
            send(std::move(response));
            return;
        }
        
        // Обработка /api/v1/maps/{id}
        if (target.substr(0, 14) == "/api/v1/maps/") {
            std::string map_id_str(target.substr(14));
            
            // Удаляем возможный trailing slash
            if (!map_id_str.empty() && map_id_str.back() == '/') {
                map_id_str.pop_back();
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
            return;
        }
        
        // Обработка других /api/ запросов
        if (target.substr(0, 5) == "/api/") {
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
                {"id", *map.GetId()},
                {"name", map.GetName()}
            });
        }
        return boost::json::serialize(maps_array);
    }
    
    std::string SerializeMap(const model::Map& map) const {
        boost::json::object map_obj;
        map_obj["id"] = *map.GetId();
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
                {"id", *office.GetId()},
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