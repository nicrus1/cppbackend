#include "postgres.h"

#include <pqxx/zview.hxx>
#include <pqxx/result>
#include <algorithm>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    transaction_.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAll() const {
    auto res = transaction_.exec("SELECT id, name FROM authors ORDER BY name");
    std::vector<domain::Author> authors;
    authors.reserve(res.size());
    for (auto row : res) {
        authors.emplace_back(
            domain::AuthorId::FromString(row[0].c_str()),
            row[1].c_str()
        );
    }
    return authors;
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    transaction_.exec_params(
        "DELETE FROM authors WHERE id = $1",
        id.ToString()
    );
}

std::optional<domain::Author> AuthorRepositoryImpl::FindByName(const std::string& name) const {
    auto res = transaction_.exec_params(
        "SELECT id, name FROM authors WHERE name = $1",
        name
    );
    if (res.empty()) {
        return std::nullopt;
    }
    return domain::Author{
        domain::AuthorId::FromString(res[0][0].c_str()),
        res[0][1].c_str()
    };
}

std::optional<domain::Author> AuthorRepositoryImpl::FindById(const domain::AuthorId& id) const {
    auto res = transaction_.exec_params(
        "SELECT id, name FROM authors WHERE id = $1",
        id.ToString()
    );
    if (res.empty()) {
        return std::nullopt;
    }
    return domain::Author{
        domain::AuthorId::FromString(res[0][0].c_str()),
        res[0][1].c_str()
    };
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    transaction_.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET 
    author_id=$2, 
    title=$3, 
    publication_year=$4;
)"_zv,
        book.GetId().ToString(),
        book.GetAuthorId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear());
}

std::vector<domain::Book> BookRepositoryImpl::GetAll() const {
    auto res = transaction_.exec("SELECT id, author_id, title, publication_year FROM books ORDER BY title");
    std::vector<domain::Book> books;
    books.reserve(res.size());
    for (auto row : res) {
        books.emplace_back(
            domain::BookId::FromString(row[0].c_str()),
            domain::AuthorId::FromString(row[1].c_str()),
            row[2].c_str(),
            row[3].as<int>()
        );
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetByAuthor(const domain::AuthorId& author_id) const {
    auto res = transaction_.exec_params(
        "SELECT id, author_id, title, publication_year FROM books "
        "WHERE author_id = $1 ORDER BY publication_year, title",
        author_id.ToString()
    );
    std::vector<domain::Book> books;
    books.reserve(res.size());
    for (auto row : res) {
        books.emplace_back(
            domain::BookId::FromString(row[0].c_str()),
            domain::AuthorId::FromString(row[1].c_str()),
            row[2].c_str(),
            row[3].as<int>()
        );
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::FindByTitle(const std::string& title) const {
    auto res = transaction_.exec_params(
        "SELECT id, author_id, title, publication_year FROM books "
        "WHERE title = $1 ORDER BY title",
        title
    );
    std::vector<domain::Book> books;
    books.reserve(res.size());
    for (auto row : res) {
        books.emplace_back(
            domain::BookId::FromString(row[0].c_str()),
            domain::AuthorId::FromString(row[1].c_str()),
            row[2].c_str(),
            row[3].as<int>()
        );
    }
    return books;
}

std::optional<domain::Book> BookRepositoryImpl::FindById(const domain::BookId& id) const {
    auto res = transaction_.exec_params(
        "SELECT id, author_id, title, publication_year FROM books WHERE id = $1",
        id.ToString()
    );
    if (res.empty()) {
        return std::nullopt;
    }
    return domain::Book{
        domain::BookId::FromString(res[0][0].c_str()),
        domain::AuthorId::FromString(res[0][1].c_str()),
        res[0][2].c_str(),
        res[0][3].as<int>()
    };
}

void BookRepositoryImpl::Delete(const domain::BookId& id) {
    transaction_.exec_params(
        "DELETE FROM books WHERE id = $1",
        id.ToString()
    );
}

void BookTagRepositoryImpl::Save(const domain::BookTag& tag) {
    transaction_.exec_params(
        "INSERT INTO book_tags (book_id, tag) VALUES ($1, $2) ON CONFLICT DO NOTHING",
        tag.GetBookId().ToString(),
        tag.GetTag()
    );
}

void BookTagRepositoryImpl::SaveAll(const std::vector<domain::BookTag>& tags) {
    if (tags.empty()) return;
    
    std::string query = "INSERT INTO book_tags (book_id, tag) VALUES ";
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) query += ", ";
        query += "($" + std::to_string(i * 2 + 1) + ", $" + std::to_string(i * 2 + 2) + ")";
    }
    query += " ON CONFLICT DO NOTHING";
    
    pqxx::params args;
    for (const auto& tag : tags) {
        args.append(tag.GetBookId().ToString());
        args.append(tag.GetTag());
    }
    transaction_.exec_params(query, args);
}

std::vector<std::string> BookTagRepositoryImpl::GetByBook(const domain::BookId& book_id) const {
    auto res = transaction_.exec_params(
        "SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag",
        book_id.ToString()
    );
    std::vector<std::string> tags;
    tags.reserve(res.size());
    for (auto row : res) {
        tags.push_back(row[0].c_str());
    }
    return tags;
}

void BookTagRepositoryImpl::DeleteByBook(const domain::BookId& book_id) {
    transaction_.exec_params(
        "DELETE FROM book_tags WHERE book_id = $1",
        book_id.ToString()
    );
}

void BookTagRepositoryImpl::DeleteByBookAndTag(const domain::BookId& book_id, const std::string& tag) {
    transaction_.exec_params(
        "DELETE FROM book_tags WHERE book_id = $1 AND tag = $2",
        book_id.ToString(),
        tag
    );
}

void Database::InitTables() {
    pqxx::work work{connection_};
    
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title varchar(100) NOT NULL,
    publication_year integer NOT NULL
);
)"_zv);
    
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag VARCHAR(30) NOT NULL,
    PRIMARY KEY (book_id, tag)
);
)"_zv);

    work.commit();
}

}  // namespace postgres