#pragma once
#include "http_server.h"
#include "logger.h"
#include "model.h"

#include <filesystem>
#include <boost/regex.hpp>
#include <boost/json.hpp>
#include <variant>
#include <optional>
#include <cassert>
#include <memory>
#include <utility>
#include <cctype>
#include <algorithm>

namespace http_handler
{
namespace net = boost::asio;
namespace beast = boost::beast;
namespace sys = boost::system;
namespace http = beast::http;
namespace fs = std::filesystem;

using namespace std::literals;

using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using StringRequest = http::request<http::string_body>;

class Endpoints
{
public:
	inline static const auto api = "/api/"s;
	inline static const auto maps = "/api/v1/maps"s;
	inline static const auto join_game = "/api/v1/game/join"s;
	inline static const auto get_players = "/api/v1/game/players"s;
	inline static const auto map_id = boost::regex(R"(/api/v1/maps/([a-zA-Z0-9_]+))");
};

template <class ResponseType>
inline ResponseType MakeHeader(http::status status, const StringRequest& req, const std::string& content_type)
{
	ResponseType resp(status, req.version());
	resp.set(http::field::content_type, content_type);
	return resp;
}

template <class BoostString>
inline std::string ToStdString(BoostString str)
{
	return std::string{ str.begin(), str.end() };
}

class API
{
public:
	struct PlayerInfo
	{
		model::Token auth_token;
		size_t player_id;
	};

	explicit API(model::Game& game, model::Players& players);
	std::optional<PlayerInfo> JoinGame(const std::string& username, model::Map::Id map_id);
	std::optional<std::vector<std::pair<size_t, std::string>>> GetPlayerList(model::Token token) const;
	const model::Game::Maps& GetMaps() const;
	const model::Map* FindMap(model::Map::Id map_id) const;
	const model::Player* FindPlayer(model::Token token) const;

private:
	model::Game& game_;
	model::Players& players_;
};

class APIRequestHandler
{
public:
	explicit APIRequestHandler(model::Game& game, model::Players& players);

	APIRequestHandler(const APIRequestHandler&) = delete;
	APIRequestHandler& operator=(const APIRequestHandler&) = delete;
	APIRequestHandler(APIRequestHandler&&) = default;

	template <typename Body, typename Allocator, typename Send>
	void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send)
	{
		StringResponse response;
		auto target = std::string(req.target());
		if (!target.empty() && target.front() != '/')
		{
			target.insert(target.begin(), '/');
		}
		boost::smatch match;

		if (target == Endpoints::maps)
		{
			response = HandleMapRequest(req);
		}
		else if (boost::regex_match(target, match, Endpoints::map_id))
		{
			response = HandleMapIdRequest(req, model::Map::Id(match[1].str()));
		}
		else if (target == Endpoints::join_game)
		{
			response = HandleJoinGame(req);
		}
		else if (target == Endpoints::get_players)
		{
			response = HandlePlayerList(req);
		}
		else
		{
			response = HandleJsonBadRequest(req);
		}

		send(std::move(response));
	}

private:
	// Helper function to extract Bearer token from Authorization header
	template <typename Request>
	std::optional<model::Token> ExtractToken(const Request& req) const
	{
		// Find Authorization header
		auto auth_it = req.find(http::field::authorization);
		if (auth_it == req.end())
		{
			return std::nullopt;
		}
		
		std::string auth_value = std::string(auth_it->value());
		
		// Trim whitespace
		size_t start = auth_value.find_first_not_of(" \t\n\r");
		if (start == std::string::npos)
		{
			return std::nullopt;
		}
		size_t end = auth_value.find_last_not_of(" \t\n\r");
		auth_value = auth_value.substr(start, end - start + 1);
		
		// Check for Bearer prefix (case-insensitive)
		std::string auth_lower = auth_value;
		std::transform(auth_lower.begin(), auth_lower.end(), auth_lower.begin(),
		               [](unsigned char c) { return std::tolower(c); });
		
		const std::string prefix = "bearer ";
		if (auth_lower.size() < prefix.size() ||
		    auth_lower.substr(0, prefix.size()) != prefix)
		{
			return std::nullopt;
		}
		
		// Extract token
		std::string token_str = auth_value.substr(prefix.size());
		
		// Trim token
		start = token_str.find_first_not_of(" \t\n\r");
		if (start == std::string::npos)
		{
			return std::nullopt;
		}
		end = token_str.find_last_not_of(" \t\n\r");
		token_str = token_str.substr(start, end - start + 1);
		
		if (token_str.empty())
		{
			return std::nullopt;
		}
		
		return model::Token{std::move(token_str)};
	}

	//API maps handler
	StringResponse HandleMapRequest(const StringRequest& req) const;
	StringResponse HandleMapIdRequest(const StringRequest& req, model::Map::Id map_id) const;
	StringResponse HandleJsonBadRequest(const StringRequest& req) const;
	void ParseMapArray(boost::json::array& map_arr) const;
	void ParseRoads(const model::Map* map, boost::json::array& roads_arr) const;
	void ParseBuildings(const model::Map* map, boost::json::array& buildings_arr) const;
	void ParseOffices(const model::Map* map, boost::json::array& offices_arr) const;

	//API join handler
	StringResponse MakeJsonBasedResponse(http::status status, const StringRequest& req, json::object response_obj);
	StringResponse HandleJoinGame(const StringRequest& req);
	StringResponse HandleEmptyName(const StringRequest& req);
	StringResponse HandleParsingError(const StringRequest& req);
	StringResponse HandleMapNotFound(const StringRequest& req);
	StringResponse HandleJoinMethodNotAllowed(const StringRequest& req);

	//API player list handler
	StringResponse HandlePlayerList(const StringRequest& req);
	StringResponse HandleInvalidToken(const StringRequest& req);
	StringResponse HandleUnknownToken(const StringRequest& req);
	StringResponse HandlePlayerListMethodNotAllowed(const StringRequest& req);
	void ParsePlayerList(boost::json::object& player_list_obj, const std::vector<std::pair<size_t, std::string>>& player_list);

	API api_;
};

class StaticRequestHandler
{
public:
	explicit StaticRequestHandler(const fs::path& to_static_folder);

	StaticRequestHandler(const StaticRequestHandler&) = delete;
	StaticRequestHandler& operator=(const StaticRequestHandler&) = delete;
	StaticRequestHandler(StaticRequestHandler&&) = default;

	template <typename Body, typename Allocator, typename Send>
	void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send)
	{
		std::variant<StringResponse, FileResponse> response;
		response = HandleFileRequest(req);

		if (std::holds_alternative<StringResponse>(response))
		{
			send(std::move(std::get<StringResponse>(response)));
		}
		else
		{
			send(std::move(std::get<FileResponse>(response)));
		}
	}

private:
	static std::string DefineContentType(const fs::path& c_target);
	StringResponse HandleFileBadRequest(const StringRequest& req) const;
	StringResponse HandleNotFound(const StringRequest& req) const;
	std::variant<StringResponse, FileResponse> HandleFileRequest(const StringRequest& req) const;
	bool IsSubPath(fs::path path) const;

	fs::path to_static_folder_;
};

class RequestHandler : public std::enable_shared_from_this<RequestHandler>
{
public:
	using Strand = net::strand<net::io_context::executor_type>;

	explicit RequestHandler(model::Game& game, model::Players& players, const fs::path& to_static_folder, Strand api_strand);

	RequestHandler(const RequestHandler&) = delete;
	RequestHandler& operator=(const RequestHandler&) = delete;
	RequestHandler(RequestHandler&&) = delete;

	template <typename Body, typename Allocator, typename Send>
	void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send)
	{
		auto target = std::string(req.target());

		if (target.substr(0, 5) == Endpoints::api || target.substr(0, 4) == "api/")
		{
			auto self = shared_from_this();
			auto handle = [self, req = std::move(req), send = std::forward<Send>(send)]() mutable
			{
				assert(self->api_strand_.running_in_this_thread());
				self->api_handler_(std::move(req), std::move(send));
			};
			return net::dispatch(api_strand_, std::move(handle));
		}
		else
		{
			auto method = req.method();

			if (method != http::verb::get && method != http::verb::post && method != http::verb::head)
			{
				auto response = HandleNotAllowed(req);
				send(std::move(response));
				return;
			}

			static_handler_(std::move(req), std::move(send));
		}
	}

private:
	StringResponse HandleNotAllowed(const StringRequest& req);

	APIRequestHandler api_handler_;
	StaticRequestHandler static_handler_;
	Strand api_strand_;
};

template <class SomeRequestHandler>
class LoggingRequestHandler
{
public:
	LoggingRequestHandler() = delete;
	LoggingRequestHandler(const SomeRequestHandler&) = delete;
	LoggingRequestHandler(SomeRequestHandler&& handler)
		: decorated_(std::move(handler))
	{
	}

	template <typename Body, typename Allocator, typename Send>
	void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, const std::string& client_ip)
	{
		LogRequest(req, client_ip);

		auto start_time = std::chrono::steady_clock::now();

		auto log_send = [send = std::forward<Send>(send), start_time](auto&& response) mutable
		{
			LogResponse(response, start_time);
			send(std::move(response));
		};
		decorated_(std::move(req), log_send);
	};

private:
	template <class Request>
	static void LogRequest(const Request& r, const std::string& client_ip)
	{
		json::object data;
		data["ip"] = client_ip;
		data["URI"] = std::string(r.target());
		data["method"] = std::string(r.method_string());

		BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "request received";
	}

	template <class Response>
	static void LogResponse(const Response& r, std::chrono::steady_clock::time_point start_time)
	{
		auto end_time = std::chrono::steady_clock::now();
		auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

		json::object data;
		data["response_time"] = static_cast<int64_t>(response_time);
		data["code"] = static_cast<int>(r.result_int());

		auto it = r.find(http::field::content_type);
		if (it != r.end())
		{
			data["content_type"] = std::string(it->value());
		}
		else
		{
			data["content_type"] = nullptr;
		}

		BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "response sent";
	}

	SomeRequestHandler decorated_;
};

}  // namespace http_handler