#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include <optional>

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/book_tag.h"
#include "../app/unit_of_work.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::work& work) : work_{work} {}

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAll() const override;
    std::optional<domain::Author> GetByName(const std::string& name) const;
    void Delete(const domain::AuthorId& id);
    void Update(const domain::Author& author);

private:
    pqxx::work& work_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::work& work) : work_{work} {}

    void Save(const domain::Book& book) override;
    std::vector<domain::Book> GetAll() const override;
    std::vector<domain::Book> GetByAuthor(const domain::AuthorId& author_id) const override;
    std::optional<domain::Book> GetById(const domain::BookId& id) const;
    void Delete(const domain::BookId& id);
    void Update(const domain::Book& book);

private:
    pqxx::work& work_;
};

class BookTagRepositoryImpl : public domain::BookTagRepository {
public:
    explicit BookTagRepositoryImpl(pqxx::work& work) : work_{work} {}

    void Save(const domain::BookTag& tag) override;
    void SaveAll(const std::vector<domain::BookTag>& tags) override;
    std::vector<std::string> GetByBook(const domain::BookId& book_id) const override;
    void DeleteByBook(const domain::BookId& book_id) override;
    void DeleteByBookAndTag(const domain::BookId& book_id, const std::string& tag) override;

private:
    pqxx::work& work_;
};

// Реализация UnitOfWork
class UnitOfWorkImpl : public app::UnitOfWork {
public:
    explicit UnitOfWorkImpl(pqxx::connection& connection)
        : work_{connection}
        , authors_{work_}
        , books_{work_}
        , tags_{work_} {}

    void Commit() override {
        work_.commit();
    }

    AuthorRepositoryImpl& Authors() override { return authors_; }
    BookRepositoryImpl& Books() override { return books_; }
    BookTagRepositoryImpl& BookTags() override { return tags_; }

private:
    pqxx::work work_;
    AuthorRepositoryImpl authors_;
    BookRepositoryImpl books_;
    BookTagRepositoryImpl tags_;
};

// Фабрика транзакций
class UnitOfWorkFactoryImpl : public app::UnitOfWorkFactory {
public:
    explicit UnitOfWorkFactoryImpl(pqxx::connection& connection)
        : connection_{connection} {}

    std::unique_ptr<app::UnitOfWork> CreateUnitOfWork() override {
        return std::make_unique<UnitOfWorkImpl>(connection_);
    }

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    pqxx::connection& GetConnection() { return connection_; }

    UnitOfWorkFactoryImpl& GetUnitOfWorkFactory() {
        return uow_factory_;
    }

private:
    pqxx::connection connection_;
    UnitOfWorkFactoryImpl uow_factory_{connection_};
};

}  // namespace postgres