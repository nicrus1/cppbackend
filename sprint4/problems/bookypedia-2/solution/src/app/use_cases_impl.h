#pragma once
#include <pqxx/connection>

#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "unit_of_work.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void AddAuthor(const std::string& name) override;
    void AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::vector<std::string>& tags) override;
    std::vector<domain::Author> GetAllAuthors() const override;
    std::vector<domain::Book> GetAllBooks() const override;
    std::vector<domain::Book> GetBooksByAuthor(const std::string& author_id) const override;
    std::unique_ptr<UnitOfWork> CreateUnitOfWork() override;

private:
    pqxx::connection& connection_;
};

}  // namespace app