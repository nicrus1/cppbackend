#pragma once
#include <pqxx/connection>
#include <optional>

#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "unit_of_work.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    // Конструктор для продакшена - принимает connection
    explicit UseCasesImpl(pqxx::connection& connection)
        : connection_{&connection}
        , is_test_mode_{false} {
    }
    
    // Конструктор для тестов - принимает репозитории
    UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : connection_{nullptr}
        , test_authors_{&authors}
        , test_books_{&books}
        , is_test_mode_{true} {
    }

    void AddAuthor(const std::string& name) override;
    void AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::vector<std::string>& tags) override;
    std::vector<domain::Author> GetAllAuthors() const override;
    std::vector<domain::Book> GetAllBooks() const override;
    std::vector<domain::Book> GetBooksByAuthor(const std::string& author_id) const override;
    std::unique_ptr<UnitOfWork> CreateUnitOfWork() const override;

private:
    pqxx::connection* connection_;  // Указатель вместо ссылки
    // Для тестов
    domain::AuthorRepository* test_authors_ = nullptr;
    domain::BookRepository* test_books_ = nullptr;
    bool is_test_mode_ = false;
};

}  // namespace app