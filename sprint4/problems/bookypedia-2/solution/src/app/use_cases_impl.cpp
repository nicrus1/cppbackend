#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/book_tag.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    auto uow = CreateUnitOfWork();
    uow->Authors().Save({AuthorId::New(), name});
    uow->Commit();
}

void UseCasesImpl::AddBook(const std::string& title, int publication_year, const std::string& author_id, const std::vector<std::string>& tags) {
    auto uow = CreateUnitOfWork();
    
    Book book{BookId::New(), AuthorId::FromString(author_id), title, publication_year};
    uow->Books().Save(book);
    
    for (const auto& tag : tags) {
        uow->Tags().Save({book.GetId(), tag});
    }
    
    uow->Commit();
}

std::vector<Author> UseCasesImpl::GetAllAuthors() const {
    auto uow = CreateUnitOfWork();
    return uow->Authors().GetAll();
}

std::vector<Book> UseCasesImpl::GetAllBooks() const {
    auto uow = CreateUnitOfWork();
    return uow->Books().GetAll();
}

std::vector<Book> UseCasesImpl::GetBooksByAuthor(const std::string& author_id) const {
    auto uow = CreateUnitOfWork();
    return uow->Books().GetByAuthor(AuthorId::FromString(author_id));
}

std::unique_ptr<UnitOfWork> UseCasesImpl::CreateUnitOfWork() {
    return std::make_unique<UnitOfWork>(connection_);
}

}  // namespace app