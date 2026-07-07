#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <filesystem>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <sstream>

#include "game.h"
#include "serializing_listener.h"

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace po = boost::program_options;
namespace pt = boost::property_tree;

std::shared_ptr<infrastructure::SerializingListener> global_listener;
std::shared_ptr<model::Game> global_game;
std::atomic<bool> running{true};

void SignalHandler(int signal) {
    std::cout << "Received signal " << signal << std::endl;
    running = false;
    if (global_listener) {
        global_listener->OnShutdown();
    }
}

// Генерация случайного токена
std::string GenerateToken() {
    static const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, chars.size() - 1);
    
    std::string token;
    token.reserve(32);
    for (int i = 0; i < 32; ++i) {
        token.push_back(chars[dist(rng)]);
    }
    return token;
}

// Обработчик HTTP запросов
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(net::ip::tcp::socket socket, std::shared_ptr<model::Game> game)
        : socket_(std::move(socket))
        , game_(game) {
    }
    
    void Run() {
        ReadRequest();
    }
    
private:
    void ReadRequest() {
        auto self = shared_from_this();
        http::async_read(socket_, buffer_, request_,
            [self](beast::error_code ec, size_t) {
                if (!ec) {
                    self->ProcessRequest();
                }
            });
    }
    
    void ProcessRequest() {
        response_.version(request_.version());
        response_.keep_alive(false);
        response_.set(http::field::server, "Game Server");
        response_.set(http::field::content_type, "application/json");
        
        try {
            std::string target = request_.target().to_string();
            
            if (request_.method() == http::verb::post && target == "/api/v1/game/join") {
                HandleJoin();
            } else if (request_.method() == http::verb::get && target == "/api/v1/game/maps") {
                HandleGetMaps();
            } else if (request_.method() == http::verb::get && target == "/api/v1/game/state") {
                HandleGetState();
            } else if (request_.method() == http::verb::post && target == "/api/v1/game/tick") {
                HandleTick();
            } else if (request_.method() == http::verb::get && target == "/api/v1/game/players") {
                HandleGetPlayers();
            } else {
                response_.result(http::status::not_found);
                response_.body() = R"({"error":"Not found"})";
            }
        } catch (const std::exception& e) {
            response_.result(http::status::internal_server_error);
            response_.body() = R"({"error":")" + std::string(e.what()) + R"("})";
        }
        
        WriteResponse();
    }
    
    void HandleJoin() {
        try {
            // Парсим тело запроса
            std::string body = request_.body();
            pt::ptree root;
            std::stringstream ss(body);
            pt::read_json(ss, root);
            
            std::string user_name = root.get<std::string>("userName");
            std::string map_id = root.get<std::string>("mapId");
            
            auto map_state = game_->GetMapState(map_id);
            if (!map_state) {
                response_.result(http::status::not_found);
                response_.body() = R"({"error":"Map not found"})";
                return;
            }
            
            // Генерируем токен
            std::string token = GenerateToken();
            
            // Создаем собаку
            auto dog_id = model::Dog::Id{static_cast<uint32_t>(game_->GetAllDogs().size() + 1)};
            geom::Point2D pos(5.0, 5.0);
            auto dog = std::make_shared<model::Dog>(dog_id, user_name, pos, 3);
            
            // Добавляем игрока
            game_->AddPlayer(token, user_name, map_id, dog);
            
            // Формируем ответ
            pt::ptree response_root;
            response_root.put("authToken", token);
            response_root.put("playerId", *dog_id);
            
            pt::ptree map_info;
            map_info.put("id", map_id);
            map_info.put("name", map_state->name);
            response_root.add_child("map", map_info);
            
            std::stringstream response_ss;
            pt::write_json(response_ss, response_root);
            response_.body() = response_ss.str();
            response_.result(http::status::ok);
            
        } catch (const std::exception& e) {
            response_.result(http::status::bad_request);
            response_.body() = R"({"error":")" + std::string(e.what()) + R"("})";
        }
    }
    
    void HandleGetMaps() {
        pt::ptree response_root;
        pt::ptree maps_array;
        
        for (const auto& map_pair : game_->GetMaps()) {
            const auto& map_state = map_pair.second;
            pt::ptree map_node;
            map_node.put("id", map_state.map_id);
            map_node.put("name", map_state.name);
            map_node.put("dogSpeed", map_state.dog_speed);
            maps_array.push_back(std::make_pair("", map_node));
        }
        
        response_root.add_child("maps", maps_array);
        
        std::stringstream ss;
        pt::write_json(ss, response_root);
        response_.body() = ss.str();
        response_.result(http::status::ok);
    }
    
    void HandleGetState() {
        pt::ptree response_root;
        
        // Информация об игроках
        pt::ptree players_array;
        for (const auto& player_pair : game_->GetPlayers()) {
            const auto& player = player_pair.second;
            if (player.dog) {
                pt::ptree player_node;
                player_node.put("token", player.token);
                player_node.put("userId", player.user_id);
                player_node.put("mapId", player.map_id);
                player_node.put("dogId", *player.dog->GetId());
                
                pt::ptree pos_node;
                pos_node.put("x", player.dog->GetPosition().x);
                pos_node.put("y", player.dog->GetPosition().y);
                player_node.add_child("position", pos_node);
                
                players_array.push_back(std::make_pair("", player_node));
            }
        }
        response_root.add_child("players", players_array);
        
        // Информация о предметах
        pt::ptree loot_array;
        for (const auto& item : game_->GetAllLootItems()) {
            pt::ptree loot_node;
            loot_node.put("id", item.id);
            loot_node.put("type", item.type);
            
            pt::ptree pos_node;
            pos_node.put("x", item.position.x);
            pos_node.put("y", item.position.y);
            loot_node.add_child("position", pos_node);
            
            loot_array.push_back(std::make_pair("", loot_node));
        }
        response_root.add_child("loot", loot_array);
        
        response_root.put("gameTime", game_->GetGameTime());
        
        std::stringstream ss;
        pt::write_json(ss, response_root);
        response_.body() = ss.str();
        response_.result(http::status::ok);
    }
    
    void HandleTick() {
        try {
            std::string body = request_.body();
            pt::ptree root;
            std::stringstream ss(body);
            pt::read_json(ss, root);
            
            int time_delta = root.get<int>("timeDelta", 0);
            
            if (time_delta > 0) {
                game_->Tick(app::milliseconds(time_delta));
            }
            
            response_.result(http::status::ok);
            response_.body() = R"({"status":"ok"})";
            
        } catch (const std::exception& e) {
            response_.result(http::status::bad_request);
            response_.body() = R"({"error":")" + std::string(e.what()) + R"("})";
        }
    }
    
    void HandleGetPlayers() {
        pt::ptree response_root;
        pt::ptree players_array;
        
        for (const auto& player_pair : game_->GetPlayers()) {
            const auto& player = player_pair.second;
            if (player.dog) {
                pt::ptree player_node;
                player_node.put("token", player.token);
                player_node.put("userId", player.user_id);
                player_node.put("mapId", player.map_id);
                player_node.put("dogId", *player.dog->GetId());
                player_node.put("score", player.dog->GetScore());
                
                pt::ptree pos_node;
                pos_node.put("x", player.dog->GetPosition().x);
                pos_node.put("y", player.dog->GetPosition().y);
                player_node.add_child("position", pos_node);
                
                players_array.push_back(std::make_pair("", player_node));
            }
        }
        
        response_root.add_child("players", players_array);
        
        std::stringstream ss;
        pt::write_json(ss, response_root);
        response_.body() = ss.str();
        response_.result(http::status::ok);
    }
    
    void WriteResponse() {
        auto self = shared_from_this();
        http::async_write(socket_, response_,
            [self](beast::error_code ec, size_t) {
                self->socket_.shutdown(net::ip::tcp::socket::shutdown_send, ec);
            });
    }
    
    net::ip::tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    http::response<http::string_body> response_;
    std::shared_ptr<model::Game> game_;
};

// Функция для загрузки карт из конфигурационного файла
void LoadMapsFromConfig(const std::string& config_path, std::shared_ptr<model::Game> game) {
    try {
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Config file not found: " << config_path << std::endl;
            if (!game->HasMap("map1")) {
                game->AddMap("map1", 3.0);
                std::cout << "Created default map1" << std::endl;
            }
            return;
        }
        
        pt::ptree root;
        pt::read_json(config_path, root);
        
        // Загружаем настройки генерации лута
        double loot_period = root.get<double>("lootGeneratorConfig.period", 5.0);
        double loot_probability = root.get<double>("lootGeneratorConfig.probability", 0.5);
        double default_dog_speed = root.get<double>("defaultDogSpeed", 3.0);
        
        // Парсим карты
        if (root.count("maps") > 0) {
            for (const auto& map_node : root.get_child("maps")) {
                const auto& map_data = map_node.second;
                
                std::string map_id = map_data.get<std::string>("id");
                std::string map_name = map_data.get<std::string>("name", map_id);
                double dog_speed = map_data.get<double>("dogSpeed", default_dog_speed);
                
                std::cout << "Loading map: " << map_id << " (" << map_name << ")" << std::endl;
                
                // Загружаем карту через метод Game
                game->LoadMapFromConfig(map_data);
                
                // Устанавливаем настройки генерации лута
                auto map_state = game->GetMapState(map_id);
                if (map_state) {
                    map_state->loot_period_ms = static_cast<uint64_t>(loot_period * 1000);
                    map_state->loot_probability = loot_probability;
                }
            }
        }
        
       