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

// URL декодирование
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

// MIME типы
std::string getMimeType(const std::string& path) {
    static const std::unordered_map<std::string, std::string> mime = {
        {".htm", "text/html"}, {".html", "text/html"},
        {".css", "text/css"}, {".js", "text/javascript"},
        {".json", "application/json"}, {".xml", "application/xml"},
        {".png", "image/png"}, {".jpg", "image/jpeg"},
        {".jpe", "image/jpeg"}, {".jpeg", "image/jpeg"},
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

// Чтение файла
std::string readFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Обработка статических файлов
bool handleStaticFile(const std::string& method, const std::string& uri,
                      const fs::path& static_dir, std::string& response) {
    
    if (method != "GET" && method != "HEAD") return false;
    
    std::string decoded = urlDecode(uri);
    size_t query_pos = decoded.find('?');
    if (query_pos != std::string::npos) decoded = decoded.substr(0, query_pos);
    size_t frag_pos = decoded.find('#');
    if (frag_pos != std::string::npos) decoded = decoded.substr(0, frag_pos);
    
    if (decoded.empty() || decoded == "/") {
        decoded = "/index.html";
    }
    
    fs::path file_path = static_dir / decoded.substr(1);
    
    // Безопасность
    try {
        fs::path canonical_static = fs::canonical(static_dir);
        fs::path canonical_file = fs::canonical(file_path);
        
        if (canonical_file.string().find(canonical_static.string()) != 0) {
            response = "HTTP/1.1 400 Bad Request\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: 11\r\n"
                       "Connection: close\r\n\r\n"
                       "Bad Request";
            return true;
        }
    } catch (const fs::filesystem_error&) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\n"
                   "Not Found";
        return true;
    }
    
    if (fs::is_directory(file_path)) {
        file_path /= "index.html";
    }
    
    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\n"
                   "Not Found";
        return true;
    }
    
    std::string content = readFile(file_path);
    if (content.empty()) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\n"
                   "Not Found";
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

// Создание ответа со списком карт
std::string createMapsListResponse(const std::string& config) {
    std::string result = "[";
    bool first = true;
    
    // Ищем каждый объект карты
    size_t search_pos = 0;
    while (true) {
        // Ищем начало объекта карты
        size_t obj_start = config.find("\"id\"", search_pos);
        if (obj_start == std::string::npos) break;
        
        // Находим начало объекта {
        while (obj_start > 0 && config[obj_start] != '{') {
            obj_start--;
        }
        if (config[obj_start] != '{') {
            search_pos = obj_start + 1;
            continue;
        }
        
        // Находим конец объекта
        size_t obj_end = obj_start;
        int brace_count = 0;
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
        
        std::string map_obj = config.substr(obj_start, obj_end - obj_start + 1);
        
        // Извлекаем id и name
        std::string id, name;
        
        size_t id_start = map_obj.find("\"id\"");
        if (id_start != std::string::npos) {
            id_start = map_obj.find("\"", id_start + 4);
            if (id_start != std::string::npos) {
                size_t id_end = map_obj.find("\"", id_start + 1);
                if (id_end != std::string::npos) {
                    id = map_obj.substr(id_start + 1, id_end - id_start - 1);
                }
            }
        }
        
        size_t name_start = map_obj.find("\"name\"");
        if (name_start != std::string::npos) {
            name_start = map_obj.find("\"", name_start + 6);
            if (name_start != std::string::npos) {
                size_t name_end = map_obj.find("\"", name_start + 1);
                if (name_end != std::string::npos) {
                    name = map_obj.substr(name_start + 1, name_end - name_start - 1);
                }
            }
        }
        
        if (!id.empty() && !name.empty()) {
            if (!first) result += ",";
            first = false;
            result += "{\"id\":\"" + id + "\",\"name\":\"" + name + "\"}";
        }
        
        search_pos = obj_end + 1;
    }
    
    result += "]";
    return result;
}

// Поиск карты по ID и возврат полного JSON объекта
std::string findMapById(const std::string& config, const std::string& map_id) {
    size_t search_pos = 0;
    
    while (true) {
        // Ищем "id": "xxx"
        std::string search_pattern = "\"id\":\"" + map_id + "\"";
        size_t id_pos = config.find(search_pattern, search_pos);
        if (id_pos == std::string::npos) break;
        
        // Находим начало объекта
        size_t obj_start = id_pos;
        while (obj_start > 0 && config[obj_start] != '{') {
            obj_start--;
        }
        if (config[obj_start] != '{') {
            search_pos = id_pos + 1;
            continue;
        }
        
        // Находим конец объекта
        size_t obj_end = obj_start;
        int brace_count = 0;
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
        
        return config.substr(obj_start, obj_end - obj_start + 1);
    }
    
    return "";
}

// Формирование JSON ошибки
std::string errorJson(const std::string& code, const std::string& message) {
    return "{\"code\":\"" + code + "\",\"message\":\"" + message + "\"}";
}

// Обработка API
bool handleApiRequest(const std::string& method, const std::string& uri,
                      const fs::path& config_path, std::string& response) {
    
    // Проверяем метод
    if (method != "GET") {
        std::string err = errorJson("METHOD_NOT_ALLOWED", "Only GET method is allowed");
        response = "HTTP/1.1 405 Method Not Allowed\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(err.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + err;
        return true;
    }
    
    // Проверяем что URI начинается с /api/v1/
    if (uri.find("/api/v1/") != 0) {
        std::string err = errorJson("BAD_REQUEST", "Invalid API path");
        response = "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(err.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + err;
        return true;
    }
    
    // Загружаем конфиг
    std::string config_content = readFile(config_path);
    if (config_content.empty()) {
        std::string err = errorJson("INTERNAL_ERROR", "Failed to load config");
        response = "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(err.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + err;
        return true;
    }
    
    // GET /api/v1/maps
    if (uri == "/api/v1/maps") {
        std::string maps_list = createMapsListResponse(config_content);
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(maps_list.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + maps_list;
        return true;
    }
    
    // GET /api/v1/maps/{id}
    if (uri.find("/api/v1/maps/") == 0) {
        std::string map_id = uri.substr(14);
        
        if (map_id.empty()) {
            std::string err = errorJson("BAD_REQUEST", "Map ID is empty");
            response = "HTTP/1.1 400 Bad Request\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(err.size()) + "\r\n"
                       "Connection: close\r\n\r\n" + err;
            return true;
        }
        
        std::string map_content = findMapById(config_content, map_id);
        
        if (map_content.empty()) {
            std::string err = errorJson("MAP_NOT_FOUND", "Map not found");
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
    
    // Неизвестный путь
    std::string err = errorJson("NOT_FOUND", "Endpoint not found");
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