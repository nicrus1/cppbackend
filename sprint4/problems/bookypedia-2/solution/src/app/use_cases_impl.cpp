#include "use_cases_impl.h"
#include <boost/algorithm/string.hpp>
#include <regex>
#include <set>
#include <stdexcept>
#include <optional>

namespace app {
using namespace domain;

namespace {
    std::vector<std::string> NormalizeTags(const std::string& tags_str) {
        std::vector<std::string> raw_tags;
        boost::split(raw_tags, tags_str, boost::is_any_of(","));
        
        std::set<std::string> unique_tags;
        std::regex multiple_spaces(R"(\s+)");
        
        for (auto& tag : raw_tags) {
            boost::trim(tag);
            if (tag.empty()) continue;
            tag = std::regex_replace(tag, multiple_spaces, " ");
            unique_tags.insert(tag);
        }
        return {unique_tags.begin(), unique_tags.end()};
    }
}

std::string UseCasesImpl::AddAuthor(const std::string& name) {
    auto uow = uow_factory_.CreateUnitOfWork();
    auto new_id = AuthorId::New();
    uow->Authors().Save({new_id, name});
    uow->Commit();
    return new_id.ToString();
}

void UseCasesImpl::DeleteAuthor(const std::string& author_id) {
    auto uow = uow_factory_.CreateUnitOfWork();
    auto id = AuthorId::FromString(author_id);
    
    // Проверяем, существует ли автор
    auto author_opt = uow->Authors().GetById(id);
    if (!author_opt) {
        throw std::runtime_error("Author not found");
    }
    
    // Сначала удаляем все книги автора (каскадное удаление через БД)
    auto books = uow->Books().GetByAuthor(id);
    for (const auto& book : books) {
        uow->Books().Delete(book.GetId());
    }
    
    uow->Authors().Delete(id);
    uow->Commit();
}

void UseCasesImpl::EditAuthor(const std::string& author_id, const std::string& new_name) {
    auto uow = uow_factory_.CreateUnitOfWork();
    auto id = AuthorId::FromString(author_id);
    
    // Проверяем, существует ли автор
    auto author_opt = uow->Authors().GetById(id);
    if (!author_opt) {
        throw std::runtime_error("Author not found");
    }
    
    uow->Authors().Update({id, new_name});
    uow->Commit();
}

std::optional<Author> UseCasesImpl::GetAuthorById(const std::string& author_id) const {
    auto uow = uow_factory_.CreateUnitOfWork();
    return uow->Authors().GetById(AuthorId::FromString(author_id));
}

std::vector<Author> UseCasesImpl::GetAllAuthors() const {
    auto uow = uow_factory_.CreateUnitOfWork();
    return uow->Authors().GetAll();
}

void UseCasesImpl::AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::string& tags) {
    auto uow = uow_factory_.CreateUnitOfWork();
    
    auto author_id_obj = AuthorId::FromString(author_id);
    
    // Проверяем, существует ли автор
    auto author_opt = uow->Authors().GetById(author_id_obj);
    if (!author_opt) {
        throw std::runtime_error("Author not found");
    }
    
    Book book{BookId::New(), author_id_obj, title, publication_year};
    uow->Books().Save(book);
    
    if (!tags.empty()) {
        auto normalized_tags = NormalizeTags(tags);
        for (const auto& tag_text : normalized_tags) {
            BookTag book_tag{book.GetId(), tag_text};
            uow->BookTags().Save(book_tag);
        }
    }
    
    uow->Commit();
}

void UseCasesImpl::DeleteBook(const std::string& book_id) {
    auto uow = uow_factory_.CreateUnitOfWork();
    auto id = BookId::FromString(book_id);
    
    // Проверяем, существует ли книга
    auto book_opt = uow->Books().GetById(id);
    if (!book_opt) {
        throw std::runtime_error("Book not found");
    }
    
    // Удаляем теги книги
    uow->BookTags().DeleteByBook(id);
    
    uow->Books().Delete(id);
    uow->Commit();
}

void UseCasesImpl::EditBook(const std::string& book_id, const std::string& title, int publication_year, const std::string& tags) {
    auto uow = uow_factory_.CreateUnitOfWork();
    auto book_uuid = BookId::FromString(book_id);
    
    auto book_opt = uow->Books().GetById(book_uuid);
    if (!book_opt) throw std::runtime_error("Book not found");
    
    Book updated_book{book_uuid, book_opt->GetAuthorId(), title, publication_year};
    uow->Books().Update(updated_book);
    
    uow->BookTags().DeleteByBook(book_uuid);
    if (!tags.empty()) {
        auto normalized_tags = NormalizeTags(tags);
        for (const auto& tag_text : normalized_tags) {
            uow->BookTags().Save({book_uuid, tag_text});
        }
    }
    
    uow->Commit();
}

std::optional<Book> UseCasesImpl::GetBookById(const std::string& book_id) const {
    auto uow = uow_factory_.CreateUnitOfWork();
    return uow->Books().GetById(BookId::FromString(book_id));
}

std::vector<Book> UseCasesImpl::GetAllBooks() const {
    auto uow = uow_factory_.CreateUnitOfWork();
    return uow->Books().GetAll();
}

std::vector<Book> UseCasesImpl::GetBooksByAuthor(const std::string& author_id) const {
    auto uow = uow_factory_.CreateUnitOfWork();
    return uow->Books().GetByAuthor(AuthorId::FromString(author_id));
}

std::vector<std::string> UseCasesImpl::GetBookTags(const std::string& book_id) const {
    auto uow = uow_factory_.CreateUnitOfWork();
    return uow->BookTags().GetByBook(BookId::FromString(book_id));
}

std::optional<Author> UseCasesImpl::GetAuthorByName(const std::string& name) const {
    auto authors = GetAllAuthors();
    for (const auto& author : authors) {
        if (author.GetName() == name) {
            return author;
        }
    }
    return std::nullopt;
}

}  // namespace app