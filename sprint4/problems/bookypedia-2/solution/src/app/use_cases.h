#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

class UseCases {
public:
    virtual std::string AddAuthor(const std::string& name) = 0;
    virtual void AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::string& tags = "") = 0;
    virtual std::vector<domain::Author> GetAllAuthors() const = 0;
    virtual std::vector<domain::Book> GetAllBooks() const = 0;
    virtual std::vector<domain::Book> GetBooksByAuthor(const std::string& author_id) const = 0;

    virtual ~UseCases() = default;
};

}  // namespace app