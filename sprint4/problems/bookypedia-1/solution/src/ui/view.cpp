#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <iostream>

#include "../app/use_cases.h"
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
  out << book.title << ", " << book.publication_year;
  return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
  int i = 1;
  for (auto& value : vector) {
    out << i++ << ". " << value << std::endl;  
  }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input,
           std::ostream& output)
    : menu_{menu}, use_cases_{use_cases}, input_{input}, output_{output} {
  menu_.AddAction(  //
      "AddAuthor"s, "name"s, "Adds author"s,
      std::bind(&View::AddAuthor, this, ph::_1));
  menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                  std::bind(&View::AddBook, this, ph::_1));
  menu_.AddAction("ShowAuthors"s, {}, "Show authors"s,
                  std::bind(&View::ShowAuthors, this));
  menu_.AddAction("ShowBooks"s, {}, "Show books"s,
                  std::bind(&View::ShowBooks, this));
  menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                  std::bind(&View::ShowAuthorBooks, this));
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
    if (auto params = GetBookParams(cmd_input)) {
      use_cases_.AddBook(params->author_id, params->title,
                         params->publication_year);
    }
  } catch (const std::exception&) {
    output_ << "Failed to add book"sv << std::endl;
  }
  return true;
}

bool View::ShowAuthors() const {
  auto authors = GetAuthors();
  if (!authors.empty()) {
    PrintVector(output_, authors);
  }
  return true;
}

bool View::ShowBooks() const {
  auto books = GetBooks();
  if (!books.empty()) {
    PrintVector(output_, books);
  }
  return true;
}

bool View::ShowAuthorBooks() const {
  try {
    if (auto author_id = SelectAuthor()) {
      auto books = GetAuthorBooks(*author_id);
      if (!books.empty()) {
        PrintVector(output_, books);
      }
    }
  } catch (const std::exception&) {
    throw std::runtime_error("Failed to Show Books");
  }
  return true;
}

std::optional<detail::AddBookParams> View::GetBookParams(
    std::istream& cmd_input) const {
  detail::AddBookParams params;

  cmd_input >> params.publication_year;
  std::getline(cmd_input, params.title);
  boost::algorithm::trim(params.title);

  if (params.title.empty()) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  }

  auto author_id = SelectAuthor();
  if (not author_id.has_value()) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  } else {
    params.author_id = author_id.value();
    return params;
  }
}

std::optional<std::string> View::SelectAuthor() const {
  output_ << "Select author:" << std::endl;
  auto authors = GetAuthors();
  if (authors.empty()) {
    output_ << "No authors available" << std::endl;
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
  } catch (std::exception const&) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  }

  --author_idx;
  if (author_idx < 0 or author_idx >= authors.size()) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  }

  return authors[author_idx].id;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
  std::vector<detail::AuthorInfo> result;
  auto authors = use_cases_.GetAuthors();
  result.reserve(authors.size());

  for (const auto& author : authors) {
    result.push_back({author.GetId().ToString(), author.GetName()});
  }

  return result;
}

std::vector<detail::BookInfo> View::GetBooks() const {
  std::vector<detail::BookInfo> result;
  auto books = use_cases_.GetBooks();
  result.reserve(books.size());

  for (const auto& book : books) {
    result.push_back({book.GetTitle(), book.GetPublicationYear()});
  }

  return result;
}

std::vector<detail::BookInfo> View::GetAuthorBooks(
    const std::string& author_id) const {
  std::vector<detail::BookInfo> result;
  auto books = use_cases_.GetAuthorBooks(author_id);
  result.reserve(books.size());

  for (const auto& book : books) {
    result.push_back({book.GetTitle(), book.GetPublicationYear()});
  }

  return result;
}

}  // namespace ui