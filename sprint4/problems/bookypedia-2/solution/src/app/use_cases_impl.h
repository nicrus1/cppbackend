#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "use_cases.h"
#include "unit_of_work.h"
#include <optional>

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(UnitOfWorkFactory& uow_factory)
        : uow_factory_{uow_factory} {
    }

    std::string AddAuthor(const std::string& name) override;
    void DeleteAuthor(const std::string& author_id) override;
    void EditAuthor(const std::string& author_id, const std::string& new_name) override;
    std::optional<domain::Author> GetAuthorById(const std::string& author_id) const override;
    std::vector<domain::Author> GetAllAuthors() const override;

    void AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::string& tags = "") override;
    void DeleteBook(const std::string& book_id) override;
    void EditBook(const std::string& book_id, const std::string& title, int publication_year, const std::string& tags) override;
    std::optional<domain::Book> GetBookById(const std::string& book_id) const override;
    std::vector<domain::Book> GetAllBooks() const override;
    std::vector<domain::Book> GetBooksByAuthor(const std::string& author_id) const override;
    std::vector<std::string> GetBookTags(const std::string& book_id) const override;
    
    std::optional<domain::Author> GetAuthorByName(const std::string& name) const;

private:
    UnitOfWorkFactory& uow_factory_;
};

}  // namespace app