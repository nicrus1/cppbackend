#include <iostream>
#include <string>
#include <optional>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std::literals;

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <connection-string>\n"sv;
            return EXIT_FAILURE;
        }

        // Подключение к БД
        pqxx::connection conn{argv[1]};
        
        // Создание таблицы, если не существует
        {
            pqxx::work w(conn);
            w.exec(
                "CREATE TABLE IF NOT EXISTS books ("
                "id SERIAL PRIMARY KEY, "
                "title varchar(100) NOT NULL, "
                "author varchar(100) NOT NULL, "
                "year integer NOT NULL, "
                "isbn char(13) UNIQUE"
                ")");
            w.commit();
        }

        // Подготовка запросов
        conn.prepare("add_book", 
            "INSERT INTO books (title, author, year, isbn) VALUES ($1, $2, $3, $4)");

        // Чтение команд из stdin
        std::string line;
        while (std::getline(std::cin, line)) {
            try {
                json request = json::parse(line);
                std::string action = request["action"];

                if (action == "exit") {
                    break;
                }
                else if (action == "add_book") {
                    json payload = request["payload"];
                    std::string title = payload["title"];
                    std::string author = payload["author"];
                    int year = payload["year"];
                    
                    std::optional<std::string> isbn;
                    // Исправлено: ищем ключ "ISBN" в верхнем регистре, как указано в задании
                    if (!payload["ISBN"].is_null()) {
                        isbn = payload["ISBN"].get<std::string>();
                    }

                    try {
                        pqxx::work w(conn);
                        if (isbn.has_value()) {
                            w.exec_prepared("add_book", title, author, year, isbn.value());
                        } else {
                            w.exec_prepared("add_book", title, author, year, nullptr);
                        }
                        w.commit();
                        std::cout << json{{"result", true}} << std::endl;
                    } catch (const pqxx::sql_error& e) {
                        std::cout << json{{"result", false}} << std::endl;
                    }
                }
                else if (action == "all_books") {
                    pqxx::read_transaction r(conn);
                    json result = json::array();
                    
                    // Исправлено: передаем сам текст SQL-запроса, а не его имя
                    for (auto [id, title, author, year, isbn] : 
                         r.query<int, std::string, std::string, int, std::optional<std::string>>(
                             "SELECT id, title, author, year, isbn FROM books "
                             "ORDER BY year DESC, title ASC, author ASC, isbn ASC")) {
                        json book;
                        book["id"] = id;
                        book["title"] = title;
                        book["author"] = author;
                        book["year"] = year;
                        if (isbn.has_value()) {
                            book["ISBN"] = isbn.value();
                        } else {
                            book["ISBN"] = nullptr;
                        }
                        result.push_back(book);
                    }
                    std::cout << result.dump() << std::endl;
                }
            } catch (const json::parse_error& e) {
                continue;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}