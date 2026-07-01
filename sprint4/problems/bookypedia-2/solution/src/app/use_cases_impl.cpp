#include "use_cases_impl.h"
#include <boost/algorithm/string.hpp>
#include <regex>
#include <set>
#include <stdexcept>
#include <optional>

namespace app {
using namespace domain;

namespace {
    // Функция нормализации тегов
    std::vector<std::string> NormalizeTags(const std::string& tags_str) {
        std::vector<std::string> raw_tags;
        boost::split(raw_tags, tags_str, boost::is_any_of(","));
        
        std::set<std::string> unique_tags;
        std::regex multiple_spaces(R"(\s+)");
        
        for (auto& tag : raw_tags) {
            boost::trim(tag);
            if (tag.empty()) continue;
            // Сжимаем лишние пробелы в один
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

void UseCasesImpl::AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::string& tags) {
    auto uow = uow_factory_.CreateUnitOfWork();
    
    Book book{BookId::New(), AuthorId::FromString(author_id), title, publication_year};
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

std::vector<Author> UseCasesImpl::GetAllAuthors() const {
    auto uow = uow_factory_.CreateUnitOfWork();
    return uow->Authors().GetAll();
}

std::vector<Book> UseCasesImpl::GetAllBooks() const {
    auto uow = uow_factory_.CreateUnitOfWork();
    return uow->Books().GetAll();
}

std::vector<Book> UseCasesImpl::GetBooksByAuthor(const std::string& author_id) const {
    auto uow = uow_factory_.CreateUnitOfWork();
    return uow->Books().GetByAuthor(AuthorId::FromString(author_id));
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