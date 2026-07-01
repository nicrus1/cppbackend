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
    menu_.AddAction("DeleteAuthor"s, {} , "Delete author"s, std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s, std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s, std::bind(&View::ShowAuthorBooks, this));
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
        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::string author_name;
        std::getline(input_, author_name);
        boost::trim(author_name);

        std::string author_id;
        
        if (author_name.empty()) {
            auto optional_author_id = SelectAuthor();
            if (!optional_author_id) {
                return true; 
            }
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
                output_ << "Failed to delete author" << std::endl;
                return true;
            }
        }

        use_cases_.DeleteAuthor(author_id);
    } catch (...) {
        output_ << "Failed to delete author"sv << std::endl;
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
    } catch (...) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 || author_idx >= authors.size()) {
        throw std::runtime_error("Invalid author num");
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