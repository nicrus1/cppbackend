#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "seabattle.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>
#include <random>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;

    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);

    if (ec) {
        return std::nullopt;
    }

    return {{buf.data(), sz}};
}

static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;

    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);

    return !ec;
}

class SeabattleAgent {
public:
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {
    }

    void StartGame(tcp::socket& socket, bool my_initiative) {
    bool my_turn = my_initiative;
    
    while (!IsGameEnded()) {
        PrintFields();
        
        if (my_turn) {
            // Мой ход — запрашиваю ввод у пользователя
            std::cout << "Ваш ход. Введите координаты (например, A1): ";
            std::string input;
            std::getline(std::cin, input);
            
            if (input.empty() || input.size() != 2) {
                std::cout << "Некорректный ввод. Используйте формат A1-H8." << std::endl;
                continue;
            }
            
            auto move = ParseMove(input);
            if (!move) {
                std::cout << "Некорректные координаты. Используйте формат A1-H8." << std::endl;
                continue;
            }
            
            // Отправляем ход сопернику
            std::string move_str = MoveToString(*move);
            if (!WriteExact(socket, move_str)) {
                std::cout << "Ошибка отправки хода!" << std::endl;
                return;
            }
            
            // Получаем результат
            auto result_str = ReadExact<1>(socket);
            if (!result_str) {
                std::cout << "Ошибка получения результата!" << std::endl;
                return;
            }
            
            SeabattleField::ShotResult result = static_cast<SeabattleField::ShotResult>(result_str->at(0));
            
            // Обрабатываем результат
            auto [x, y] = *move;
            switch (result) {
                case SeabattleField::ShotResult::MISS:
                    std::cout << "Промах!" << std::endl;
                    other_field_.MarkMiss(x, y);
                    my_turn = false;
                    break;
                case SeabattleField::ShotResult::HIT:
                    std::cout << "Попадание!" << std::endl;
                    other_field_.MarkHit(x, y);
                    // my_turn остается true — ещё один ход
                    break;
                case SeabattleField::ShotResult::KILL:
                    std::cout << "Убил!" << std::endl;
                    other_field_.MarkKill(x, y);
                    // my_turn остается true — ещё один ход
                    break;
            }
        } else {
            // Ход соперника — просто ждём данные из сокета
            std::cout << "Ход соперника. Ожидание..." << std::endl;
            
            // Получаем ход соперника
            auto move_str = ReadExact<2>(socket);
            if (!move_str) {
                std::cout << "Ошибка получения хода от соперника!" << std::endl;
                return;
            }
            
            auto move = ParseMove(*move_str);
            if (!move) {
                std::cout << "Получены некорректные координаты от соперника!" << std::endl;
                return;
            }
            
            auto [x, y] = *move;
            
            // Выстрел по моему полю
            SeabattleField::ShotResult result = my_field_.Shoot(x, y);
            
            // Отправляем результат
            std::string result_str(1, static_cast<char>(result));
            if (!WriteExact(socket, result_str)) {
                std::cout << "Ошибка отправки результата!" << std::endl;
                return;
            }
            
            // Обрабатываем результат
            switch (result) {
                case SeabattleField::ShotResult::MISS:
                    std::cout << "Соперник промахнулся по " << MoveToString(*move) << std::endl;
                    my_turn = true;
                    break;
                case SeabattleField::ShotResult::HIT:
                    std::cout << "Соперник попал в " << MoveToString(*move) << std::endl;
                    // my_turn остается false — соперник ходит ещё раз
                    break;
                case SeabattleField::ShotResult::KILL:
                    std::cout << "Соперник уничтожил корабль в " << MoveToString(*move) << std::endl;
                    // my_turn остается false — соперник ходит ещё раз
                    break;
            }
        }
    }
    
    // Игра закончена
    PrintFields();
    if (my_field_.IsLoser()) {
        std::cout << "Вы проиграли!" << std::endl;
    } else {
        std::cout << "Вы победили!" << std::endl;
    }
}
        }
        
        // Игра закончена
        PrintFields();
        if (my_field_.IsLoser() && !other_field_.IsLoser()) {
            std::cout << "Вы проиграли!" << std::endl;
        } else if (!my_field_.IsLoser() && other_field_.IsLoser()) {
            std::cout << "Вы победили!" << std::endl;
        }
    }

private:
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;

        int p1 = sv[0] - 'A', p2 = sv[1] - '1';

        if (p1 < 0 || p1 > 8) return std::nullopt;
        if (p2 < 0 || p2 > 8) return std::nullopt;

        return {{p1, p2}};
    }

    static std::string MoveToString(std::pair<int, int> move) {
        char buff[] = {static_cast<char>(move.first) + 'A', static_cast<char>(move.second) + '1'};
        return {buff, 2};
    }

    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

private:
    SeabattleField my_field_;
    SeabattleField other_field_;
};

void StartServer(const SeabattleField& field, unsigned short port) {
    SeabattleAgent agent(field);
    
    net::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    
    std::cout << "Сервер запущен на порту " << port << ". Ожидание подключения..." << std::endl;
    
    tcp::socket socket(io_context);
    acceptor.accept(socket);
    
    std::cout << "Клиент подключился!" << std::endl;
    
    agent.StartGame(socket, false);
}

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    SeabattleAgent agent(field);
    
    net::io_context io_context;
    tcp::socket socket(io_context);
    
    tcp::endpoint endpoint(net::ip::make_address(ip_str), port);
    
    std::cout << "Подключение к серверу " << ip_str << ":" << port << "..." << std::endl;
    socket.connect(endpoint);
    
    std::cout << "Подключено!" << std::endl;
    
    agent.StartGame(socket, true);
}

int main(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage:" << std::endl;
        std::cout << "  Server: program <seed> <port>" << std::endl;
        std::cout << "  Client: program <seed> <ip> <port>" << std::endl;
        return 1;
    }

    std::mt19937 engine(std::stoi(argv[1]));
    SeabattleField fieldL = SeabattleField::GetRandomField(engine);

    if (argc == 3) {
        StartServer(fieldL, std::stoi(argv[2]));
    } else if (argc == 4) {
        StartClient(fieldL, argv[2], std::stoi(argv[3]));
    }
    
    return 0;
}