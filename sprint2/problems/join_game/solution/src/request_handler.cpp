#include "request_handler.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace http_handler {

// ========== API ==========

API::API(model::Game& game, model::Players& players)
    : game_(game)
    , players_(players)
{
}

std::optional<API::PlayerInfo> API::JoinGame(const std::string& username, model::Map::Id map_id)
{
    const model::Map* map = game_.FindMap(map_id);
    if (!map)
    {
        return std::nullopt;
    }
    
    model::Player& player = players_.AddPlayer(username, map_id);
    model::Token token = players_.GenerateToken(player);
    
    return PlayerInfo{std::move(token), static_cast<size_t>(*player.GetId())};
}

std::optional<std::vector<std::pair<size_t, std::string>>> API::GetPlayerList(model::Token token) const
{
    const model::Player* player = players_.FindPlayerByToken(token);
    if (!player)
    {
        return std::nullopt;
    }
    
    auto players_on_map = players_.GetPlayersOnMap(player->GetMapId());
    std::vector<std::pair<size_t, std::string>> result;
    for (auto* p : players_on_map)
    {
        result.emplace_back(static_cast<size_t>(*p->GetId()), p->GetName());
    }
    return result;
}

const model::Game::Maps& API::GetMaps() const
{
    return game_.GetMaps();
}

const model::Map* API::FindMap(model::Map::Id map_id) const
{
    return game_.FindMap(map_id);
}

const model::Player* API::FindPlayer(model::Token token) const
{
    return players_.FindPlayerByToken(token);
}

// ========== APIRequestHandler ==========

APIRequestHandler::APIRequestHandler(model::Game& game, model::Players& players)
    : api_(game, players)
{
}

// Maps handlers
StringResponse APIRequestHandler::HandleMapRequest(const StringRequest& req) const
{
    if (req.method() != http::verb::get)
    {
        return HandleJsonBadRequest(req);
    }
    
    boost::json::array map_arr;
    ParseMapArray(map_arr);
    
    auto response = MakeHeader<StringResponse>(http::status::ok, req, "application/json");
    response.body() = boost::json::serialize(map_arr);
    response.prepare_payload();
    return response;
}

StringResponse APIRequestHandler::HandleMapIdRequest(const StringRequest& req, model::Map::Id map_id) const
{
    if (req.method() != http::verb::get)
    {
        return HandleJsonBadRequest(req);
    }
    
    const model::Map* map = api_.FindMap(map_id);
    if (!map)
    {
        return HandleJsonBadRequest(req);
    }
    
    boost::json::object map_obj;
    map_obj["id"] = *map->GetId();
    map_obj["name"] = map->GetName();
    
    boost::json::array roads_arr;
    ParseRoads(map, roads_arr);
    map_obj["roads"] = roads_arr;
    
    boost::json::array buildings_arr;
    ParseBuildings(map, buildings_arr);
    map_obj["buildings"] = buildings_arr;
    
    boost::json::array offices_arr;
    ParseOffices(map, offices_arr);
    map_obj["offices"] = offices_arr;
    
    auto response = MakeHeader<StringResponse>(http::status::ok, req, "application/json");
    response.body() = boost::json::serialize(map_obj);
    response.prepare_payload();
    return response;
}

void APIRequestHandler::ParseMapArray(boost::json::array& map_arr) const
{
    for (const auto& map : api_.GetMaps())
    {
        boost::json::object map_obj;
        map_obj["id"] = *map.GetId();
        map_obj["name"] = map.GetName();
        map_arr.push_back(map_obj);
    }
}

void APIRequestHandler::ParseRoads(const model::Map* map, boost::json::array& roads_arr) const
{
    for (const auto& road : map->GetRoads())
    {
        boost::json::object road_obj;
        auto start = road.GetStart();
        auto end = road.GetEnd();
        
        if (road.IsHorizontal())
        {
            road_obj["x0"] = start.x;
            road_obj["y0"] = start.y;
            road_obj["x1"] = end.x;
        }
        else
        {
            road_obj["x0"] = start.x;
            road_obj["y0"] = start.y;
            road_obj["y1"] = end.y;
        }
        roads_arr.push_back(road_obj);
    }
}

void APIRequestHandler::ParseBuildings(const model::Map* map, boost::json::array& buildings_arr) const
{
    for (const auto& building : map->GetBuildings())
    {
        const auto& bounds = building.GetBounds();
        buildings_arr.push_back(boost::json::object{
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        });
    }
}

void APIRequestHandler::ParseOffices(const model::Map* map, boost::json::array& offices_arr) const
{
    for (const auto& office : map->GetOffices())
    {
        offices_arr.push_back(boost::json::object{
            {"id", *office.GetId()},
            {"x", office.GetPosition().x},
            {"y", office.GetPosition().y},
            {"offsetX", office.GetOffset().dx},
            {"offsetY", office.GetOffset().dy}
        });
    }
}

StringResponse APIRequestHandler::HandleJsonBadRequest(const StringRequest& req) const
{
    auto response = MakeHeader<StringResponse>(http::status::bad_request, req, "application/json");
    response.body() = boost::json::serialize(boost::json::object{
        {"code", "badRequest"},
        {"message", "Bad request"}
    });
    response.prepare_payload();
    return response;
}

// Join game handlers
StringResponse APIRequestHandler::MakeJsonBasedResponse(http::status status, const StringRequest& req, json::object response_obj)
{
    auto response = MakeHeader<StringResponse>(status, req, "application/json");
    response.body() = boost::json::serialize(response_obj);
    response.set(http::field::cache_control, "no-cache");
    response.prepare_payload();
    return response;
}

StringResponse APIRequestHandler::HandleJoinGame(const StringRequest& req)
{
    if (req.method() != http::verb::post)
    {
        return HandleJoinMethodNotAllowed(req);
    }
    
    boost::json::value json;
    try
    {
        json = boost::json::parse(req.body());
    }
    catch (...)
    {
        return HandleParsingError(req);
    }
    
    if (!json.is_object())
    {
        return HandleParsingError(req);
    }
    
    auto& obj = json.as_object();
    
    if (!obj.contains("userName") || !obj.at("userName").is_string())
    {
        return HandleEmptyName(req);
    }
    
    std::string user_name = std::string(obj.at("userName").as_string());
    if (user_name.empty())
    {
        return HandleEmptyName(req);
    }
    
    if (!obj.contains("mapId") || !obj.at("mapId").is_string())
    {
        return HandleMapNotFound(req);
    }
    
    model::Map::Id map_id{std::string(obj.at("mapId").as_string())};
    
    auto result = api_.JoinGame(user_name, map_id);
    if (!result)
    {
        return HandleMapNotFound(req);
    }
    
    boost::json::object response_obj;
    response_obj["authToken"] = *result->auth_token;
    response_obj["playerId"] = static_cast<int64_t>(result->player_id);
    
    return MakeJsonBasedResponse(http::status::ok, req, response_obj);
}

StringResponse APIRequestHandler::HandleEmptyName(const StringRequest& req)
{
    return MakeJsonBasedResponse(http::status::bad_request, req, boost::json::object{
        {"code", "invalidArgument"},
        {"message", "Invalid name"}
    });
}

StringResponse APIRequestHandler::HandleParsingError(const StringRequest& req)
{
    return MakeJsonBasedResponse(http::status::bad_request, req, boost::json::object{
        {"code", "invalidArgument"},
        {"message", "Join game request parse error"}
    });
}

StringResponse APIRequestHandler::HandleMapNotFound(const StringRequest& req)
{
    return MakeJsonBasedResponse(http::status::not_found, req, boost::json::object{
        {"code", "mapNotFound"},
        {"message", "Map not found"}
    });
}

StringResponse APIRequestHandler::HandleJoinMethodNotAllowed(const StringRequest& req)
{
    auto response = MakeHeader<StringResponse>(http::status::method_not_allowed, req, "application/json");
    response.body() = boost::json::serialize(boost::json::object{
        {"code", "invalidMethod"},
        {"message", "Only POST method is expected"}
    });
    response.set(http::field::allow, "POST");
    response.set(http::field::cache_control, "no-cache");
    response.prepare_payload();
    return response;
}

// Player list handlers
StringResponse APIRequestHandler::HandlePlayerList(const StringRequest& req)
{
    if (req.method() != http::verb::get && req.method() != http::verb::head)
    {
        return HandlePlayerListMethodNotAllowed(req);
    }
    
    // Extract token from Authorization header
    auto token_opt = ExtractToken(req);
    if (!token_opt)
    {
        return HandleInvalidToken(req);
    }
    
    auto player_list_opt = api_.GetPlayerList(*token_opt);
    if (!player_list_opt)
    {
        return HandleUnknownToken(req);
    }
    
    if (req.method() == http::verb::head)
    {
        auto response = MakeHeader<StringResponse>(http::status::ok, req, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.prepare_payload();
        return response;
    }
    
    boost::json::object player_list_obj;
    ParsePlayerList(player_list_obj, *player_list_opt);
    
    auto response = MakeHeader<StringResponse>(http::status::ok, req, "application/json");
    response.body() = boost::json::serialize(player_list_obj);
    response.set(http::field::cache_control, "no-cache");
    response.prepare_payload();
    return response;
}

void APIRequestHandler::ParsePlayerList(boost::json::object& player_list_obj, const std::vector<std::pair<size_t, std::string>>& player_list)
{
    for (const auto& [id, name] : player_list)
    {
        player_list_obj[std::to_string(id)] = boost::json::object{{"name", name}};
    }
}

StringResponse APIRequestHandler::HandleInvalidToken(const StringRequest& req)
{
    auto response = MakeHeader<StringResponse>(http::status::unauthorized, req, "application/json");
    response.body() = boost::json::serialize(boost::json::object{
        {"code", "invalidToken"},
        {"message", "Authorization header is missing or invalid"}
    });
    response.set(http::field::cache_control, "no-cache");
    response.prepare_payload();
    return response;
}

StringResponse APIRequestHandler::HandleUnknownToken(const StringRequest& req)
{
    auto response = MakeHeader<StringResponse>(http::status::unauthorized, req, "application/json");
    response.body() = boost::json::serialize(boost::json::object{
        {"code", "unknownToken"},
        {"message", "Player token has not been found"}
    });
    response.set(http::field::cache_control, "no-cache");
    response.prepare_payload();
    return response;
}

StringResponse APIRequestHandler::HandlePlayerListMethodNotAllowed(const StringRequest& req)
{
    auto response = MakeHeader<StringResponse>(http::status::method_not_allowed, req, "application/json");
    response.body() = boost::json::serialize(boost::json::object{
        {"code", "invalidMethod"},
        {"message", "Invalid method"}
    });
    response.set(http::field::allow, "GET, HEAD");
    response.set(http::field::cache_control, "no-cache");
    response.prepare_payload();
    return response;
}

// ========== StaticRequestHandler ==========

StaticRequestHandler::StaticRequestHandler(const fs::path& to_static_folder)
    : to_static_folder_(to_static_folder)
{
}

std::string StaticRequestHandler::DefineContentType(const fs::path& c_target)
{
    std::string target = c_target.string();
    if (target.ends_with(".htm") || target.ends_with(".html"))
    {
        return "text/html";
    }
    else if (target.ends_with(".css"))
    {
        return "text/css";
    }
    else if (target.ends_with(".js"))
    {
        return "application/javascript";
    }
    else if (target.ends_with(".json"))
    {
        return "application/json";
    }
    else if (target.ends_with(".png"))
    {
        return "image/png";
    }
    else if (target.ends_with(".jpg") || target.ends_with(".jpeg"))
    {
        return "image/jpeg";
    }
    else if (target.ends_with(".svg"))
    {
        return "image/svg+xml";
    }
    return "application/octet-stream";
}

bool StaticRequestHandler::IsSubPath(fs::path path) const
{
    auto normalized_path = fs::weakly_canonical(path);
    auto normalized_static = fs::weakly_canonical(to_static_folder_);
    
    for (auto it = normalized_path.begin(); it != normalized_path.end(); ++it)
    {
        if (normalized_static == normalized_path)
        {
            return true;
        }
    }
    return false;
}

StringResponse StaticRequestHandler::HandleFileBadRequest(const StringRequest& req) const
{
    auto response = MakeHeader<StringResponse>(http::status::bad_request, req, "text/plain");
    response.body() = "Bad request";
    response.prepare_payload();
    return response;
}

StringResponse StaticRequestHandler::HandleNotFound(const StringRequest& req) const
{
    auto response = MakeHeader<StringResponse>(http::status::not_found, req, "text/plain");
    response.body() = "File not found";
    response.prepare_payload();
    return response;
}

std::variant<StringResponse, FileResponse> StaticRequestHandler::HandleFileRequest(const StringRequest& req) const
{
    if (req.method() != http::verb::get && req.method() != http::verb::head)
    {
        return HandleFileBadRequest(req);
    }
    
    std::string target = std::string(req.target());
    if (target.empty() || target[0] != '/')
    {
        return HandleFileBadRequest(req);
    }
    
    // Security: prevent path traversal
    fs::path full_path = to_static_folder_ / target.substr(1);
    if (!IsSubPath(full_path))
    {
        return HandleNotFound(req);
    }
    
    // Open file
    boost::beast::error_code ec;
    http::file_body::value_type file;
    file.open(full_path.c_str(), beast::file_mode::read, ec);
    
    if (ec)
    {
        return HandleNotFound(req);
    }
    
    // Create response
    FileResponse response(http::status::ok, req.version());
    response.set(http::field::content_type, DefineContentType(full_path));
    response.body() = std::move(file);
    response.prepare_payload();
    
    return response;
}

// ========== RequestHandler ==========

RequestHandler::RequestHandler(model::Game& game, model::Players& players, const fs::path& to_static_folder, Strand api_strand)
    : api_handler_(game, players)
    , static_handler_(to_static_folder)
    , api_strand_(api_strand)
{
}

StringResponse RequestHandler::HandleNotAllowed(const StringRequest& req)
{
    auto response = MakeHeader<StringResponse>(http::status::method_not_allowed, req, "text/plain");
    response.body() = "Method not allowed";
    response.prepare_payload();
    return response;
}

}  // namespace http_handler