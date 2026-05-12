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
            int hex;
            std::istringstream iss(str.substr(i + 1, 2));
            iss >> std::hex >> hex;
            result += static_cast<char>(hex);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

// Получение MIME типа
std::string getMimeType(const fs::path& path) {
    std::string ext = path.extension().string();
    for (char& c : ext) c = std::tolower(c);
    
    static const std::unordered_map<std::string, std::string> mime = {
        {".html", "text/html"}, {".htm", "text/html"},
        {".css", "text/css"}, {".js", "text/javascript"},
        {".json", "application/json"}, {".png", "image/png"},
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".gif", "image/gif"}, {".svg", "image/svg+xml"},
        {".txt", "text/plain"}, {".ico", "image/vnd.microsoft.icon"},
        {".mp3", "audio/mpeg"}
    };
    
    auto it = mime.find(ext);
    return it != mime.end() ? it->second : "application/octet-stream";
}

// Обработка статических файлов
bool handleStaticFile(const std::string& method,
                      const std::string& uri,
                      const std::string& static_dir,
                      std::string& response) {
    
    if (method != "GET" && method != "HEAD") {
        return false;
    }
    
    std::string decoded = urlDecode(uri);
    
    size_t query_pos = decoded.find('?');
    if (query_pos != std::string::npos) {
        decoded = decoded.substr(0, query_pos);
    }
    
    // Формируем путь
    fs::path file_path = fs::path(static_dir) / decoded.substr(1);
    
    if (fs::is_directory(file_path)) {
        file_path /= "index.html";
    }
    
    // Проверка существования
    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\n"
                   "Not Found";
        return true;
    }
    
    // Проверка безопасности (path traversal)
    fs::path canonical_path = fs::canonical(file_path);
    fs::path canonical_static = fs::canonical(static_dir);
    
    if (canonical_path.string().find(canonical_static.string()) != 0) {
        response = "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 20\r\n"
                   "Connection: close\r\n\r\n"
                   "Bad Request";
        return true;
    }
    
    // Читаем файл
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        response = "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 9\r\n"
                   "Connection: close\r\n\r\n"
                   "Not Found";
        return true;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
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

// Обработка API
bool handleApiRequest(const std::string& uri,
                      const std::string& config_path,
                      std::string& response) {
    
    if (uri == "/api/v1/maps") {
        std::ifstream config(config_path);
        if (!config.is_open()) {
            response = "HTTP/1.1 500 Internal Server Error\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: 21\r\n\r\n"
                       "Internal Server Error";
            return true;
        }
        
        std::stringstream buffer;
        buffer << config.rdbuf();
        std::string content = buffer.str();
        
        // Извлекаем список карт из JSON (упрощённо)
        // В реальном проекте используйте JSON библиотеку
        
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(content.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + content;
        return true;
        
    } else if (uri.find("/api/v1/maps/") == 0) {
        // Возвращаем конкретную карту
        std::ifstream config(config_path);
        if (!config.is_open()) {
            response = "HTTP/1.1 500 Internal Server Error\r\n"
                       "Content-Type: text/plain\r\n\r\n"
                       "Internal Server Error";
            return true;
        }
        
        std::stringstream buffer;
        buffer << config.rdbuf();
        std::string content = buffer.str();
        
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: " + std::to_string(content.size()) + "\r\n"
                   "Connection: close\r\n\r\n" + content;
        return true;
    }
    
    response = "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Length: 9\r\n\r\n"
               "Not Found";
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config_file> <static_dir>" << std::endl;
        return 1;
    }
    
    std::string config_path = argv[1];
    std::string static_dir = argv[2];
    
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
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Listen failed" << std::endl;
        return 1;
    }
    
    std::cout << "Server has started..." << std::endl;
    std::cout << "Listening on http://127.0.0.1:8080/" << std::endl;
    
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) continue;
        
        char buffer[8192] = {0};
        read(client_fd, buffer, sizeof(buffer) - 1);
        
        std::string request(buffer);
        std::istringstream request_stream(request);
        
        std::string method, uri, version;
        request_stream >> method >> uri >> version;
        
        std::string response;
        
        if (uri.find("/api/") == 0) {
            handleApiRequest(uri, config_path, response);
        } else {
            handleStaticFile(method, uri, static_dir, response);
        }
        
        send(client_fd, response.c_str(), response.size(), 0);
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}