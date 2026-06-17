#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdint>
#include <string>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <random>

#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "model.h"
#include "extra_data.h"
#include "loot_generator.h"

namespace json = boost::json;
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Функция для безопасного парсинга чисел из JSON
double AsDouble(const boost::json::value& val) {
    if (val.is_double()) return val.as_double();
    if (val.is_int64()) return static_cast<double>(val.as_int64());
    if (val.is_uint64()) return static_cast<double>(val.as_uint64());
    throw std::runtime_error("Value is not a number");
}

std::string MakeErrorJson(const std::string& code, const std::string& message) {
    boost::json::object obj;
    obj["code"] = code;
    obj["message"] = message;
    return boost::json::serialize(obj);
}

// Генерация случайного 32-значного токена авторизации
std::string GenerateToken() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << dist(rng) 
       << std::hex << std::setfill('0') << std::setw(16) << dist(rng);
    return ss.str();
}

struct PlayerInfo {
    std::shared_ptr<model::GameSession> session;
    model::Player* player;
};

class GameServer {
public:
    GameServer(const std::string& config_path, unsigned short port = 8080)
        : port_(port)
        , acceptor_(io_context_, tcp::endpoint(net::ip::make_address("0.0.0.0"), port_)) {
        LoadConfig(config_path);
    }
    
    void Run() {
        std::cout << "Server running on port " << port_ << "...\n";
        DoAccept();
        
        // Таймер для обновления состояния игры в главном потоке ASIO (устраняет Data Race)
        auto timer = std::make_shared<net::steady_timer>(io_context_, std::chrono::milliseconds(50));
        std::function<void(boost::system::error_code)> timer_handler;
        auto last_time = std::chrono::steady_clock::now();
        
        timer_handler = [this, timer, &last_time, &timer_handler](boost::system::error_code ec) {
            if (!ec && running_) {
                auto now = std::chrono::steady_clock::now();
                auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
                if (delta.count() > 0) {
                    game_.Update(delta);
                    last_time = now;
                }
                timer->expires_after(std::chrono::milliseconds(50));
                timer->async_wait(timer_handler);
            }
        };
        timer->async_wait(timer_handler);

        // Блокирующий запуск цикла (один поток)
        io_context_.run();
    }
    
    void Stop() { running_ = false; }

private:
    void DoAccept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<HttpSession>(std::move(socket), *this);
                    session->Start();
                }
                if (running_) {
                    DoAccept();
                }
            }
        );
    }
    
    void LoadConfig(const std::string& config_path) {
        std::ifstream f(config_path);
        if (!f.is_open()) {
            throw std::runtime_error("Failed to open config file: " + config_path);
        }
        
        std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto config = json::parse(str);
        
        if (config.as_object().contains("lootGeneratorConfig")) {
            auto loot_config = config.at("lootGeneratorConfig").as_object();
            extra_data_.SetLootGeneratorConfig(
                AsDouble(loot_config.at("period")), AsDouble(loot_config.at("probability"))
            );
        } else {
            extra_data_.SetLootGeneratorConfig(1.0, 1.0);
        }
        
        if (config.as_object().contains("maps")) {
            json::array maps_array;
            for (const auto& map_val : config.at("maps").as_array()) {
                auto map_obj = map_val.as_object();
                std::string id = map_obj.at("id").as_string().c_str();
                std::string name = map_obj.at("name").as_string().c_str();
                
                json::object m_short;
                m_short["id"] = id;
                m_short["name"] = name;
                maps_array.push_back(m_short);
                
                extra_data::MapExtraData map_extra;
                if (map_obj.contains("lootTypes")) {
                    auto loot_types_arr = map_obj.at("lootTypes").as_array();
                    extra_data::MapExtraData::LootTypes loot_types;
                    for (const auto& loot_val : loot_types_arr) {
                        loot_types.push_back({loot_val.as_object()});
                    }
                    map_extra.SetLootTypes(std::move(loot_types));
                }
                
                auto map = std::make_shared<model::Map>(model::Map::Id(id), name, map_extra.GetLootTypes().size());
                
                if (map_obj.contains("roads")) {
                    for (const auto& road_val : map_obj.at("roads").as_array()) {
                        auto road_obj = road_val.as_object();
                        double x0 = AsDouble(road_obj.at("x0"));
                        double y0 = AsDouble(road_obj.at("y0"));
                        double x1 = road_obj.contains("x1") ? AsDouble(road_obj.at("x1")) : x0;
                        double y1 = road_obj.contains("y1") ? AsDouble(road_obj.at("y1")) : y0;
                        map->AddRoad({{x0, y0}, {x1, y1}});
                    }
                }
                
                game_.AddMap(map);
                extra_data_.SetMapExtraData(id, std::move(map_extra));
                
                auto period_ms = std::chrono::milliseconds(static_cast<long long>(extra_data_.GetLootGeneratorPeriod() * 1000));
                auto session = std::make_shared<model::GameSession>(
                    model::GameSession::Id(next_session_id_++), map, period_ms, extra_data_.GetLootGeneratorProbability()
                );
                game_.AddSession(session);
                
                // Удаляем dogSpeed для клиента и кэшируем оригинальную структуру
                map_obj.erase("dogSpeed");
                map_json_cache_[id] = map_obj;
            }
            maps_list_json_ = boost::json::serialize(maps_array);
        }
    }
    
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    public:
        HttpSession(tcp::socket socket, GameServer& server)
            : socket_(std::move(socket)), server_(server) {}
        
        void Start() { DoRead(); }
        
    private:
        void DoRead() {
            auto self = shared_from_this();
            http::async_read(socket_, buffer_, request_,
                [self](beast::error_code ec, size_t) {
                    if (!ec) self->HandleRequest();
                });
        }
        
        void HandleRequest() {
            response_ = server_.HandleHttpRequest(request_);
            response_.keep_alive(request_.keep_alive()); // Поддержка Keep-Alive для pytest/requests
            response_.prepare_payload(); 
            
            auto self = shared_from_this();
            http::async_write(socket_, response_,
                [self](beast::error_code ec, size_t) {
                    if (!ec && self->response_.keep_alive()) {
                        self->request_ = {};
                        self->DoRead();
                    } else {
                        self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                    }
                });
        }
        
        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> request_;
        http::response<http::string_body> response_; // Теперь response живёт до конца отправки!
        GameServer& server_;
    };
    
    http::response<http::string_body> HandleHttpRequest(const http::request<http::string_body>& req) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "GameServer");
        res.set(http::field::content_type, "application/json");
        
        try {
            std::string path(req.target().data(), req.target().size());
            
            // Список карт
            if (path == "/api/v1/maps") {
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    res.body() = maps_list_json_;
                    res.set(http::field::cache_control, "no-cache");
                    return res;
                } else {
                    res.result(http::status::method_not_allowed);
                    res.set(http::field::cache_control, "no-cache");
                    res.set(http::field::allow, "GET, HEAD");
                    res.body() = MakeErrorJson("invalidMethod", "Only GET and HEAD are allowed");
                    return res;
                }
            }
            
            // Конкретная карта
            if (path.find("/api/v1/maps/") == 0) {
                std::string map_id = path.substr(14);
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    auto it = map_json_cache_.find(map_id);
                    if (it == map_json_cache_.end()) {
                        res.result(http::status::not_found);
                        res.set(http::field::cache_control, "no-cache");
                        res.body() = MakeErrorJson("mapNotFound", "Map not found");
                        return res;
                    }
                    res.body() = boost::json::serialize(it->second);
                    res.set(http::field::cache_control, "no-cache");
                    return res;
                } else {
                    res.result(http::status::method_not_allowed);
                    res.set(http::field::cache_control, "no-cache");
                    res.set(http::field::allow, "GET, HEAD");
                    res.body() = MakeErrorJson("invalidMethod", "Only GET and HEAD are allowed");
                    return res;
                }
            }
            
            // Присоединение к игре
            if (path == "/api/v1/game/join") {
                if (req.method() == http::verb::post) {
                    try {
                        auto body = json::parse(req.body());
                        if (!body.is_object() || !body.as_object().contains("mapId") || !body.as_object().contains("userName")) {
                            res.result(http::status::bad_request);
                            res.set(http::field::cache_control, "no-cache");
                            res.body() = MakeErrorJson("invalidArgument", "Join game request parse error");
                            return res;
                        }
                        
                        std::string map_id = body.as_object().at("mapId").as_string().c_str();
                        std::string user_name = body.as_object().at("userName").as_string().c_str();
                        
                        if (user_name.empty()) {
                            res.result(http::status::bad_request);
                            res.set(http::field::cache_control, "no-cache");
                            res.body() = MakeErrorJson("invalidArgument", "Invalid name");
                            return res;
                        }
                        
                        auto map = game_.FindMap(model::Map::Id(map_id));
                        if (!map) {
                            res.result(http::status::not_found);
                            res.set(http::field::cache_control, "no-cache");
                            res.body() = MakeErrorJson("mapNotFound", "Map not found");
                            return res;
                        }
                        
                        std::shared_ptr<model::GameSession> target_session;
                        for (auto& [session_id, session] : game_.GetSessions()) {
                            if (session->GetMap()->GetId() == map->GetId()) {
                                target_session = session;
                                break;
                            }
                        }
                        
                        auto player = target_session->AddPlayer(user_name);
                        std::string token = GenerateToken();
                        tokens_[token] = {target_session, player};
                        
                        json::object response;
                        response["authToken"] = token;
                        response["playerId"] = player->id;
                        
                        res.set(http::field::cache_control, "no-cache");
                        res.body() = json::serialize(response);
                        return res;
                    } catch (...) {
                        res.result(http::status::bad_request);
                        res.set(http::field::cache_control, "no-cache");
                        res.body() = MakeErrorJson("invalidArgument", "Join game request parse error");
                        return res;
                    }
                } else {
                    res.result(http::status::method_not_allowed);
                    res.set(http::field::cache_control, "no-cache");
                    res.set(http::field::allow, "POST");
                    res.body() = MakeErrorJson("invalidMethod", "Only POST is allowed");
                    return res;
                }
            }
            
            // Игровое состояние
            if (path == "/api/v1/game/state") {
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    auto auth_it = req.find(http::field::authorization);
                    if (auth_it == req.end() || std::string(auth_it->value()).find("Bearer ") != 0) {
                        res.result(http::status::unauthorized);
                        res.set(http::field::cache_control, "no-cache");
                        res.body() = MakeErrorJson("invalidToken", "Authorization header is missing");
                        return res;
                    }
                    
                    std::string token = std::string(auth_it->value()).substr(7);
                    if (token.length() != 32 || tokens_.find(token) == tokens_.end()) {
                        res.result(http::status::unauthorized);
                        res.set(http::field::cache_control, "no-cache");
                        res.body() = MakeErrorJson("unknownToken", "Player token has not been found");
                        return res;
                    }
                    
                    auto player_info = tokens_[token];
                    auto session = player_info.session;
                    
                    json::object state_json;
                    json::object players_json;
                    for (const auto& [player_id, p] : session->GetPlayers()) {
                        json::object p_json;
                        p_json["pos"] = json::array{p.position.x, p.position.y};
                        p_json["speed"] = json::array{p.speed[0], p.speed[1]};
                        p_json["dir"] = p.dir;
                        players_json[std::to_string(player_id)] = std::move(p_json);
                    }
                    
                    json::object lost_objects;
                    for (const auto& [obj_id, obj] : session->GetLostObjects()) {
                        json::object obj_json;
                        obj_json["type"] = static_cast<std::int64_t>(obj.type);
                        obj_json["pos"] = json::array{obj.position.x, obj.position.y};
                        lost_objects[std::to_string(obj_id.GetUnderlying())] = std::move(obj_json);
                    }
                    
                    state_json["players"] = std::move(players_json);
                    state_json["lostObjects"] = std::move(lost_objects);
                    
                    res.set(http::field::cache_control, "no-cache");
                    res.body() = json::serialize(state_json);
                    return res;
                } else {
                    res.result(http::status::method_not_allowed);
                    res.set(http::field::cache_control, "no-cache");
                    res.set(http::field::allow, "GET, HEAD");
                    res.body() = MakeErrorJson("invalidMethod", "Only GET and HEAD are allowed");
                    return res;
                }
            }

            // Перемещение времени в игре (Tick API)
            if (path == "/api/v1/game/tick") {
                if (req.method() == http::verb::post) {
                    try {
                        auto body = json::parse(req.body());
                        if (body.as_object().contains("timeDelta")) {
                            auto delta_ms = body.as_object().at("timeDelta").as_int64();
                            game_.Update(std::chrono::milliseconds(delta_ms));
                        } else {
                            res.result(http::status::bad_request);
                            res.set(http::field::cache_control, "no-cache");
                            res.body() = MakeErrorJson("invalidArgument", "Failed to parse tick request JSON");
                            return res;
                        }
                    } catch (...) {
                        res.result(http::status::bad_request);
                        res.set(http::field::cache_control, "no-cache");
                        res.body() = MakeErrorJson("invalidArgument", "Failed to parse tick request JSON");
                        return res;
                    }
                    res.set(http::field::cache_control, "no-cache");
                    res.body() = "{}";
                    return res;
                } else {
                    res.result(http::status::method_not_allowed);
                    res.set(http::field::cache_control, "no-cache");
                    res.set(http::field::allow, "POST");
                    res.body() = MakeErrorJson("invalidMethod", "Only POST is allowed");
                    return res;
                }
            }
            
            res.result(http::status::bad_request);
            res.set(http::field::cache_control, "no-cache");
            res.body() = MakeErrorJson("badRequest", "Bad request");
            return res;
            
        } catch (const std::exception& e) {
            res.result(http::status::bad_request);
            res.body() = json::serialize(json::object{{"error", e.what()}});
            return res;
        }
    }
    
    model::Game game_;
    extra_data::ExtraData extra_data_;
    bool running_ = true;
    size_t next_session_id_ = 0;
    
    std::unordered_map<std::string, PlayerInfo> tokens_;
    std::unordered_map<std::string, boost::json::object> map_json_cache_;
    std::string maps_list_json_;
    
    unsigned short port_;
    net::io_context io_context_;
    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config.json> [port]\n";
        return 1;
    }
    
    unsigned short port = 8080;
    if (argc >= 3) {
        port = static_cast<unsigned short>(std::stoi(argv[2]));
    }
    
    try {
        GameServer server(argv[1], port);
        server.Run();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    
    return 0;
}