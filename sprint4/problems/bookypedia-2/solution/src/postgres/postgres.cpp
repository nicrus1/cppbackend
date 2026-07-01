#include <pqxx/zview.hxx>
#include <pqxx/result>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

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

std::optional<domain::Author> AuthorRepositoryImpl::GetByName(const std::string& name) const {
    auto res = work_.exec_params("SELECT id, name FROM authors WHERE name = $1", name);
    if (res.empty()) return std::nullopt;
    return domain::Author{domain::AuthorId::FromString(res[0][0].c_str()), res[0][1].c_str()};
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    work_.exec_params("DELETE FROM authors WHERE id = $1", id.ToString());
}

void AuthorRepositoryImpl::Update(const domain::Author& author) {
    work_.exec_params("UPDATE authors SET name = $2 WHERE id = $1", author.GetId().ToString(), author.GetName());
}


void BookRepositoryImpl::Save(const domain::Book& book) {
    work_.exec_params(
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
    auto res = work_.exec(R"(
SELECT b.id, b.author_id, b.title, b.publication_year 
FROM books b
JOIN authors a ON b.author_id = a.id
ORDER BY b.title ASC, a.name ASC, b.publication_year ASC
    )");
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
    auto res = work_.exec_params(
        "SELECT id, author_id, title, publication_year FROM books "
        "WHERE author_id = $1 ORDER BY publication_year ASC, title ASC",
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

std::optional<domain::Book> BookRepositoryImpl::GetById(const domain::BookId& id) const {
    auto res = work_.exec_params("SELECT id, author_id, title, publication_year FROM books WHERE id = $1", id.ToString());
    if (res.empty()) return std::nullopt;
    return domain::Book{
        domain::BookId::FromString(res[0][0].c_str()),
        domain::AuthorId::FromString(res[0][1].c_str()),
        res[0][2].c_str(),
        res[0][3].as<int>()
    };
}

void BookRepositoryImpl::Delete(const domain::BookId& id) {
    // Сначала удаляем теги книги
    work_.exec_params("DELETE FROM book_tags WHERE book_id = $1", id.ToString());
    // Затем удаляем книгу
    work_.exec_params("DELETE FROM books WHERE id = $1", id.ToString());
}

void BookRepositoryImpl::Update(const domain::Book& book) {
    work_.exec_params(
        "UPDATE books SET title = $2, publication_year = $3, author_id = $4 WHERE id = $1",
        book.GetId().ToString(), book.GetTitle(), book.GetPublicationYear(), book.GetAuthorId().ToString()
    );
}

void BookTagRepositoryImpl::Save(const domain::BookTag& tag) {
    work_.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)", tag.GetBookId().ToString(), tag.GetTag());
}

void BookTagRepositoryImpl::SaveAll(const std::vector<domain::BookTag>& tags) {
    for (const auto& tag : tags) {
        Save(tag);
    }
}

std::vector<std::string> BookTagRepositoryImpl::GetByBook(const domain::BookId& book_id) const {
    auto res = work_.exec_params("SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag ASC", book_id.ToString());
    std::vector<std::string> tags;
    tags.reserve(res.size());
    for (auto row : res) {
        tags.push_back(row[0].c_str());
    }
    return tags;
}

void BookTagRepositoryImpl::DeleteByBook(const domain::BookId& book_id) {
    work_.exec_params("DELETE FROM book_tags WHERE book_id = $1", book_id.ToString());
}

void BookTagRepositoryImpl::DeleteByBookAndTag(const domain::BookId& book_id, const std::string& tag) {
    work_.exec_params("DELETE FROM book_tags WHERE book_id = $1 AND tag = $2", book_id.ToString(), tag);
}


Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
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
    tag varchar(30) NOT NULL
);
)"_zv);

    work.commit();
}

}  // namespace postgres