#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace fs = std::filesystem;

std::string urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream iss(str.substr(i + 1, 2));
            iss >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string getMimeType(const std::string& path) {
    static const std::unordered_map<std::string, std::string> mime = {
        {".html", "text/html"}, {".htm", "text/html"},
        {".css", "text/css"}, {".js", "text/javascript"},
        {".json", "application/json"}, {".xml", "application/xml"},
        {".png", "image/png"}, {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"}, {".jpe", "image/jpeg"},
        {".gif", "image/gif"}, {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"}, {".tif", "image/tiff"},
        {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"}, {".txt", "text/plain"}
    };
    
    std::string ext;
    size_t dot_pos = path.rfind('.');
    if (dot_pos != std::string::npos) {
        ext = path.substr(dot_pos);
        for (char& c : ext) c = std::tolower(c);
    }
    
    auto it = mime.find(ext);
    return it != mime.end() ? it->second : "application/octet-stream";
}

std::string readFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Поиск карты по ID - возвращает полный JSON объекта
std::string findMapByID(const std::string& config, const std::string& map_id) {
    // Ищем "id":"map1"
    std::string pattern = "\"id\":\"" + map_id + "\"";
    size_t pos = config.find(pattern);
    if (pos == std::string::npos) {
        // Пробуем с пробелом: "id": "map1"
        pattern = "\"id\": \"" + map_id + "\"";
        pos = config.find(pattern);
    }
    if (pos == std::string::npos) return "";
    
    // Ищем начало объекта {
    size_t start = pos;
    while (start > 0 && config[start] != '{') {
        start--;
    }
    if (config[start] != '{') return "";
    
    // Ищем конец объекта }
    int brace_count = 0;
    size_t end = start;
    for (size_t i = start; i < config.length(); ++i) {
        if (config[i] == '{') brace_count++;
        else if (config[i] == '}') {
            brace_count--;
            if (brace_count == 0) {
                end = i;
                break;
            }
        }
    }
    
    return config.substr(start, end - start + 1);
}

// Получение списка карт
std::string getMapsList(const std::string& config) {
    std::string result = "[";
    bool first = true;
    
    size_t pos = 0;
    while (true) {
        size_t obj_start = config.find("{", pos);
        if (obj_start == std::string::npos) break;
        
        int brace_count = 0;
        size_t obj_end = obj_start;
        for (size_t i = obj_start; i < config.length(); ++i) {
            if (config[i] == '{') brace_count++;
            else if (config[i] == '}') {
                brace_count--;
                if (brace_count == 0) {
                    obj_end = i;
                    break;
                }
            }
        }
        
        std::string obj = config.substr(obj_start, obj_end - obj_start + 1);
        
        // Извлекаем id
        std::string id;
        size_t id_pos = obj.find("\"id\"");
        if (id_pos != std::string::npos) {
            size_t colon = obj.find(":", id_pos);
            size_t q1 = obj.find("\"", colon);
            if (q1 != std::string::npos) {
                size_t q2 = obj.find("\"", q1 + 1);
                if (q2 != std::string::npos) {
                    id = obj.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }
        
        // Извлекаем name
        std::string name;
        size_t name_pos = obj.find("\"name\"");
        if (name_pos != std::string::npos) {
            size_t colon = obj.find(":", name_pos);
            size_t q1 = obj.find("\"", colon);
            if (q1 != std::string::npos) {
                size_t q2 = obj.find("\"", q1 + 1);
                if (q2 != std::string::npos) {
                    name = obj.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }
        
        if (!id.empty() && !name.empty()) {
            if (!first) result += ",";
            first = false;
            result += "{\"id\":\"" + id + "\",\"name\":\"" + name + "\"}";
        }
        
        pos = obj_end + 1;
    }
    
    result += "]";
    return result;
}

std::string errorJson(const std::string& code, const std::string& message) {
    return "{\"code\":\"" + code + "\",\"message\":\"" + message + "\"}";
}

bool handleStaticFile(const std::string& method, const std::string& uri,
                      const fs::path& static_dir, std::string& response) {
    
    if (method != "GET" && method != "HEAD") return false;
    
    std::string decoded = urlDecode(uri);
    size_t query_pos = decoded.find('?');
    if (query_pos != std::string::npos) decoded = decoded.substr(0, query_pos);
    
    if (decoded == "/") decoded = "/index.html";
    
    fs::path file_path = static_dir / decoded.substr(1);
    
    try {
        fs::path canonical_static = fs::canonical(static_dir);
        fs::path canonical_file = fs::canonical(file_path);
        if (canonical_file.string().find(canonical_static.string()) != 0) {
            response = "HTTP/1.1 400 Bad Request\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: 11\r\n"
                       "Connection: close\r\n\r\nBad Request";
            return true;
        }
    } catch (...) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\nNot Found";
        return true;
    }
    
    if (fs::is_directory(file_path)) {
        file_path /= "index.html";
    }
    
    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\nNot Found";
        return true;
    }
    
    std::string content = readFile(file_path);
    if (content.empty()) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\nNot Found";
        return true;
    }
    
    std::stringstream resp;
    resp << "HTTP/1.1 200 OK\r\n";
    resp << "Content-Type: " << getMimeType(file_path.string()) << "\r\n";
    resp << "Content-Length: " << content.size() << "\r\n";
    resp << "Connection: close\r\n";
    resp << "\r\n";
    
    if (method == "GET") {
        resp << content;
    }
    
    response = resp.str();
    return true;
}

bool handleApiRequest(const std::string& method, const std::string& uri,
                      const fs::path& config_path, std::string& response) {
    
    if (uri.find("/api/v1/") != 0) {
        std::string err = errorJson("badRequest", "Invalid API path");
        response = "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(err.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + err;
        return true;
    }
    
    if (method != "GET") {
        std::string err = errorJson("methodNotAllowed", "Only GET method is allowed");
        response = "HTTP/1.1 405 Method Not Allowed\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(err.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + err;
        return true;
    }
    
    std::string config = readFile(config_path);
    if (config.empty()) {
        std::string err = errorJson("internalError", "Failed to load config");
        response = "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(err.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + err;
        return true;
    }
    
    if (uri == "/api/v1/maps") {
        std::string maps_list = getMapsList(config);
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(maps_list.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + maps_list;
        return true;
    }
    
    if (uri.find("/api/v1/maps/") == 0) {
        std::string map_id = uri.substr(14);
        
        if (map_id.empty()) {
            std::string err = errorJson("badRequest", "Map ID is empty");
            response = "HTTP/1.1 400 Bad Request\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(err.size()) + "\r\n"
                       "Connection: close\r\n\r\n" + err;
            return true;
        }
        
        std::string map_content = findMapByID(config, map_id);
        
        if (map_content.empty()) {
            std::string err = errorJson("mapNotFound", "Map not found");
            response = "HTTP/1.1 404 Not Found\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(err.size()) + "\r\n"
                       "Connection: close\r\n\r\n" + err;
        } else {
            response = "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(map_content.size()) + "\r\n"
                       "Connection: close\r\n\r\n" + map_content;
        }
        return true;
    }
    
    std::string err = errorJson("notFound", "Endpoint not found");
    response = "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(err.size()) + "\r\n"
               "Connection: close\r\n\r\n" + err;
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <static_dir>" << std::endl;
        return 1;
    }
    
    fs::path config_path = argv[1];
    fs::path static_dir = argv[2];
    
    // Проверяем существование файла конфига
    if (!fs::exists(config_path)) {
        std::cerr << "Config file not found: " << config_path << std::endl;
        return 1;
    }
    
    if (!fs::exists(static_dir)) {
        std::cerr << "Static directory not found: " << static_dir << std::endl;
        return 1;
    }
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Listen failed" << std::endl;
        close(server_fd);
        return 1;
    }
    
    std::cout << "Server has started..." << std::endl;
    
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) continue;
        
        char buffer[65536] = {0};
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read <= 0) {
            close(client_fd);
            continue;
        }
        
        std::string request(buffer);
        std::istringstream request_stream(request);
        
        std::string method, uri, version;
        request_stream >> method >> uri >> version;
        
        std::string response;
        
        if (uri.find("/api/") == 0) {
            handleApiRequest(method, uri, config_path, response);
        } else {
            handleStaticFile(method, uri, static_dir, response);
        }
        
        send(client_fd, response.c_str(), response.size(), 0);
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}