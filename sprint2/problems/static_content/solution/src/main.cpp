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

// MIME типы (полный список по условию)
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

// Парсинг JSON для списка карт
std::string extractMapsList(const std::string& json) {
    std::string result = "[";
    bool first = true;
    
    size_t pos = 0;
    while (true) {
        size_t id_pos = json.find("\"id\"", pos);
        if (id_pos == std::string::npos) break;
        
        size_t name_pos = json.find("\"name\"", id_pos);
        if (name_pos == std::string::npos) break;
        
        // Находим начало объекта
        size_t obj_start = json.rfind("{", id_pos);
        if (obj_start == std::string::npos) break;
        
        // Находим конец объекта
        size_t obj_end = name_pos;
        int brace_count = 0;
        for (size_t i = obj_start; i < json.length(); ++i) {
            if (json[i] == '{') brace_count++;
            else if (json[i] == '}') {
                brace_count--;
                if (brace_count == 0) {
                    obj_end = i;
                    break;
                }
            }
        }
        
        // Извлекаем id
        size_t id_start = json.find("\"", id_pos + 4);
        if (id_start == std::string::npos) break;
        size_t id_end = json.find("\"", id_start + 1);
        if (id_end == std::string::npos) break;
        std::string id = json.substr(id_start + 1, id_end - id_start - 1);
        
        // Извлекаем name
        size_t name_start = json.find("\"", name_pos + 6);
        if (name_start == std::string::npos) break;
        size_t name_end = json.find("\"", name_start + 1);
        if (name_end == std::string::npos) break;
        std::string name = json.substr(name_start + 1, name_end - name_start - 1);
        
        if (!first) result += ",";
        first = false;
        result += "{\"id\":\"" + id + "\",\"name\":\"" + name + "\"}";
        
        pos = obj_end + 1;
    }
    
    result += "]";
    return result;
}

// Извлечение конкретной карты по ID
std::string extractMapById(const std::string& json, const std::string& map_id) {
    size_t pos = 0;
    while (true) {
        size_t id_pos = json.find("\"id\"", pos);
        if (id_pos == std::string::npos) break;
        
        size_t id_start = json.find("\"", id_pos + 4);
        if (id_start == std::string::npos) break;
        size_t id_end = json.find("\"", id_start + 1);
        if (id_end == std::string::npos) break;
        std::string id = json.substr(id_start + 1, id_end - id_start - 1);
        
        if (id == map_id) {
            // Находим начало и конец объекта
            size_t obj_start = json.rfind("{", id_pos);
            if (obj_start == std::string::npos) return "";
            
            size_t obj_end = id_end;
            int brace_count = 0;
            for (size_t i = obj_start; i < json.length(); ++i) {
                if (json[i] == '{') brace_count++;
                else if (json[i] == '}') {
                    brace_count--;
                    if (brace_count == 0) {
                        obj_end = i;
                        break;
                    }
                }
            }
            
            return json.substr(obj_start, obj_end - obj_start + 1);
        }
        
        pos = id_end + 1;
    }
    return "";
}

// Обработка статических файлов
bool handleStaticFile(const std::string& method, const std::string& uri,
                      const fs::path& static_dir, std::string& response) {
    
    // Только GET и HEAD
    if (method != "GET" && method != "HEAD") return false;
    
    // Декодируем URI
    std::string decoded = urlDecode(uri);
    
    // Удаляем query string
    size_t query_pos = decoded.find('?');
    if (query_pos != std::string::npos) decoded = decoded.substr(0, query_pos);
    
    // Удаляем фрагмент
    size_t frag_pos = decoded.find('#');
    if (frag_pos != std::string::npos) decoded = decoded.substr(0, frag_pos);
    
    // Пустой URI -> корень
    if (decoded.empty() || decoded == "/") {
        decoded = "/index.html";
    }
    
    // Строим путь
    fs::path file_path = static_dir / decoded.substr(1);
    
    // Проверка безопасности (path traversal)
    try {
        fs::path canonical_static = fs::canonical(static_dir);
        fs::path canonical_file;
        
        if (fs::exists(file_path)) {
            canonical_file = fs::canonical(file_path);
        } else {
            // Если файл не существует, проверяем родительскую директорию
            fs::path parent = file_path.parent_path();
            if (fs::exists(parent)) {
                canonical_file = fs::canonical(parent) / file_path.filename();
            } else {
                canonical_file = fs::canonical(static_dir) / file_path;
            }
        }
        
        // Проверяем, что путь внутри static_dir
        if (canonical_file.string().find(canonical_static.string()) != 0) {
            response = "HTTP/1.1 400 Bad Request\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: 11\r\n"
                       "Connection: close\r\n\r\n"
                       "Bad Request";
            return true;
        }
    } catch (const fs::filesystem_error& e) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\n"
                   "Not Found";
        return true;
    }
    
    // Если это директория, ищем index.html
    if (fs::is_directory(file_path)) {
        file_path /= "index.html";
    }
    
    // Проверяем существование файла
    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\n"
                   "Not Found";
        return true;
    }
    
    // Читаем файл
    std::string content = readFile(file_path);
    if (content.empty()) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\n"
                   "Not Found";
        return true;
    }
    
    // Формируем ответ
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

// Обработка API
bool handleApiRequest(const std::string& method, const std::string& uri,
                      const fs::path& config_path, std::string& response) {
    
    // Проверяем формат API
    if (uri.find("/api/v1/") != 0) {
        response = "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 11\r\n"
                   "Connection: close\r\n\r\n"
                   "Bad Request";
        return true;
    }
    
    // Только GET
    if (method != "GET") {
        response = "HTTP/1.1 405 Method Not Allowed\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 17\r\n"
                   "Connection: close\r\n\r\n"
                   "Method Not Allowed";
        return true;
    }
    
    // Загружаем конфиг
    std::string config_content = readFile(config_path);
    if (config_content.empty()) {
        response = "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 21\r\n"
                   "Connection: close\r\n\r\n"
                   "Internal Server Error";
        return true;
    }
    
    // Список карт
    if (uri == "/api/v1/maps") {
        std::string maps_list = extractMapsList(config_content);
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(maps_list.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + maps_list;
        return true;
    }
    
    // Конкретная карта
    if (uri.find("/api/v1/maps/") == 0) {
        std::string map_id = uri.substr(14);
        std::string map_content = extractMapById(config_content, map_id);
        
        if (map_content.empty()) {
            response = "HTTP/1.1 404 Not Found\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: 9\r\n"
                       "Connection: close\r\n\r\n"
                       "Not Found";
        } else {
            response = "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(map_content.size()) + "\r\n"
                       "Connection: close\r\n\r\n" + map_content;
        }
        return true;
    }
    
    // Неизвестный API путь
    response = "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Length: 9\r\n"
               "Connection: close\r\n\r\n"
               "Not Found";
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <static_dir>" << std::endl;
        return 1;
    }
    
    fs::path config_path = argv[1];
    fs::path static_dir = argv[2];
    
    // Проверяем существование
    if (!fs::exists(static_dir)) {
        std::cerr << "Static directory not found: " << static_dir << std::endl;
        return 1;
    }
    
    // Создаём сокет
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