#include "request_handler.h"
#include "model.h"
#include "game_session.h"
#include <boost/json.hpp>
#include <iostream>

namespace http_handler {

RequestHandler::RequestHandler(model::Game& game)
    : game_(game)
    , session_(game) {
}

void RequestHandler::operator()(http::request<http::string_body>&& req,
                                 std::function<void(http::response<http::string_body>&&)>&& send) {
    std::string target = std::string(req.target());
    std::string method = std::string(req.method_string());
    
    http::response<http::string_body> response;
    response.version(req.version());
    response.keep_alive(false);
    
    try {
        // Обработка GET /api/maps
        if (method == "GET" && target == "/api/maps") {
            response.result(http::status::ok);
            response.set(http::field::content_type, "application/json");
            response.body() = SerializeMaps();
        }
        // Обработка GET /api/maps/{id}
        else if (method == "GET" && target.find("/api/maps/") == 0) {
            std::string map_id_str = target.substr(10); // после "/api/maps/"
            model::Map::Id map_id{map_id_str};
            const model::Map* map = game_.FindMap(map_id);
            
            if (!map) {
                response.result(http::status::not_found);
                response.set(http::field::content_type, "application/json");
                response.body() = R"({"code":"mapNotFound","message":"Map not found"})";
            } else {
                response.result(http::status::ok);
                response.set(http::field::content_type, "application/json");
                response.body() = SerializeMap(*map);
            }
        }
        // Обработка POST /api/game/join
        else if (method == "POST" && target == "/api/game/join") {
            // Парсим JSON тело запроса
            auto body_json = boost::json::parse(req.body());
            auto obj = body_json.as_object();
            
            std::string user_name = std::string(obj["userName"].as_string());
            std::string map_id_str = std::string(obj["mapId"].as_string());
            model::Map::Id map_id{map_id_str};
            
            auto result = session_.JoinGame(user_name, map_id);
            
            response.result(http::status::ok);
            response.set(http::field::content_type, "application/json");
            boost::json::object resp_obj;
            resp_obj["authToken"] = *result.token;
            resp_obj["playerId"] = *result.player_id;
            response.body() = boost::json::serialize(resp_obj);
        }
        // Обработка GET /api/game/players
        else if (method == "GET" && target == "/api/game/players") {
            // Извлекаем токен из заголовка Authorization
            auto auth_header = req.find(http::field::authorization);
            if (auth_header == req.end()) {
                response.result(http::status::unauthorized);
                response.set(http::field::content_type, "application/json");
                response.body() = R"({"code":"invalidToken","message":"Authorization header missing"})";
            } else {
                std::string auth_value = std::string(auth_header->value());
                if (auth_value.find("Bearer ") != 0) {
                    response.result(http::status::unauthorized);
                    response.set(http::field::content_type, "application/json");
                    response.body() = R"({"code":"invalidToken","message":"Invalid authorization header format"})";
                } else {
                    std::string token_str = auth_value.substr(7);
                    model::Token token{token_str};
                    
                    if (!session_.ValidateToken(token)) {
                        response.result(http::status::unauthorized);
                        response.set(http::field::content_type, "application/json");
                        response.body() = R"({"code":"invalidToken","message":"Invalid token"})";
                    } else {
                        auto players = session_.GetPlayersOnMap(token);
                        response.result(http::status::ok);
                        response.set(http::field::content_type, "application/json");
                        
                        boost::json::object players_obj;
                        for (const auto& [id, name] : players) {
                            players_obj[id] = name;
                        }
                        response.body() = boost::json::serialize(players_obj);
                    }
                }
            }
        }
        // Обработка неизвестного маршрута
        else {
            response.result(http::status::not_found);
            response.set(http::field::content_type, "application/json");
            response.body() = R"({"code":"notFound","message":"Not found"})";
        }
    } catch (const std::exception& ex) {
        response.result(http::status::internal_server_error);
        response.set(http::field::content_type, "application/json");
        response.body() = R"({"code":"internalError","message":")" + std::string(ex.what()) + R"("})";
    }
    
    response.content_length(response.body().size());
    send(std::move(response));
}

std::string RequestHandler::SerializeMaps() const {
    boost::json::array maps_array;
    for (const auto& map : game_.GetMaps()) {
        maps_array.push_back(boost::json::object{
            {"id", *map.GetId()},
            {"name", map.GetName()}
        });
    }
    return boost::json::serialize(maps_array);
}

std::string RequestHandler::SerializeMap(const model::Map& map) const {
    boost::json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();
    map_obj["roads"] = SerializeRoads(map);
    map_obj["buildings"] = SerializeBuildings(map);
    map_obj["offices"] = SerializeOffices(map);
    
    return boost::json::serialize(map_obj);
}

boost::json::array RequestHandler::SerializeRoads(const model::Map& map) const {
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
    return roads_array;
}

boost::json::array RequestHandler::SerializeBuildings(const model::Map& map) const {
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
    return buildings_array;
}

boost::json::array RequestHandler::SerializeOffices(const model::Map& map) const {
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
    return offices_array;
}

}  // namespace http_handler