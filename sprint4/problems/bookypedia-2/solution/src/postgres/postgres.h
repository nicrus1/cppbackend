#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/book_tag.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::transaction_base& transaction)
        : transaction_{transaction} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAll() const override;
    void Delete(const domain::AuthorId& id);
    std::optional<domain::Author> FindByName(const std::string& name) const;
    std::optional<domain::Author> FindById(const domain::AuthorId& id) const;

private:
    pqxx::transaction_base& transaction_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::transaction_base& transaction)
        : transaction_{transaction} {
    }

    void Save(const domain::Book& book) override;
    std::vector<domain::Book> GetAll() const override;
    std::vector<domain::Book> GetByAuthor(const domain::AuthorId& author_id) const override;
    std::vector<domain::Book> FindByTitle(const std::string& title) const;
    std::optional<domain::Book> FindById(const domain::BookId& id) const;
    void Delete(const domain::BookId& id);

private:
    pqxx::transaction_base& transaction_;
};

class BookTagRepositoryImpl : public domain::BookTagRepository {
public:
    explicit BookTagRepositoryImpl(pqxx::transaction_base& transaction)
        : transaction_{transaction} {
    }

    void Save(const domain::BookTag& tag) override;
    void SaveAll(const std::vector<domain::BookTag>& tags) override;
    std::vector<std::string> GetByBook(const domain::BookId& book_id) const override;
    void DeleteByBook(const domain::BookId& book_id) override;
    void DeleteByBookAndTag(const domain::BookId& book_id, const std::string& tag) override;

private:
    pqxx::transaction_base& transaction_;
};

class Database {
public:
    explicit Database(pqxx::connection& connection)  // Принимаем ссылку
        : connection_{connection} {
        InitTables();
    }

    pqxx::connection& GetConnection() {
        return connection_;
    }

private:
    void InitTables();
    pqxx::connection& connection_;
};

}  // namespace postgres