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
    virtual void DeleteAuthor(const std::string& author_id) = 0;
    virtual void EditAuthor(const std::string& author_id, const std::string& new_name) = 0;
    virtual std::optional<domain::Author> GetAuthorById(const std::string& author_id) const = 0;
    virtual std::vector<domain::Author> GetAllAuthors() const = 0;

    virtual void AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::string& tags = "") = 0;
    virtual void DeleteBook(const std::string& book_id) = 0;
    virtual void EditBook(const std::string& book_id, const std::string& title, int publication_year, const std::string& tags) = 0;
    virtual std::optional<domain::Book> GetBookById(const std::string& book_id) const = 0;
    virtual std::vector<domain::Book> GetAllBooks() const = 0;
    virtual std::vector<domain::Book> GetBooksByAuthor(const std::string& author_id) const = 0;
    virtual std::vector<std::string> GetBookTags(const std::string& book_id) const = 0;
    
    virtual std::optional<domain::Author> GetAuthorByName(const std::string& name) const = 0;

    virtual ~UseCases() = default;
};

}  // namespace app