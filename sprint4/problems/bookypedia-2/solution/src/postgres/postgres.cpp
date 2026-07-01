#include "postgres.h"
#include <pqxx/zview.hxx>
#include <pqxx/result>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

// --- AuthorRepositoryImpl ---

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    work_.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAll() const {
    auto res = work_.exec("SELECT id, name FROM authors ORDER BY name");
    std::vector<domain::Author> authors;
    authors.reserve(res.size());
    for (auto row : res) {
        authors.emplace_back(domain::AuthorId::FromString(row[0].c_str()), row[1].c_str());
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::GetById(const domain::AuthorId& id) const {
    auto res = work_.exec_params("SELECT id, name FROM authors WHERE id = $1", id.ToString());
    if (res.empty()) return std::nullopt;
    return domain::Author{domain::AuthorId::FromString(res[0][0].c_str()), res[0][1].c_str()};
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    work_.exec_params("DELETE FROM authors WHERE id = $1", id.ToString());
}

void AuthorRepositoryImpl::Update(const domain::Author& author) {
    work_.exec_params("UPDATE authors SET name = $2 WHERE id = $1", author.GetId().ToString(), author.GetName());
}

std::optional<domain::Author> AuthorRepositoryImpl::GetByName(const std::string& name) const {
    auto res = work_.exec_params("SELECT id, name FROM authors WHERE name = $1", name);
    if (res.empty()) return std::nullopt;
    return domain::Author{domain::AuthorId::FromString(res[0][0].c_str()), res[0][1].c_str()};
}

// --- BookRepositoryImpl ---

void BookRepositoryImpl::Save(const domain::Book& book) {
    work_.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET author_id=$2, title=$3, publication_year=$4;
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(), book.GetPublicationYear());
}

std::vector<domain::Book> BookRepositoryImpl::GetAll() const {
    auto res = work_.exec("SELECT id, author_id, title, publication_year FROM books ORDER BY title");
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

std::vector<domain::Book> Book