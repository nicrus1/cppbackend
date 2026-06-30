#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/book_tag.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (test_authors_) {
        // Тестовый режим
        test_authors_->Save({AuthorId::New(), name});
        return;
    }
    auto uow = CreateUnitOfWork();
    uow->Authors().Save({AuthorId::New(), name});
    uow->Commit();
}

void UseCasesImpl::AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::vector<std::string>& tags) {
    if (test_books_) {
        // Тестовый режим
        Book book{BookId::New(), AuthorId::FromString(author_id), title, publication_year};
        test_books_->Save(book);
        return;
    }
    auto uow = CreateUnitOfWork();
    
    Book book{BookId::New(), AuthorId::FromString(author_id), title, publication_year};
    uow->Books().Save(book);
    
    for (const auto& tag : tags) {
        uow->Tags().Save({book.GetId(), tag});
    }
    
    uow->Commit();
}

std::vector<Author> UseCasesImpl::GetAllAuthors() const {
    if (test_authors_) {
        return test_authors_->GetAll();
    }
    auto uow = CreateUnitOfWork();
    return uow->Authors().GetAll();
}

std::vector<Book> UseCasesImpl::GetAllBooks() const {
    if (test_books_) {
        return test_books_->GetAll();
    }
    auto uow = CreateUnitOfWork();
    return uow->Books().GetAll();
}

std::vector<Book> UseCasesImpl::GetBooksByAuthor(const std::string& author_id) const {
    if (test_books_) {
        return test_books_->GetByAuthor(AuthorId::FromString(author_id));
    }
    auto uow = CreateUnitOfWork();
    return uow->Books().GetByAuthor(AuthorId::FromString(author_id));
}

std::unique_ptr<UnitOfWork> UseCasesImpl::CreateUnitOfWork() const {
    return std::make_unique<UnitOfWork>(connection_);
}

}  // namespace app