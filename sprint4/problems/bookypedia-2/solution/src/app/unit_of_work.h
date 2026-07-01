#pragma once
#include <memory>
#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/book_tag.h"

namespace app {

class UnitOfWork {
public:
    virtual void Commit() = 0;
    virtual domain::AuthorRepository& Authors() = 0;
    virtual domain::BookRepository& Books() = 0;
    virtual domain::BookTagRepository& BookTags() = 0;
protected:
    ~UnitOfWork() = default;
};

class UnitOfWorkFactory {
public:
    virtual std::unique_ptr<UnitOfWork> CreateUnitOfWork() = 0;
protected:
    ~UnitOfWorkFactory() = default;
};

}  // namespace app