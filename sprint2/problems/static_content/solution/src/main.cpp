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
#include <fcntl.h>

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
std::string getMimeType(const fs::path& path) {
    static const std::unordered_map<std::string, std::string> mime = {
        {".htm", "text/html"}, {".html", "text/html"},
        {".css", "text/css"}, {".js", "text/javascript"},
        {".json", "application/json"}, {".png", "image/png"},
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".gif", "image/gif"}, {".svg", "image/svg+xml"},
        {".txt", "text/plain"}, {".ico", "image/vnd.microsoft.icon"},
        {".mp3", "audio/mpeg"}, {".xml", "application/xml"}
    };
    
    std::string ext = path.extension().string();
    for (char& c : ext) c = std::tolower(c);
    
    auto it = mime.find(ext);
    return it != mime.end() ? it->second : "application/octet-stream";
}

// Чтение файла
std::string readFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Обработка статических файлов
bool handleStaticFile(const std::string& method, const std::string& uri,
                      const fs::path& static_dir, std::string& response) {
    
    if (method != "GET" && method != "HEAD") {
        return false;
    }
    
    // Декодируем URI
    std::string decoded = urlDecode(uri);
    
    // Удаляем query parameters
    size_t query_pos = decoded.find('?');
    if (query_pos != std::string::npos) {
        decoded = decoded.substr(0, query_pos);
    }
    
    // Удаляем фрагмент
    size_t frag_pos = decoded.find('#');
    if (frag_pos != std::string::npos) {
        decoded = decoded.substr(0, frag_pos);
    }
    
    // Пустой URI -> корень
    if (decoded == "/" || decoded.empty()) {
        decoded = "/index.html";
    }
    
    // Строим путь к файлу
    fs::path file_path = static_dir / decoded.substr(1);
    
    // Проверка на безопасность (path traversal)
    try {
        fs::path canonical_static = fs::canonical(static_dir);
        fs::path canonical_file;
        
        if (fs::exists(file_path)) {
            canonical_file = fs::canonical(file_path);
        } else {
            // Если файла нет, всё равно проверяем родительскую директорию
            canonical_file = fs::canonical(file_path.parent_path()) / file_path.filename();
        }
        
        // Проверяем, что путь внутри статической директории
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
    resp << "Content-Type: " << getMimeType(file_path) << "\r\n";
    resp << "Content-Length: " << content.size() << "\r\n";
    resp << "Connection: close\r\n";
    resp << "\r\n";
    
    if (method == "GET") {
        resp << content;
    }
    
    response = resp.str();
    return true;
}

// Обработка API запросов
bool handleApiRequest(const std::string& method, const std::string& uri,
                      const fs::path& config_path, std::string& response) {
    
    if (method != "GET") {
        response = "HTTP/1.1 405 Method Not Allowed\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 17\r\n"
                   "Connection: close\r\n\r\n"
                   "Method Not Allowed";
        return true;
    }
    
    // Загружаем config.json
    std::string config_content = readFile(config_path);
    if (config_content.empty()) {
        response = "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 21\r\n"
                   "Connection: close\r\n\r\n"
                   "Internal Server Error";
        return true;
    }
    
    // API endpoints
    if (uri == "/api/v1/maps") {
        // Возвращаем список карт (нужно распарсить JSON)
        // Для простоты пока возвращаем весь config
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(config_content.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + config_content;
        return true;
        
    } else if (uri.find("/api/v1/maps/") == 0) {
        // Возвращаем конкретную карту
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(config_content.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + config_content;
        return true;
    }
    
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
    
    // Проверяем существование директорий
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
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt failed" << std::endl;
        return 1;
    }
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Listen failed" << std::endl;
        return 1;
    }
    
    std::cout << "Server has started..." << std::endl;
    std::cout << "Listening on http://127.0.0.1:8080/" << std::endl;
    std::cout << "Static directory: " << static_dir << std::endl;
    std::cout << "Config file: " << config_path << std::endl;
    
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            continue;
        }
        
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