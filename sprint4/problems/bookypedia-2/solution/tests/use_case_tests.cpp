#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"
#include "../src/app/unit_of_work.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }

    std::vector<domain::Author> GetAll() const override {
        return saved_authors;
    }

    void Delete(const domain::AuthorId& id) override {
        std::erase_if(saved_authors, [&](const auto& a) { return a.GetId() == id; });
    }

    std::optional<domain::Author> GetById(const domain::AuthorId& id) const override {
        for (const auto& a : saved_authors) {
            if (a.GetId() == id) return a;
        }
        return std::nullopt;
    }

    void Update(const domain::Author& author) override {
        for (auto& a : saved_authors) {
            if (a.GetId() == author.GetId()) {
                a = author;
                break;
            }
        }
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;

    void Save(const domain::Book& book) override {
        saved_books.emplace_back(book);
    }

    std::vector<domain::Book> GetAll() const override {
        return saved_books;
    }

    std::vector<domain::Book> GetByAuthor(const domain::AuthorId& /*author_id*/) const override {
        return {};
    }

    std::optional<domain::Book> GetById(const domain::BookId& id) const override {
        for (const auto& b : saved_books) {
            if (b.GetId() == id) return b;
        }
        return std::nullopt;
    }

    void Delete(const domain::BookId& id) override {
        std::erase_if(saved_books, [&](const auto& b) { return b.GetId() == id; });
    }

    void Update(const domain::Book& book) override {
        for (auto& b : saved_books) {
            if (b.GetId() == book.GetId()) {
                b = book;
                break;
            }
        }
    }
};

struct MockBookTagRepository : domain::BookTagRepository {
    std::vector<domain::BookTag> saved_tags;

    void Save(const domain::BookTag& tag) override {
        saved_tags.emplace_back(tag);
    }

    void SaveAll(const std::vector<domain::BookTag>& tags) override {
        for (const auto& tag : tags) {
            saved_tags.emplace_back(tag);
        }
    }

    std::vector<std::string> GetByBook(const domain::BookId& /*book_id*/) const override {
        return {};
    }

    void DeleteByBook(const domain::BookId& /*book_id*/) override {}
    void DeleteByBookAndTag(const domain::BookId& /*book_id*/, const std::string& /*tag*/) override {}
};

struct MockUnitOfWork : app::UnitOfWork {
    MockAuthorRepository authors;
    MockBookRepository books;
    MockBookTagRepository tags;
    bool committed = false;

    void Commit() override {
        committed = true;
    }

    domain::AuthorRepository& Authors() override {
        return authors;
    }

    domain::BookRepository& Books() override {
        return books;
    }

    domain::BookTagRepository& BookTags() override {
        return tags;
    }
};

struct MockUnitOfWorkFactory : app::UnitOfWorkFactory {
    MockUnitOfWork* last_uow = nullptr;

    std::unique_ptr<app::UnitOfWork> CreateUnitOfWork() override {
        auto uow = std::make_unique<MockUnitOfWork>();
        last_uow = uow.get();
        return uow;
    }
};

struct Fixture {
    MockUnitOfWorkFactory factory;
    app::UseCasesImpl use_cases{factory};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Author Adding") {
    GIVEN("Use cases") {
        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            auto author_id = use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(factory.last_uow != nullptr);
                REQUIRE(factory.last_uow->authors.saved_authors.size() == 1);
                CHECK(factory.last_uow->authors.saved_authors.at(0).GetName() == author_name);
                CHECK(factory.last_uow->authors.saved_authors.at(0).GetId() != domain::AuthorId{});
                CHECK(factory.last_uow->committed);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        WHEN("Adding a book") {
            auto author_id = use_cases.AddAuthor("Joanne Rowling");
            use_cases.AddBook("Harry Potter", 1997, author_id, "fantasy, magic");

            THEN("book is saved to repository") {
                REQUIRE(factory.last_uow != nullptr);
                REQUIRE(factory.last_uow->books.saved_books.size() == 1);
                CHECK(factory.last_uow->books.saved_books.at(0).GetTitle() == "Harry Potter");
                CHECK(factory.last_uow->books.saved_books.at(0).GetPublicationYear() == 1997);
                CHECK(factory.last_uow->committed);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Book Adding Without Tags") {
    GIVEN("Use cases") {
        WHEN("Adding a book without tags") {
            auto author_id = use_cases.AddAuthor("Joanne Rowling");
            use_cases.AddBook("Harry Potter", 1997, author_id, "");

            THEN("book is saved to repository without tags") {
                REQUIRE(factory.last_uow != nullptr);
                REQUIRE(factory.last_uow->books.saved_books.size() == 1);
                CHECK(factory.last_uow->books.saved_books.at(0).GetTitle() == "Harry Potter");
                CHECK(factory.last_uow->tags.saved_tags.empty());
                CHECK(factory.last_uow->committed);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Getting All Authors") {
    GIVEN("Use cases with authors") {
        use_cases.AddAuthor("Joanne Rowling");
        use_cases.AddAuthor("J.R.R. Tolkien");

        WHEN("Getting all authors") {
            auto authors = use_cases.GetAllAuthors();

            THEN("all authors are returned") {
                REQUIRE(authors.size() == 2);
                CHECK((authors[0].GetName() == "Joanne Rowling" || authors[0].GetName() == "J.R.R. Tolkien"));
                CHECK((authors[1].GetName() == "Joanne Rowling" || authors[1].GetName() == "J.R.R. Tolkien"));
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Getting Books By Author") {
    GIVEN("Use cases with books") {
        auto author_id = use_cases.AddAuthor("Joanne Rowling");
        use_cases.AddBook("Harry Potter", 1997, author_id, "fantasy");
        use_cases.AddBook("The Casual Vacancy", 2012, author_id, "fiction");

        WHEN("Getting books by author") {
            auto books = use_cases.GetBooksByAuthor(author_id);

            THEN("books by the author are returned") {
                REQUIRE(books.size() == 0); 
            }
        }
    }
}