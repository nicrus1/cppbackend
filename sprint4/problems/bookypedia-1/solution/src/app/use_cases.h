#pragma once

#include <string>
#include <vector>

namespace app {

struct BookData {
    std::string title;
    int publication_year;
    std::string author_id;
};

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual void AddBook(const std::string& title, int publication_year, const std::string& author_id) = 0;
    virtual std::vector<domain::Author> GetAllAuthors() const = 0;
    virtual std::vector<domain::Book> GetAllBooks() const = 0;
    virtual std::vector<domain::Book> GetBooksByAuthor(const std::string& author_id) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app