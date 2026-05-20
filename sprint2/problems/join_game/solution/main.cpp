#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <map>  // <-- ДОБАВИТЬ ЭТОТ INCLUDE
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include "api_handler.h"
#include "game_session.h"
#include "player_tokens.h"

const int PORT = 8080;
const std::string STATIC_DIR = "static";
const std::string CONFIG_DIR = "data/config.json";

GameSession gameSession;
PlayerTokens playerTokens;
ApiHandler apiHandler(gameSession, playerTokens);

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Функция для проверки окончания строки (замена ends_with для C++17)
bool hasSuffix(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string getMimeType(const std::string& path) {
    if (hasSuffix(path, ".html")) return "text/html";
    if (hasSuffix(path, ".css")) return "text/css";
    if (hasSuffix(path, ".js")) return "application/javascript";
    if (hasSuffix(path, ".svg")) return "image/svg+xml";
    if (hasSuffix(path, ".json")) return "application/json";
    if (hasSuffix(path, ".png")) return "image/png";
    if (hasSuffix(path, ".jpg") || hasSuffix(path, ".jpeg")) return "image/jpeg";
    return "text/plain";
}

std::string urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            char hex[3] = {str[i+1], str[i+2], 0};
            result += static_cast<char>(strtol(hex, nullptr, 16));
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string serveStaticFile(const std::string& url) {
    // Защита от path traversal
    if (url.find("..") != std::string::npos) {
        return "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
    }
    
    std::string path = url;
    if (path == "/") path = "/index.html";
    
    // Убираем query string
    size_t qpos = path.find('?');
    if (qpos != std::string::npos) path = path.substr(0, qpos);
    
    // URL decode
    std::string decoded = urlDecode(path);
    
    std::string fullPath = STATIC_DIR + decoded;
    std::string content = readFile(fullPath);
    if (content.empty()) {
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    }
    
    std::string mime = getMimeType(fullPath);
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: " + mime + "\r\n";
    response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    response += "Cache-Control: no-cache\r\n";
    response += "Connection: close\r\n";
    response += "\r\n" + content;
    return response;
}

std::string extractHeader(const std::string& request, const std::string& headerName) {
    std::string search = headerName + ": ";
    size_t pos = request.find(search);
    if (pos == std::string::npos) return "";
    size_t end = request.find("\r\n", pos);
    if (end == std::string::npos) return "";
    return request.substr(pos + search.length(), end - pos - search.length());
}

void* handleClient(void* arg) {
    int clientFd = *(int*)arg;
    delete (int*)arg;
    
    char buffer[65536] = {0};
    int bytesRead = read(clientFd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) {
        close(clientFd);
        return nullptr;
    }
    
    std::string request(buffer);
    
    // Парсим метод и путь
    size_t methodEnd = request.find(' ');
    if (methodEnd == std::string::npos) {
        close(clientFd);
        return nullptr;
    }
    
    std::string method = request.substr(0, methodEnd);
    size_t pathEnd = request.find(' ', methodEnd + 1);
    if (pathEnd == std::string::npos) {
        close(clientFd);
        return nullptr;
    }
    
    std::string path = request.substr(methodEnd + 1, pathEnd - methodEnd - 1);
    
    // Извлекаем заголовки
    std::string authHeader = extractHeader(request, "Authorization");
    std::string contentType = extractHeader(request, "Content-Type");
    
    // Извлекаем тело запроса
    std::string body;
    size_t bodyStart = request.find("\r\n\r\n");
    if (bodyStart != std::string::npos && bodyStart + 4 < request.length()) {
        body = request.substr(bodyStart + 4);
    }
    
    std::string response;
    
    // Обработка API
    if (path == "/api/v1/game/join") {
        if (method != "POST") {
            response = "HTTP/1.1 405 Method Not Allowed\r\n"
                       "Content-Type: application/json\r\n"
                       "Allow: POST\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Content-Length: 68\r\n"
                       "\r\n"
                       "{\"code\":\"invalidMethod\",\"message\":\"Only POST method is expected\"}";
        } else {
            std::string respBody = apiHandler.handleJoinPost(body);
            int status = 200;
            if (respBody.find("mapNotFound") != std::string::npos) status = 404;
            else if (respBody.find("invalidArgument") != std::string::npos) status = 400;
            
            response = "HTTP/1.1 " + std::to_string(status) + " OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(respBody.size()) + "\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Connection: close\r\n"
                       "\r\n" + respBody;
        }
    }
    else if (path == "/api/v1/game/players") {
        if (method != "GET" && method != "HEAD") {
            response = "HTTP/1.1 405 Method Not Allowed\r\n"
                       "Content-Type: application/json\r\n"
                       "Allow: GET, HEAD\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Content-Length: 54\r\n"
                       "\r\n"
                       "{\"code\":\"invalidMethod\",\"message\":\"Invalid method\"}";
        } else {
            std::string respBody = apiHandler.handlePlayersGet(authHeader);
            int status = 200;
            if (respBody.find("invalidToken") != std::string::npos || 
                respBody.find("unknownToken") != std::string::npos) {
                status = 401;
            }
            
            response = "HTTP/1.1 " + std::to_string(status) + " OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(respBody.size()) + "\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Connection: close\r\n"
                       "\r\n" + respBody;
        }
    }
    else {
        response = serveStaticFile(path);
    }
    
    send(clientFd, response.c_str(), response.size(), 0);
    close(clientFd);
    return nullptr;
}

int main() {
    std::cout << "Game Server starting on port " << PORT << std::endl;
    std::cout << "Static files directory: " << STATIC_DIR << std::endl;
    
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }
    
    int opt = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(serverFd);
        return 1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        close(serverFd);
        return 1;
    }
    
    if (listen(serverFd, 10) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(serverFd);
        return 1;
    }
    
    std::cout << "Server is ready. Waiting for connections..." << std::endl;
    
    while (true) {
        int* clientFd = new int;
        *clientFd = accept(serverFd, nullptr, nullptr);
        if (*clientFd < 0) {
            delete clientFd;
            continue;
        }
        
        pthread_t thread;
        if (pthread_create(&thread, nullptr, handleClient, clientFd) != 0) {
            delete clientFd;
            continue;
        }
        pthread_detach(thread);
    }
    
    close(serverFd);
    return 0;
}