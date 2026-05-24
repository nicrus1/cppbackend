#include "my_logger.h"

#include <string_view>
#include <thread>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace fs = std::filesystem;
using tcp = net::ip::tcp;

using namespace std::literals;

// Функция для URL-декодирования
std::string UrlDecode(std::string_view encoded) {
    std::string result;
    result.reserve(encoded.size());
    
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            char hex[3] = {encoded[i + 1], encoded[i + 2], '\0'};
            char* endptr;
            long value = std::strtol(hex, &endptr, 16);
            if (endptr == hex + 2) {
                result.push_back(static_cast<char>(value));
                i += 2;
                continue;
            }
        } else if (encoded[i] == '+') {
            result.push_back(' ');
            continue;
        }
        result.push_back(encoded[i]);
    }
    
    return result;
}

// Функция для определения MIME-типа по расширению файла
std::string GetMimeType(const fs::path& filepath) {
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".htm", "text/html"},
        {".html", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpe", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"},
    };
    
    std::string ext = filepath.extension().string();
    // Приводим расширение к нижнему регистру
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    auto it = mime_types.find(ext);
    if (it != mime_types.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}

// Проверка, находится ли путь внутри корневого каталога
bool IsSubPath(const fs::path& path, const fs::path& base) {
    try {
        auto canonical_path = fs::weakly_canonical(path);
        auto canonical_base = fs::weakly_canonical(base);
        
        auto b = canonical_base.begin();
        auto p = canonical_path.begin();
        
        for (; b != canonical_base.end(); ++b, ++p) {
            if (p == canonical_path.end() || *p != *b) {
                return false;
            }
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Обработчик статических файлов
http::response<http::file_body> HandleStaticFile(const std::string& target, const fs::path& static_root) {
    http::response<http::file_body> response;
    
    try {
        // Декодируем URL
        std::string decoded_path = UrlDecode(target);
        
        // Удаляем начальный слеш, если есть
        if (!decoded_path.empty() && decoded_path[0] == '/') {
            decoded_path = decoded_path.substr(1);
        }
        
        // Если путь пустой, используем index.html
        if (decoded_path.empty()) {
            decoded_path = "index.html";
        }
        
        // Строим полный путь к файлу
        fs::path file_path = static_root / decoded_path;
        
        // Если путь указывает на директорию, добавляем index.html
        if (fs::exists(file_path) && fs::is_directory(file_path)) {
            file_path /= "index.html";
        }
        
        // Проверяем, что путь находится внутри корневого каталога
        if (!IsSubPath(file_path, static_root)) {
            // Для ошибок возвращаем string_body, но функция обещает file_body
            // Создаем response с string_body для ошибки
            http::response<http::string_body> error_response;
            error_response.result(http::status::bad_request);
            error_response.set(http::field::content_type, "text/plain");
            error_response.body() = "Bad Request: Path outside root directory";
            error_response.prepare_payload();
            
            // Конвертируем в file_body (не идеально, но для простоты)
            http::response<http::file_body> converted;
            converted.result(error_response.result());
            converted.set(http::field::content_type, "text/plain");
            // Не можем установить тело, поэтому просто возвращаем
            converted.prepare_payload();
            return converted;
        }
        
        // Проверяем существование файла
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            http::response<http::string_body> error_response;
            error_response.result(http::status::not_found);
            error_response.set(http::field::content_type, "text/plain");
            error_response.body() = "404 Not Found";
            error_response.prepare_payload();
            
            http::response<http::file_body> converted;
            converted.result(error_response.result());
            converted.set(http::field::content_type, "text/plain");
            converted.prepare_payload();
            return converted;
        }
        
        // Открываем файл
        http::file_body::value_type file;
        boost::system::error_code ec;
        file.open(file_path.string().c_str(), beast::file_mode::read, ec);
        
        if (ec) {
            http::response<http::string_body> error_response;
            error_response.result(http::status::internal_server_error);
            error_response.set(http::field::content_type, "text/plain");
            error_response.body() = "Internal Server Error: Cannot open file";
            error_response.prepare_payload();
            
            http::response<http::file_body> converted;
            converted.result(error_response.result());
            converted.set(http::field::content_type, "text/plain");
            converted.prepare_payload();
            return converted;
        }
        
        // Формируем успешный ответ
        response.result(http::status::ok);
        response.set(http::field::content_type, GetMimeType(file_path));
        response.body() = std::move(file);
        response.prepare_payload();
        
    } catch (const std::exception& e) {
        response.result(http::status::internal_server_error);
        response.set(http::field::content_type, "text/plain");
        // Для file_body нельзя присвоить строку, поэтому просто создаем пустой ответ
        response.prepare_payload();
    }
    
    return response;
}

// Обработчик API (заглушка)
http::response<http::string_body> HandleApiRequest(const std::string& target) {
    http::response<http::string_body> response;
    response.set(http::field::content_type, "application/json");
    
    if (target == "/api/v1/maps") {
        response.result(http::status::ok);
        response.body() = R"({"maps": []})";
    } else if (target.find("/api/v1/maps/") == 0) {
        response.result(http::status::ok);
        response.body() = R"({"map": {"id": "1", "name": "Test Map"}})";
    } else {
        response.result(http::status::not_found);
        response.body() = R"({"error": "Not found"})";
    }
    
    response.prepare_payload();
    return response;
}

// Вспомогательная функция для отправки ответа с ошибкой
void SendErrorResponse(tcp::socket& socket, http::status status, const std::string& message) {
    http::response<http::string_body> response;
    response.result(status);
    response.set(http::field::content_type, "text/plain");
    response.body() = message;
    response.prepare_payload();
    
    beast::error_code ec;
    http::write(socket, response, ec);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config.json> <static_dir>" << std::endl;
        return EXIT_FAILURE;
    }
    
    std::string config_path = argv[1];
    fs::path static_root = argv[2];
    
    // Проверяем существование статической директории
    if (!fs::exists(static_root) || !fs::is_directory(static_root)) {
        std::cerr << "Error: Static directory does not exist: " << static_root << std::endl;
        return EXIT_FAILURE;
    }
    
    // Инициализация логгера
    Logger::GetInstance().SetTimestamp(std::chrono::system_clock::now());
    LOG("Server starting with config: ", config_path, ", static root: ", static_root.string());
    
    try {
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, tcp::endpoint(tcp::v4(), 8080)};
        
        std::cout << "Server has started..." << std::endl;
        LOG("Server listening on port 8080");
        
        while (true) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            
            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            beast::error_code ec;
            
            http::read(socket, buffer, req, ec);
            if (ec) {
                LOG("Error reading request: ", ec.message());
                continue;
            }
            
            std::string target(req.target());
            LOG("Received request: ", req.method_string(), " ", target);
            
            // Обработка запросов
            if (target.find("/api/") == 0) {
                // API запрос
                auto response = HandleApiRequest(target);
                http::write(socket, response, ec);
                if (ec) {
                    LOG("Error writing response: ", ec.message());
                }
            } else {
                // Статический файл
                // Сначала проверяем с string_body, чтобы корректно обработать ошибки
                try {
                    std::string decoded_path = UrlDecode(target);
                    if (!decoded_path.empty() && decoded_path[0] == '/') {
                        decoded_path = decoded_path.substr(1);
                    }
                    if (decoded_path.empty()) {
                        decoded_path = "index.html";
                    }
                    
                    fs::path file_path = static_root / decoded_path;
                    if (fs::exists(file_path) && fs::is_directory(file_path)) {
                        file_path /= "index.html";
                    }
                    
                    if (!IsSubPath(file_path, static_root)) {
                        SendErrorResponse(socket, http::status::bad_request, 
                                        "Bad Request: Path outside root directory");
                        continue;
                    }
                    
                    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
                        SendErrorResponse(socket, http::status::not_found, "404 Not Found");
                        continue;
                    }
                    
                    // Открываем и отправляем файл
                    http::response<http::file_body> response;
                    response.result(http::status::ok);
                    response.set(http::field::content_type, GetMimeType(file_path));
                    
                    http::file_body::value_type file;
                    boost::system::error_code file_ec;
                    file.open(file_path.string().c_str(), beast::file_mode::read, file_ec);
                    
                    if (file_ec) {
                        SendErrorResponse(socket, http::status::internal_server_error, 
                                        "Internal Server Error: Cannot open file");
                        continue;
                    }
                    
                    response.body() = std::move(file);
                    response.prepare_payload();
                    http::write(socket, response, ec);
                    
                } catch (const std::exception& e) {
                    SendErrorResponse(socket, http::status::internal_server_error, 
                                    "Internal Server Error: "s + e.what());
                }
            }
            
            socket.shutdown(tcp::socket::shutdown_send, ec);
        }
        
    } catch (const std::exception& e) {
        LOG("Server error: ", e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}