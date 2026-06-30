#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>

#include "../app/use_cases.h"
#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/book_tag.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {

namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << " by " << book.author << ", " << book.publication_year;
    return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book with tags"s,
                    std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("DeleteAuthor"s, "name"s, "Deletes author and their books"s,
                    std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "name"s, "Edits author"s,
                    std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("ShowBook"s, "title"s, "Shows book details"s,
                    std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "title"s, "Deletes a book"s,
                    std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "title"s, "Edits a book"s,
                    std::bind(&View::EditBook, this, ph::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        if (name.empty()) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        // Получаем параметры книги
        detail::AddBookParams params;
        cmd_input >> params.publication_year;
        if (cmd_input.fail()) {
            output_ << "Failed to add book" << std::endl;
            return true;
        }
        
        std::getline(cmd_input, params.title);
        boost::algorithm::trim(params.title);
        if (params.title.empty()) {
            output_ << "Failed to add book" << std::endl;
            return true;
        }

        // Выбираем или создаем автора
        auto author_id = SelectOrCreateAuthor(cmd_input);
        if (!author_id) {
            output_ << "Failed to add book" << std::endl;
            return true;
        }
        params.author_id = *author_id;

        // Вводим теги
        output_ << "Enter tags (comma separated):" << std::endl;
        std::string tags_line;
        std::getline(input_, tags_line);
        params.tags = NormalizeTags(tags_line);

        // Сохраняем книгу
        use_cases_.AddBook(params.title, params.publication_year, params.author_id, params.tags);
        return true;
    } catch (const std::exception&) {
        output_ << "Failed to add book" << std::endl;
        return true;
    }
}

bool View::ShowAuthors() const {
    auto authors = GetAuthors();
    PrintVector(output_, authors);
    return true;
}

bool View::ShowBooks() const {
    auto books = GetBooks();
    PrintVector(output_, books);
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        if (auto author_id = SelectAuthor()) {
            auto books = GetAuthorBooks(*author_id);
            PrintVector(output_, books);
        }
    } catch (const std::exception&) {
        output_ << "Failed to Show Books" << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::optional<domain::AuthorId> author_id;
        
        if (name.empty()) {
            auto id_str = SelectAuthor();
            if (!id_str) return true;
            author_id = domain::AuthorId::FromString(*id_str);
        } else {
            auto uow = use_cases_.CreateUnitOfWork();
            auto author = uow->Authors().FindByName(name);
            if (!author) {
                output_ << "Failed to delete author" << std::endl;
                return true;
            }
            author_id = author->GetId();
        }

        auto uow = use_cases_.CreateUnitOfWork();
        auto books = uow->Books().GetByAuthor(*author_id);
        for (const auto& book : books) {
            uow->Tags().DeleteByBook(book.GetId());
            uow->Books().Delete(book.GetId());
        }
        uow->Authors().Delete(*author_id);
        uow->Commit();
        return true;
    } catch (const std::exception&) {
        output_ << "Failed to delete author" << std::endl;
        return true;
    }
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::optional<domain::AuthorId> author_id;
        
        if (name.empty()) {
            auto id_str = SelectAuthor();
            if (!id_str) return true;
            author_id = domain::AuthorId::FromString(*id_str);
        } else {
            auto uow = use_cases_.CreateUnitOfWork();
            auto author = uow->Authors().FindByName(name);
            if (!author) {
                output_ << "Failed to edit author" << std::endl;
                return true;
            }
            author_id = author->GetId();
        }

        output_ << "Enter new name:" << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        if (new_name.empty()) {
            output_ << "Failed to edit author" << std::endl;
            return true;
        }

        auto uow = use_cases_.CreateUnitOfWork();
        uow->Authors().Save({*author_id, new_name});
        uow->Commit();
        return true;
    } catch (const std::exception&) {
        output_ << "Failed to edit author" << std::endl;
        return true;
    }
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        auto uow = use_cases_.CreateUnitOfWork();
        std::vector<domain::Book> books;
        
        if (title.empty()) {
            // Показываем все книги
            books = uow->Books().GetAll();
            if (books.empty()) return true;
        } else {
            books = uow->Books().FindByTitle(title);
            if (books.empty()) return true;
        }

        domain::BookId book_id;
        if (books.size() > 1) {
            auto selected = SelectBook(books);
            if (!selected) return true;
            book_id = *selected;
        } else if (!books.empty()) {
            book_id = books[0].GetId();
        } else {
            return true;
        }

        auto book = uow->Books().FindById(book_id);
        if (!book) return true;
        
        auto author = uow->Authors().FindById(book->GetAuthorId());
        if (!author) return true;
        
        auto tags = uow->Tags().GetByBook(book_id);

        output_ << "Title: " << book->GetTitle() << std::endl;
        output_ << "Author: " << author->GetName() << std::endl;
        output_ << "Publication year: " << book->GetPublicationYear() << std::endl;
        
        if (!tags.empty()) {
            output_ << "Tags: ";
            for (size_t i = 0; i < tags.size(); ++i) {
                if (i > 0) output_ << ", ";
                output_ << tags[i];
            }
            output_ << std::endl;
        }
        return true;
    } catch (const std::exception&) {
        output_ << "Failed to show book" << std::endl;
        return true;
    }
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        auto uow = use_cases_.CreateUnitOfWork();
        std::vector<domain::Book> books;
        
        if (title.empty()) {
            books = uow->Books().GetAll();
            if (books.empty()) {
                output_ << "Failed to delete book" << std::endl;
                return true;
            }
        } else {
            books = uow->Books().FindByTitle(title);
            if (books.empty()) {
                output_ << "Failed to delete book" << std::endl;
                return true;
            }
        }

        domain::BookId book_id;
        if (books.size() > 1) {
            auto selected = SelectBook(books);
            if (!selected) return true;
            book_id = *selected;
        } else {
            book_id = books[0].GetId();
        }

        uow->Tags().DeleteByBook(book_id);
        uow->Books().Delete(book_id);
        uow->Commit();
        return true;
    } catch (const std::exception&) {
        output_ << "Failed to delete book" << std::endl;
        return true;
    }
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        auto uow = use_cases_.CreateUnitOfWork();
        std::vector<domain::Book> books;
        
        if (title.empty()) {
            books = uow->Books().GetAll();
            if (books.empty()) {
                output_ << "Book not found" << std::endl;
                return true;
            }
        } else {
            books = uow->Books().FindByTitle(title);
            if (books.empty()) {
                output_ << "Book not found" << std::endl;
                return true;
            }
        }

        domain::BookId book_id;
        if (books.size() > 1) {
            auto selected = SelectBook(books);
            if (!selected) return true;
            book_id = *selected;
        } else {
            book_id = books[0].GetId();
        }

        auto book = uow->Books().FindById(book_id);
        if (!book) {
            output_ << "Book not found" << std::endl;
            return true;
        }

        // Редактируем название
        output_ << "Enter new title or empty line to use the current one (" << book->GetTitle() << "):" << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        if (new_title.empty()) {
            new_title = book->GetTitle();
        }

        // Редактируем год
        output_ << "Enter publication year or empty line to use the current one (" << book->GetPublicationYear() << "):" << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        boost::algorithm::trim(year_str);
        int new_year = book->GetPublicationYear();
        if (!year_str.empty()) {
            try {
                new_year = std::stoi(year_str);
            } catch (const std::exception&) {
                output_ << "Failed to edit book" << std::endl;
                return true;
            }
        }

        // Редактируем теги
        auto current_tags = uow->Tags().GetByBook(book_id);
        std::string tags_str;
        for (size_t i = 0; i < current_tags.size(); ++i) {
            if (i > 0) tags_str += ", ";
            tags_str += current_tags[i];
        }
        output_ << "Enter tags (current tags: " << tags_str << "):" << std::endl;
        std::string new_tags_line;
        std::getline(input_, new_tags_line);
        auto new_tags = NormalizeTags(new_tags_line);
        if (new_tags_line.empty()) {
            new_tags = current_tags;
        }

        // Обновляем книгу
        uow->Books().Save({book_id, book->GetAuthorId(), new_title, new_year});
        uow->Tags().DeleteByBook(book_id);
        for (const auto& tag : new_tags) {
            uow->Tags().Save({book_id, tag});
        }
        uow->Commit();
        return true;
    } catch (const std::exception&) {
        output_ << "Failed to edit book" << std::endl;
        return true;
    }
}

std::optional<detail::AddBookParams> View::GetBookParams(std::istream& cmd_input) const {
    detail::AddBookParams params;

    cmd_input >> params.publication_year;
    if (cmd_input.fail()) {
        output_ << "Failed to add book" << std::endl;
        return std::nullopt;
    }
    
    std::getline(cmd_input, params.title);
    boost::algorithm::trim(params.title);
    
    if (params.title.empty()) {
        output_ << "Failed to add book" << std::endl;
        return std::nullopt;
    }

    auto author_id = SelectAuthor();
    if (!author_id.has_value())
        return std::nullopt;
    else {
        params.author_id = author_id.value();
        return params;
    }
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = GetAuthors();
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[author_idx].id;
}

std::optional<std::string> View::SelectOrCreateAuthor(std::istream& /*cmd_input*/) const {
    output_ << "Enter author name or empty line to select from list:" << std::endl;
    std::string name;
    std::getline(input_, name);
    boost::algorithm::trim(name);

    if (name.empty()) {
        return SelectAuthor();
    }

    auto uow = use_cases_.CreateUnitOfWork();
    auto author = uow->Authors().FindByName(name);
    if (author) {
        return author->GetId().ToString();
    }

    output_ << "No author found. Do you want to add " << name << " (y/n)?" << std::endl;
    std::string answer;
    std::getline(input_, answer);
    boost::algorithm::trim(answer);

    if (answer != "y" && answer != "Y") {
        return std::nullopt;
    }

    auto new_id = domain::AuthorId::New();
    uow->Authors().Save({new_id, name});
    uow->Commit();
    return new_id.ToString();
}

std::optional<domain::BookId> View::SelectBook(const std::vector<domain::Book>& books) const {
    output_ << "Select book:" << std::endl;
    
    auto uow = use_cases_.CreateUnitOfWork();
    std::vector<detail::BookInfo> book_infos;
    for (const auto& book : books) {
        auto author = uow->Authors().FindById(book.GetAuthorId());
        if (!author) continue;
        book_infos.push_back({book.GetTitle(), author->GetName(), book.GetPublicationYear()});
    }
    
    PrintVector(output_, book_infos);
    output_ << "Enter book # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int book_idx;
    try {
        book_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid book num");
    }

    --book_idx;
    if (book_idx < 0 || book_idx >= static_cast<int>(books.size())) {
        throw std::runtime_error("Invalid book num");
    }

    return books[book_idx].GetId();
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> dst_authors;
    auto authors = use_cases_.GetAllAuthors();
    dst_authors.reserve(authors.size());
    for (const auto& author : authors) {
        detail::AuthorInfo info;
        info.id = author.GetId().ToString();
        info.name = author.GetName();
        dst_authors.push_back(info);
    }
    return dst_authors;
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> books_info;
    auto books = use_cases_.GetAllBooks();
    books_info.reserve(books.size());
    
    auto uow = use_cases_.CreateUnitOfWork();
    for (const auto& book : books) {
        auto author = uow->Authors().FindById(book.GetAuthorId());
        if (!author) continue;
        detail::BookInfo info;
        info.title = book.GetTitle();
        info.author = author->GetName();
        info.publication_year = book.GetPublicationYear();
        books_info.push_back(info);
    }
    
    // Сортировка по названию, автору, году
    std::sort(books_info.begin(), books_info.end(),
        [](const detail::BookInfo& a, const detail::BookInfo& b) {
            if (a.title != b.title) return a.title < b.title;
            if (a.author != b.author) return a.author < b.author;
            return a.publication_year < b.publication_year;
        });
    
    return books_info;
}

std::vector<detail::BookInfo> View::GetAuthorBooks(const std::string& author_id) const {
    std::vector<detail::BookInfo> books_info;
    auto books = use_cases_.GetBooksByAuthor(author_id);
    books_info.reserve(books.size());
    
    auto uow = use_cases_.CreateUnitOfWork();
    auto author = uow->Authors().FindById(domain::AuthorId::FromString(author_id));
    if (!author) return books_info;
    
    for (const auto& book : books) {
        detail::BookInfo info;
        info.title = book.GetTitle();
        info.author = author->GetName();
        info.publication_year = book.GetPublicationYear();
        books_info.push_back(info);
    }
    
    std::sort(books_info.begin(), books_info.end(),
        [](const detail::BookInfo& a, const detail::BookInfo& b) {
            if (a.title != b.title) return a.title < b.title;
            if (a.author != b.author) return a.author < b.author;
            return a.publication_year < b.publication_year;
        });
    
    return books_info;
}

std::vector<std::string> View::NormalizeTags(const std::string& input) const {
    std::vector<std::string> result;
    std::set<std::string> unique_tags;
    
    std::stringstream ss(input);
    std::string tag;
    
    while (std::getline(ss, tag, ',')) {
        // Удаляем пробелы в начале и конце
        boost::algorithm::trim(tag);
        
        // Удаляем лишние пробелы внутри
        tag = std::regex_replace(tag, std::regex("\\s+"), " ");
        
        // Пропускаем пустые теги
        if (tag.empty()) continue;
        
        // Проверяем длину
        if (tag.size() > 30) continue;
        
        // Добавляем только уникальные теги
        if (unique_tags.find(tag) == unique_tags.end()) {
            unique_tags.insert(tag);
            result.push_back(tag);
        }
    }
    
    return result;
}

}  // namespace ui