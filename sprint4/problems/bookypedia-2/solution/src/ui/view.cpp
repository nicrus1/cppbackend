#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/join.hpp>
#include <cassert>
#include <iostream>

#include "../app/use_cases.h"
#include "../domain/author.h"
#include "../domain/book.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
using namespace detail;

namespace detail {
std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
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
    : menu_{menu}, use_cases_{use_cases}, input_{input}, output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "name"s, "Delete author"s, std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "name"s, "Edit author"s, std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s, std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s, std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("ShowBook"s, "<title>"s, "Show book"s, std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "<title>"s, "Delete book"s, std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "<title>"s, "Edit book"s, std::bind(&View::EditBook, this, ph::_1));
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
    } catch (...) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string author_name;
        std::getline(cmd_input, author_name);
        boost::trim(author_name);

        std::string author_id;
        
        if (author_name.empty()) {
            auto optional_author_id = SelectAuthor();
            if (!optional_author_id) return true; 
            author_id = *optional_author_id;
        } else {
            auto authors = use_cases_.GetAllAuthors();
            auto it = std::find_if(authors.begin(), authors.end(), 
                [&](const auto& a){ return a.GetName() == author_name; });
            if (it == authors.end()) {
                output_ << "Failed to delete author" << std::endl;
                return true;
            }
            author_id = it->GetId().ToString();
        }

        use_cases_.DeleteAuthor(author_id);
    } catch (const std::exception& e) {
        output_ << "Failed to delete author" << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string author_name;
        std::getline(cmd_input, author_name);
        boost::trim(author_name);

        std::string author_id;
        
        if (author_name.empty()) {
            auto optional_author_id = SelectAuthor();
            if (!optional_author_id) return true;
            author_id = *optional_author_id;
        } else {
            auto authors = use_cases_.GetAllAuthors();
            auto it = std::find_if(authors.begin(), authors.end(), 
                [&](const auto& a){ return a.GetName() == author_name; });
            if (it == authors.end()) {
                output_ << "Failed to edit author" << std::endl;
                return true;
            }
            author_id = it->GetId().ToString();
        }

        output_ << "Enter new name:" << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::trim(new_name);

        if (new_name.empty()) {
            output_ << "Failed to edit author" << std::endl;
            return true;
        }

        use_cases_.EditAuthor(author_id, new_name);
    } catch (const std::exception& e) {
        output_ << "Failed to edit author" << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        std::string year_str;
        cmd_input >> year_str;
        
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        if (title.empty() || year_str.empty()) {
            output_ << "Failed to add book" << std::endl;
            return true;
        }

        int publication_year;
        try {
            publication_year = std::stoi(year_str);
        } catch (...) {
            output_ << "Failed to add book" << std::endl;
            return true;
        }

        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::string author_name;
        std::getline(input_, author_name);
        boost::trim(author_name);

        std::string author_id;
        
        if (author_name.empty()) {
            auto optional_author_id = SelectAuthor();
            if (!optional_author_id) return true;
            author_id = *optional_author_id;
        } else {
            auto authors = use_cases_.GetAllAuthors();
            bool found = false;
            for (const auto& a : authors) {
                if (a.GetName() == author_name) {
                    author_id = a.GetId().ToString();
                    found = true;
                    break;
                }
            }

            if (!found) {
                output_ << "No author found. Do you want to add " << author_name << " (y/n)?" << std::endl;
                std::string answer;
                std::getline(input_, answer);
                boost::trim(answer);
                if (answer != "y" && answer != "Y") {
                    output_ << "Failed to add book" << std::endl;
                    return true;
                }
                author_id = use_cases_.AddAuthor(author_name);
            }
        }

        output_ << "Enter tags (comma separated):" << std::endl;
        std::string tags_str;
        std::getline(input_, tags_str);
        boost::trim(tags_str);

        use_cases_.AddBook(title, publication_year, author_id, tags_str);
    } catch (...) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    auto authors = GetAuthors();
    PrintVector(output_, authors);
    return true;
}

bool View::ShowBooks() const {
    auto books = use_cases_.GetAllBooks();
    auto authors = use_cases_.GetAllAuthors();
    
    if (books.empty()) {
        return true;
    }
    
    int i = 1;
    for (const auto& book : books) {
        std::string author_name = "Unknown";
        for (const auto& a : authors) {
            if (a.GetId() == book.GetAuthorId()) {
                author_name = a.GetName();
                break;
            }
        }
        output_ << i++ << " " << book.GetTitle() << " by " << author_name << ", " << book.GetPublicationYear() << std::endl;
    }
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        if (auto author_id = SelectAuthor()) {
            auto books = use_cases_.GetBooksByAuthor(*author_id);
            int i = 1;
            for (const auto& book : books) {
                output_ << i++ << " " << book.GetTitle() << ", " << book.GetPublicationYear() << std::endl;
            }
        }
    } catch (...) {
        output_ << "Failed to Show Books" << std::endl;
    }
    return true;
}

std::optional<domain::Book> View::SelectBook(const std::string& title_filter) const {
    auto books = use_cases_.GetAllBooks();
    std::vector<domain::Book> matches;
    
    if (!title_filter.empty()) {
        for (const auto& b : books) {
            if (b.GetTitle() == title_filter) matches.push_back(b);
        }
        if (matches.empty()) {
            output_ << "Book not found" << std::endl;
            return std::nullopt;
        }
        if (matches.size() == 1) return matches.front();
    } else {
        matches = books;
    }
    
    if (matches.empty()) {
        output_ << "Book not found" << std::endl;
        return std::nullopt;
    }
    
    auto authors = use_cases_.GetAllAuthors();
    int i = 1;
    for (const auto& b : matches) {
        std::string author_name = "Unknown";
        for (const auto& a : authors) {
            if (a.GetId() == b.GetAuthorId()) {
                author_name = a.GetName(); break;
            }
        }
        output_ << i++ << " " << b.GetTitle() << " by " << author_name << ", " << b.GetPublicationYear() << std::endl;
    }
    
    output_ << "Enter the book # or empty line to cancel:" << std::endl;
    std::string str;
    if (!std::getline(input_, str) || str.empty()) return std::nullopt;
    
    try {
        int idx = std::stoi(str) - 1;
        if (idx < 0 || idx >= static_cast<int>(matches.size())) {
            throw std::out_of_range("");
        }
        return matches[idx];
    } catch (...) {
        output_ << "Invalid book number" << std::endl;
        return std::nullopt;
    }
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::trim(title);

        auto book_opt = SelectBook(title);
        if (!book_opt) return true;

        auto author_opt = use_cases_.GetAuthorById(book_opt->GetAuthorId().ToString());
        std::string author_name = author_opt ? author_opt->GetName() : "Unknown";

        output_ << "Title: " << book_opt->GetTitle() << std::endl;
        output_ << "Author: " << author_name << std::endl;
        output_ << "Publication year: " << book_opt->GetPublicationYear() << std::endl;
        
        auto tags = use_cases_.GetBookTags(book_opt->GetId().ToString());
        if (!tags.empty()) {
            output_ << "Tags: " << boost::algorithm::join(tags, ", ") << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << e.what() << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::trim(title);

        auto book_opt = SelectBook(title);
        if (book_opt) {
            use_cases_.DeleteBook(book_opt->GetId().ToString());
        }
    } catch (const std::exception& e) {
        output_ << "Failed to delete book" << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::trim(title);

        auto book_opt = SelectBook(title);
        if (!book_opt) return true;

        auto book = *book_opt;

        output_ << "Enter new title:" << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::trim(new_title);
        
        // Если пользователь ввел пустую строку для названия - отменяем операцию
        if (new_title.empty()) {
            output_ << "Operation cancelled" << std::endl;
            return true;
        }

        output_ << "Enter publication year:" << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        boost::trim(year_str);
        int new_year = book.GetPublicationYear();
        if (!year_str.empty()) {
            try {
                new_year = std::stoi(year_str);
            } catch (...) {
                // Если введен невалидный год, оставляем старый
            }
        }

        auto tags = use_cases_.GetBookTags(book.GetId().ToString());
        std::string tags_joined = boost::algorithm::join(tags, ", ");
        output_ << "Enter tags (current tags: " << tags_joined << "):" << std::endl;
        
        std::string new_tags_str;
        std::getline(input_, new_tags_str);
        boost::trim(new_tags_str);

        // Если передана пустая строка для тегов, используем ее как сигнал для сброса тегов
        use_cases_.EditBook(book.GetId().ToString(), new_title, new_year, new_tags_str);
    } catch (const std::exception& e) {
        output_ << "Failed to edit book" << std::endl;
    }
    return true;
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = GetAuthors();
    if (authors.empty()) {
        output_ << "No authors found" << std::endl;
        return std::nullopt;
    }
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (...) {
        output_ << "Invalid author number" << std::endl;
        return std::nullopt;
    }

    --author_idx;
    if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
        output_ << "Invalid author number" << std::endl;
        return std::nullopt;
    }

    return authors[author_idx].id;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> dst_authors;
    auto authors = use_cases_.GetAllAuthors();
    dst_authors.reserve(authors.size());
    for (const auto& author : authors) {
        dst_authors.push_back({author.GetId().ToString(), author.GetName()});
    }
    return dst_authors;
}

}  // namespace ui