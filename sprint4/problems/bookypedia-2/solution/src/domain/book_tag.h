#pragma once
#include <string>
#include <vector>
#include "book.h"

namespace domain {

class BookTag {
public:
    BookTag(BookId book_id, std::string tag)
        : book_id_(std::move(book_id))
        , tag_(std::move(tag)) {
        if (tag_.size() > 30) {
            throw std::runtime_error("Tag too long (max 30 characters)");
        }
    }

    const BookId& GetBookId() const noexcept {
        return book_id_;
    }

    const std::string& GetTag() const noexcept {
        return tag_;
    }

private:
    BookId book_id_;
    std::string tag_;
};

class BookTagRepository {
public:
    virtual void Save(const BookTag& tag) = 0;
    virtual void SaveAll(const std::vector<BookTag>& tags) = 0;
    virtual std::vector<std::string> GetByBook(const BookId& book_id) const = 0;
    virtual void DeleteByBook(const BookId& book_id) = 0;
    virtual void DeleteByBookAndTag(const BookId& book_id, const std::string& tag) = 0;

protected:
    ~BookTagRepository() = default;
};

}  // namespace domain