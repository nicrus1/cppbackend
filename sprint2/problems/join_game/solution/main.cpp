// main.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
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

std::string getMimeType(const std::string& path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".css")) return "text/css";
    if (path.ends_with(".js")) return "application/javascript";
    if (path.ends_with(".svg")) return "image/svg+xml";
    if (path.ends_with(".json")) return "application/json";
    return "text/plain";
}

std::string serveStaticFile(const std::string& url) {
    // Защита от path traversal
    std::string path = url;
    if (path == "/") path = "/index.html";
    
    // Убираем query string
    size_t qpos = path.find('?');
    if (qpos != std::string::npos) path = path.substr(0, qpos);
    
    // URL decode (упрощённо)
    // Для file%20with+spaces.html -> file with spaces.html
    std::string decoded;
    for (size_t i = 0; i < path.length(); ++i) {
        if (path[i] == '%' && i + 2 < path.length()) {
            char hex[3] = {path[i+1], path[i+2], 0};
            decoded += static_cast<char>(strtol(hex, nullptr, 16));
            i += 2;
        } else if (path[i] == '+') {
            decoded += ' ';
        } else {
            decoded += path[i];
        }
    }
    
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
    response += "\r\n" + content;
    return response;
}

void* handleClient(void* arg) {
    int clientFd = *(int*)arg;
    delete (int*)arg;
    
    char buffer[65536] = {0};
    read(clientFd, buffer, sizeof(buffer) - 1);
    
    std::string request(buffer);
    size_t methodEnd = request.find(' ');
    if (methodEnd == std::string::npos) {
        close(clientFd);
        return nullptr;
    }
    
    std::string method = request.substr(0, methodEnd);
    size_t pathEnd = request.find(' ', methodEnd + 1);
    std::string path = request.substr(methodEnd + 1, pathEnd - methodEnd - 1);
    
    // Заголовки
    std::map<std::string, std::string> headers;
    size_t headerStart = request.find("\r\n");
    while (headerStart != std::string::npos) {
        size_t lineEnd = request.find("\r\n", headerStart + 2);
        if (lineEnd == std::string::npos) break;
        std::string line = request.substr(headerStart + 2, lineEnd - headerStart - 2);
        if (line.empty()) break;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 2);
            headers[key] = value;
        }
        headerStart = lineEnd;
    }
    
    // Тело
    std::string body;
    size_t bodyStart = request.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        body = request.substr(bodyStart + 4);
    }
    
    std::string response;
    
    if (path.find("/api/v1/game/join") == 0) {
        if (method != "POST") {
            response = "HTTP/1.1 405 Method Not Allowed\r\n"
                       "Content-Type: application/json\r\n"
                       "Allow: POST\r\n"
                       "Cache-Control: no-cache\r\n\r\n"
                       "{\"code\":\"invalidMethod\",\"message\":\"Only POST method is expected\"}";
        } else {
            std::string respBody = apiHandler.handleJoinPost(body);
            int status = 200;
            if (respBody.find("mapNotFound") != std::string::npos) status = 404;
            else if (respBody.find("invalidArgument") != std::string::npos) status = 400;
            response = "HTTP/1.1 " + std::to_string(status) + " \r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(respBody.size()) + "\r\n"
                       "Cache-Control: no-cache\r\n\r\n" + respBody;
        }
    }
    else if (path.find("/api/v1/game/players") == 0) {
        if (method != "GET" && method != "HEAD") {
            response = "HTTP/1.1 405 Method Not Allowed\r\n"
                       "Content-Type: application/json\r\n"
                       "Allow: GET, HEAD\r\n"
                       "Cache-Control: no-cache\r\n\r\n"
                       "{\"code\":\"invalidMethod\",\"message\":\"Invalid method\"}";
        } else {
            std::string authHeader = headers.count("Authorization") ? headers["Authorization"] : "";
            std::string respBody = apiHandler.handlePlayersGet(authHeader);
            int status = 200;
            if (respBody.find("invalidToken") != std::string::npos || 
                respBody.find("unknownToken") != std::string::npos) status = 401;
            response = "HTTP/1.1 " + std::to_string(status) + " \r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(respBody.size()) + "\r\n"
                       "Cache-Control: no-cache\r\n\r\n" + respBody;
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
    // Загружаем конфиг (можно добавить)
    std::cout << "Server starting on port " << PORT << std::endl;
    
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    bind(serverFd, (struct sockaddr*)&addr, sizeof(addr));
    listen(serverFd, 10);
    
    while (true) {
        int* clientFd = new int;
        *clientFd = accept(serverFd, nullptr, nullptr);
        pthread_t thread;
        pthread_create(&thread, nullptr, handleClient, clientFd);
        pthread_detach(thread);
    }
    
    close(serverFd);
    return 0;
}