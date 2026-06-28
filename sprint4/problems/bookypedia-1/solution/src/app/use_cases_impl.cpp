#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}

void UseCasesImpl::AddBook(const std::string& title, int publication_year, const std::string& author_id) {
    Book book{BookId::New(), AuthorId::FromString(author_id), title, publication_year};
    books_.Save(book);
}

std::vector<Author> UseCasesImpl::GetAllAuthors() const {
    return authors_.GetAll();
}

std::vector<Book> UseCasesImpl::GetAllBooks() const {
    return books_.GetAll();
}

std::vector<Book> UseCasesImpl::GetBooksByAuthor(const std::string& author_id) const {
    return books_.GetByAuthor(AuthorId::FromString(author_id));
}

}  // namespace app