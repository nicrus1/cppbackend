#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

class UnitOfWork;

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual void AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::vector<std::string>& tags) = 0;
    virtual std::vector<domain::Author> GetAllAuthors() const = 0;
    virtual std::vector<domain::Book> GetAllBooks() const = 0;
    virtual std::vector<domain::Book> GetBooksByAuthor(const std::string& author_id) const = 0;
    virtual std::unique_ptr<UnitOfWork> CreateUnitOfWork() = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app