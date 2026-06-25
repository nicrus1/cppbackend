#pragma once
#include "model.h"
#include "game_state.h"
#include "http_server.h"
#include "logger.h"
#include "extra_data.h"
#include <boost/json.hpp>
#include <optional>
#include <string>
#include <functional>
#include <cctype>
#include <algorithm>
#include <memory>
#include <chrono>
#include <tuple>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;

class ApiHandler {
public:
    explicit ApiHandler(model::Game& game) 
        : game_state_(std::make_unique<game::GameState>(game)) {}

    void Tick(std::chrono::milliseconds delta) {
        game_state_->ProcessTick(delta.count());
    }
    
    void SetLootGeneratorConfig(double period, double probability) {
        game_state_->SetLootGeneratorConfig(period, probability);
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::post) {
            SendErrorWithAllow(std::move(req), send, http::status::method_not_allowed,
                               "invalidMethod", "Only POST method is expected", "POST");
            return;
        }

        auto content_type = req.find(http::field::content_type);
        if (content_type == req.end() || content_type->value() != "application/json") {
            SendError(std::move(req), send, http::status::bad_request,
                      "invalidArgument", "Invalid Content-Type");
            return;
        }

        try {
            auto obj = boost::json::parse(req.body()).as_object();
            if (!obj.contains("userName") || !obj.contains("mapId")) {
                SendError(std::move(req), send, http::status::bad_request,
                          "invalidArgument", "Join game request parses but misses fields");
                return;
            }

            std::string user_name = std::string(obj.at("userName").as_string());
            if (user_name.empty()) {
                SendError(std::move(req), send, http::status::bad_request,
                          "invalidArgument", "Invalid name");
                return;
            }

            std::string map_id_str = std::string(obj.at("mapId").as_string());
            model::Map::Id map_id{map_id_str};

            auto join_result = game_state_->JoinGame(user_name, map_id);
            
            boost::json::object res_obj;
            res_obj["authToken"] = *join_result.token;
            res_obj["playerId"] = *join_result.player_id;

            SendResponse(std::move(req), send, http::status::ok, boost::json::serialize(res_obj));
        } catch (const std::exception& e) {
            logger::LogError(0, "Join game error: " + std::string(e.what()), "HandleJoin");
            SendError(std::move(req), send, http::status::bad_request,
                      "invalidArgument", "Join game request parses but misses fields");
        } catch (...) {
            SendError(std::move(req), send, http::status::bad_request,
                      "invalidArgument", "Join game request parses but misses fields");
        }
    }

    template <typename Body, typename Allocator, typename Send>
    void HandlePlayersList(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            SendErrorWithAllow(std::move(req), send, http::status::method_not_allowed,
                               "invalidMethod", "Invalid method", "GET, HEAD");
            return;
        }

        auto token_opt = ExtractToken(req);
        if (!token_opt) {
            SendError(std::move(req), send, http::status::unauthorized,
                      "invalidToken", "Authorization header is required");
            return;
        }

        if (!game_state_->ValidateToken(*token_opt)) {
            SendError(std::move(req), send, http::status::unauthorized,
                      "unknownToken", "Player token not found");
            return;
        }

        if (req.method() == http::verb::head) {
            SendResponse(std::move(req), send, http::status::ok, "");
            return;
        }

        auto players = game_state_->GetPlayersOnMap(*token_opt);
        boost::json::object res_obj;
        for (const auto& [id_str, name] : players) {
            boost::json::object p_obj;
            p_obj["name"] = name;
            res_obj[id_str] = p_obj;
        }

        SendResponse(std::move(req), send, http::status::ok, boost::json::serialize(res_obj));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleGetPlayers(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        HandlePlayersList(std::move(req), std::forward<Send>(send));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleGameState(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            SendErrorWithAllow(std::move(req), send, http::status::method_not_allowed,
                               "invalidMethod", "Invalid method", "GET, HEAD");
            return;
        }

        auto token_opt = ExtractToken(req);
        if (!token_opt) {
            SendError(std::move(req), send, http::status::unauthorized,
                      "invalidToken", "Authorization header is required");
            return;
        }

        if (!game_state_->ValidateToken(*token_opt)) {
            SendError(std::move(req), send, http::status::unauthorized,
                      "unknownToken", "Player token not found");
            return;
        }

        if (req.method() == http::verb::head) {
            SendResponse(std::move(req), send, http::status::ok, "");
            return;
        }

        auto states = game_state_->GetGameState(*token_opt);
        boost::json::object players_obj;

        for (const auto& ps : states) {
            boost::json::object player_info;
            
            boost::json::array pos_arr;
            pos_arr.push_back(ps.pos.x);
            pos_arr.push_back(ps.pos.y);
            player_info["pos"] = pos_arr;

            boost::json::array speed_arr;
            speed_arr.push_back(ps.speed.vx);
            speed_arr.push_back(ps.speed.vy);
            player_info["speed"] = speed_arr;

            player_info["dir"] = model::DirectionToString(ps.dir);
            
            // Add bag contents
            boost::json::array bag_arr;
            for (const auto& item : ps.bag) {
                boost::json::object bag_item;
                bag_item["id"] = item.id;
                bag_item["type"] = item.type;
                bag_arr.push_back(bag_item);
            }
            player_info["bag"] = bag_arr;
            
            // Add score
            player_info["score"] = ps.score;

            players_obj[ps.player_id] = player_info;
        }
        
        // Add loot objects
        boost::json::object loot_obj;
        auto loot_items = game_state_->GetLootState(*token_opt);
        for (const auto& [id, loot] : loot_items) {
            boost::json::object item_obj;
            item_obj["type"] = std::get<0>(loot);  // type
            boost::json::array pos_arr;
            pos_arr.push_back(std::get<2>(loot).x);  // position.x
            pos_arr.push_back(std::get<2>(loot).y);  // position.y
            item_obj["pos"] = pos_arr;
            loot_obj[std::to_string(id)] = item_obj;
        }

        boost::json::object res_obj;
        res_obj["players"] = players_obj;
        res_obj["lostObjects"] = loot_obj;

        SendResponse(std::move(req), send, http::status::ok, boost::json::serialize(res_obj));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandlePlayerAction(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::post) {
            SendErrorWithAllow(std::move(req), send, http::status::method_not_allowed,
                               "invalidMethod", "Only POST method is expected", "POST");
            return;
        }

        auto content_type = req.find(http::field::content_type);
        if (content_type == req.end() || content_type->value() != "application/json") {
            SendError(std::move(req), send, http::status::bad_request,
                      "invalidArgument", "Invalid Content-Type");
            return;
        }

        auto token_opt = ExtractToken(req);
        if (!token_opt) {
            SendError(std::move(req), send, http::status::unauthorized,
                      "invalidToken", "Authorization header is required");
            return;
        }

        if (!game_state_->ValidateToken(*token_opt)) {
            SendError(std::move(req), send, http::status::unauthorized,
                      "unknownToken", "Player token not found");
            return;
        }

        try {
            auto obj = boost::json::parse(req.body()).as_object();
            if (!obj.contains("move")) {
                SendError(std::move(req), send, http::status::bad_request,
                          "invalidArgument", "Failed to parse action");
                return;
            }

            std::string move_dir = std::string(obj.at("move").as_string());
            if (move_dir.empty()) {
                game_state_->StopDog(*token_opt);
            } else {
                game_state_->SetDogDirection(*token_opt, model::StringToDirection(move_dir));
            }

            boost::json::object res_obj;
            SendResponse(std::move(req), send, http::status::ok, boost::json::serialize(res_obj));
        } catch (...) {
            SendError(std::move(req), send, http::status::bad_request,
                      "invalidArgument", "Failed to parse action");
        }
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleTick(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::post) {
            SendErrorWithAllow(std::move(req), send, http::status::method_not_allowed,
                               "invalidMethod", "Only POST method is expected", "POST");
            return;
        }

        auto content_type = req.find(http::field::content_type);
        if (content_type == req.end() || content_type->value() != "application/json") {
            SendError(std::move(req), send, http::status::bad_request,
                      "invalidArgument", "Invalid Content-Type");
            return;
        }

        try {
            auto obj = boost::json::parse(req.body()).as_object();
            if (!obj.contains("timeDelta") || !obj.at("timeDelta").is_int64()) {
                SendError(std::move(req), send, http::status::bad_request,
                          "invalidArgument", "Failed to parse tick request JSON");
                return;
            }

            int64_t time_delta_ms = obj.at("timeDelta").as_int64();
            if (time_delta_ms <= 0) {
                SendError(std::move(req), send, http::status::bad_request,
                          "invalidArgument", "timeDelta must be positive");
                return;
            }
            
            game_state_->ProcessTick(time_delta_ms);

            boost::json::object res_obj;
            SendResponse(std::move(req), send, http::status::ok, boost::json::serialize(res_obj));
        } catch (const std::exception& e) {
            logger::LogError(0, "Tick error: " + std::string(e.what()), "HandleTick");
            SendError(std::move(req), send, http::status::bad_request,
                      "invalidArgument", "Failed to parse tick request JSON");
        } catch (...) {
            SendError(std::move(req), send, http::status::bad_request,
                      "invalidArgument", "Failed to parse tick request JSON");
        }
    }

private:
    template <typename Body, typename Allocator>
    std::optional<model::Token> ExtractToken(const http::request<Body, http::basic_fields<Allocator>>& req) {
        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return std::nullopt;
        }

        std::string auth_val = std::string(auth_it->value());
        std::string prefix = "Bearer ";
        if (auth_val.size() <= prefix.size() || auth_val.substr(0, prefix.size()) != prefix) {
            return std::nullopt;
        }

        std::string token_str = auth_val.substr(prefix.size());
        token_str.erase(token_str.begin(), std::find_if(token_str.begin(), token_str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        token_str.erase(std::find_if(token_str.rbegin(), token_str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), token_str.end());

        if (token_str.size() != 32) {
            return std::nullopt;
        }

        return model::Token{token_str};
    }

    template <typename Body, typename Allocator, typename Send>
    void SendResponse(http::request<Body, http::basic_fields<Allocator>>&& req,
                      Send&& send,
                      http::status status,
                      std::string_view body) {
        http::response<http::string_body> response(status, req.version());
        response.set(http::field::content_type, "application/json");
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
        SendResponse(std::move(req), send, status, body);
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

private:
    std::unique_ptr<game::GameState> game_state_;
};

} // namespace http_handler