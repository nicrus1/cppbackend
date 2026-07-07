#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <filesystem>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;