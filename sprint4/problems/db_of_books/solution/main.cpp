#include <iostream>
#include <string>
#include <optional>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std::literals;
using pqxx::operator"" _zv;

// Константы для подготовленных запросов
constexpr auto PREP_ADD_BOOK = "add_book"_zv;
constexpr auto PREP_SELECT_ALL = "select_all"_zv;

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <connection-string>\n"sv;
            return EXIT_FAILURE;
        }

        // Подключение к БД
        pqxx::connection conn{argv[1]};
        
        // Подготовка запросов
        conn.prepare(PREP_ADD_BOOK, 
            "INSERT INTO books (title, author, year, isbn) VALUES ($1, $2, $3, $4)"_zv);
        conn.prepare(PREP_SELECT_ALL,
            "SELECT id, title, author, year, isbn FROM books "
            "ORDER BY year DESC, title ASC, author ASC, isbn ASC"_zv);

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
                ");"_zv);
            w.commit();
        }

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
                    if (!payload["isbn"].is_null()) {
                        isbn = payload["isbn"].get<std::string>();
                    }

                    try {
                        pqxx::work w(conn);
                        if (isbn.has_value()) {
                            w.exec_prepared(PREP_ADD_BOOK, title, author, year, isbn.value());
                        } else {
                            w.exec_prepared(PREP_ADD_BOOK, title, author, year, nullptr);
                        }
                        w.commit();
                        std::cout << json{{"result", true}} << std::endl;
                    } catch (const pqxx::sql_error& e) {
                        // Ошибка дублирования ISBN или другая ошибка SQL
                        std::cout << json{{"result", false}} << std::endl;
                    }
                }
                else if (action == "all_books") {
                    pqxx::read_transaction r(conn);
                    json result = json::array();
                    
                    for (auto [id, title, author, year, isbn] : 
                         r.query<int, std::string, std::string, int, std::optional<std::string>>(
                             PREP_SELECT_ALL)) {
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
                // Игнорируем некорректный JSON
                continue;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}