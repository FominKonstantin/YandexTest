#include "view.h"

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <iostream>
#include <set>

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
  out << book.title << " by " << book.author_name << ", "
      << book.publication_year;
  return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
  int i = 1;
  for (const auto& value : vector) {
    out << i++ << " " << value << std::endl;
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
  menu_.AddAction("DeleteAuthor"s, "[name]"s,
                  "Delete author and all his books"s,
                  std::bind(&View::DeleteAuthor, this, ph::_1));
  menu_.AddAction("DeleteBook"s, "[title]"s, "Delete book"s,
                  std::bind(&View::DeleteBook, this, ph::_1));
  menu_.AddAction("EditAuthor"s, "[name]"s, "Edit author name"s,
                  std::bind(&View::EditAuthor, this, ph::_1));
  menu_.AddAction("EditBook"s, "[title]"s, "Edit book details"s,
                  std::bind(&View::EditBook, this, ph::_1));
  menu_.AddAction("ShowBook"s, "[title]"s, "Show book details"s,
                  std::bind(&View::ShowBook, this, ph::_1));
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
                         params->publication_year, params->tags);
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

bool View::DeleteAuthor(std::istream& cmd_input) const {
  try {
    std::string name;
    std::getline(cmd_input, name);
    boost::algorithm::trim(name);

    std::string author_id;
    if (!name.empty()) {
      auto author = use_cases_.GetAuthorByName(name);
      if (!author.has_value()) {
        output_ << "Failed to delete author"sv << std::endl;
        return true;
      }
      author_id = author->GetId().ToString();
    } else {
      auto selected = SelectAuthor();
      if (!selected.has_value()) {
        return true;
      }
      author_id = *selected;
    }

    if (!use_cases_.DeleteAuthor(author_id)) {
      output_ << "Failed to delete author"sv << std::endl;
    }
  } catch (const std::exception&) {
    output_ << "Failed to delete author"sv << std::endl;
  }
  return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
  try {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    std::vector<detail::BookInfo> books;
    if (!title.empty()) {
      books = GetBooksByTitle(title);
      if (books.empty()) {
        output_ << "Book not found"sv << std::endl;
        return true;
      }

      if (books.size() > 1) {
        output_ << "Multiple books found:" << std::endl;
        PrintVector(output_, books);
        auto selected = SelectBookFromList(
            books, "Enter the book # or empty line to cancel:");
        if (!selected.has_value()) {
          return true;
        }
        if (!use_cases_.DeleteBook(*selected)) {
          output_ << "Failed to delete book"sv << std::endl;
        }
        return true;
      }

      if (!use_cases_.DeleteBook(books[0].id)) {
        output_ << "Failed to delete book"sv << std::endl;
      }
      return true;
    }

    // title.empty() - show all books for selection
    books = GetBooks();
    if (books.empty()) {
      output_ << "No books available" << std::endl;
      return true;
    }

    PrintVector(output_, books);
    auto selected =
        SelectBookFromList(books, "Enter the book # or empty line to cancel:");
    if (!selected.has_value()) {
      return true;
    }
    if (!use_cases_.DeleteBook(*selected)) {
      output_ << "Failed to delete book"sv << std::endl;
    }
  } catch (const std::exception&) {
    output_ << "Failed to delete book"sv << std::endl;
  }
  return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
  try {
    std::string name;
    std::getline(cmd_input, name);
    boost::algorithm::trim(name);

    std::string author_id;
    if (!name.empty()) {
      auto author = use_cases_.GetAuthorByName(name);
      if (!author.has_value()) {
        output_ << "Failed to edit author"sv << std::endl;
        return true;
      }
      author_id = author->GetId().ToString();
    } else {
      auto selected = SelectAuthor();
      if (!selected.has_value()) {
        return true;
      }
      author_id = *selected;
    }

    output_ << "Enter new name:" << std::endl;
    std::string new_name;
    if (!std::getline(input_, new_name) || new_name.empty()) {
      output_ << "Failed to edit author"sv << std::endl;
      return true;
    }
    boost::algorithm::trim(new_name);

    if (!use_cases_.EditAuthor(author_id, new_name)) {
      output_ << "Failed to edit author"sv << std::endl;
    }
  } catch (const std::exception&) {
    output_ << "Failed to edit author"sv << std::endl;
  }
  return true;
}

bool View::EditBook(std::istream& cmd_input) const {
  try {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    std::vector<detail::BookInfo> books;
    std::string book_id;

    if (!title.empty()) {
      books = GetBooksByTitle(title);
      if (books.empty()) {
        output_ << "Book not found"sv << std::endl;
        return true;
      }
      if (books.size() > 1) {
        PrintVector(output_, books);
        auto selected = SelectBookFromList(
            books, "Enter the book # or empty line to cancel:");
        if (!selected.has_value()) {
          return true;
        }
        book_id = *selected;
      } else {
        book_id = books[0].id;
      }
    } else {
      books = GetBooks();
      if (books.empty()) {
        output_ << "No books available" << std::endl;
        return true;
      }
      PrintVector(output_, books);
      auto selected = SelectBookFromList(
          books, "Enter the book # or empty line to cancel:");
      if (!selected.has_value()) {
        return true;
      }
      book_id = *selected;
    }

    auto book_details = use_cases_.GetBookWithDetails(book_id);
    if (!book_details.has_value()) {
      output_ << "Book not found"sv << std::endl;
      return true;
    }

    const auto& current_book = book_details->book;
    const auto& current_tags = book_details->tags;

    output_ << "Enter new title or empty line to use the current one ("
            << current_book.GetTitle() << "):" << std::endl;
    std::string new_title;
    std::getline(input_, new_title);
    boost::algorithm::trim(new_title);
    if (new_title.empty()) {
      new_title = current_book.GetTitle();
    }

    output_ << "Enter publication year or empty line to use the current one ("
            << current_book.GetPublicationYear() << "):" << std::endl;
    std::string year_str;
    std::getline(input_, year_str);
    boost::algorithm::trim(year_str);
    int new_year = current_book.GetPublicationYear();
    if (!year_str.empty()) {
      try {
        new_year = std::stoi(year_str);
      } catch (...) {
        output_ << "Invalid year, using current" << std::endl;
      }
    }

    std::string tags_str;
    if (!current_tags.empty()) {
      std::string tags_list;
      for (size_t i = 0; i < current_tags.size(); ++i) {
        if (i > 0) tags_list += ", ";
        tags_list += current_tags[i];
      }
      output_ << "Enter tags (current tags: " << tags_list << "):" << std::endl;
    } else {
      output_ << "Enter tags (current tags: none):" << std::endl;
    }

    std::getline(input_, tags_str);
    auto new_tags = NormalizeTags(tags_str);
    if (new_tags.empty() && !current_tags.empty()) {
      new_tags = current_tags;
    }

    if (!use_cases_.EditBook(book_id, new_title, new_year, new_tags)) {
      output_ << "Failed to edit book"sv << std::endl;
    }
  } catch (const std::exception&) {
    output_ << "Failed to edit book"sv << std::endl;
  }
  return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
  try {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    std::vector<detail::BookInfo> books;
    std::string book_id;

    if (!title.empty()) {
      books = GetBooksByTitle(title);
      if (books.empty()) {
        return true;
      }
      if (books.size() > 1) {
        PrintVector(output_, books);
        auto selected = SelectBookFromList(
            books, "Enter the book # or empty line to cancel:");
        if (!selected.has_value()) {
          return true;
        }
        book_id = *selected;
      } else {
        book_id = books[0].id;
      }
    } else {
      books = GetBooks();
      if (books.empty()) {
        return true;
      }
      PrintVector(output_, books);
      auto selected = SelectBookFromList(
          books, "Enter the book # or empty line to cancel:");
      if (!selected.has_value()) {
        return true;
      }
      book_id = *selected;
    }

    auto book_details = use_cases_.GetBookWithDetails(book_id);
    if (!book_details.has_value()) {
      return true;
    }

    output_ << "Title: " << book_details->book.GetTitle() << std::endl;
    output_ << "Author: " << book_details->author_name << std::endl;
    output_ << "Publication year: " << book_details->book.GetPublicationYear()
            << std::endl;

    if (!book_details->tags.empty()) {
      output_ << "Tags: ";
      for (size_t i = 0; i < book_details->tags.size(); ++i) {
        if (i > 0) output_ << ", ";
        output_ << book_details->tags[i];
      }
      output_ << std::endl;
    }
  } catch (const std::exception&) {
    output_ << "Failed to show book"sv << std::endl;
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

  output_ << "Enter author name or empty line to select from list:"
          << std::endl;
  std::string author_input;
  std::getline(input_, author_input);
  boost::algorithm::trim(author_input);

  std::optional<std::string> author_id;
  if (!author_input.empty()) {
    auto author = use_cases_.GetAuthorByName(author_input);
    if (author.has_value()) {
      author_id = author->GetId().ToString();
    } else {
      output_ << "No author found. Do you want to add " << author_input
              << " (y/n)?" << std::endl;
      std::string answer;
      std::getline(input_, answer);
      boost::algorithm::trim(answer);
      if (answer == "y" || answer == "Y") {
        use_cases_.AddAuthor(author_input);
        auto new_author = use_cases_.GetAuthorByName(author_input);
        if (new_author.has_value()) {
          author_id = new_author->GetId().ToString();
        } else {
          output_ << "Failed to add book"sv << std::endl;
          return std::nullopt;
        }
      } else {
        output_ << "Failed to add book"sv << std::endl;
        return std::nullopt;
      }
    }
  } else {
    auto selected = SelectAuthor();
    if (!selected.has_value()) {
      output_ << "Failed to add book"sv << std::endl;
      return std::nullopt;
    }
    author_id = selected;
  }

  if (!author_id.has_value()) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  }
  params.author_id = author_id.value();

  output_ << "Enter tags (comma separated):" << std::endl;
  std::string tags_input;
  std::getline(input_, tags_input);
  params.tags = NormalizeTags(tags_input);

  return params;
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
  if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  }

  return authors[author_idx].id;
}

std::optional<std::string> View::SelectBookFromList(
    const std::vector<detail::BookInfo>& books,
    const std::string& prompt) const {
  output_ << prompt << std::endl;
  std::string str;
  if (!std::getline(input_, str) || str.empty()) {
    return std::nullopt;
  }

  int idx;
  try {
    idx = std::stoi(str);
  } catch (std::exception const&) {
    return std::nullopt;
  }

  --idx;
  if (idx < 0 || idx >= static_cast<int>(books.size())) {
    return std::nullopt;
  }

  return books[idx].id;
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
    result.push_back({book.book.GetId().ToString(), book.book.GetTitle(),
                      book.author_name, book.book.GetPublicationYear(),
                      book.tags});
  }

  return result;
}

std::vector<detail::BookInfo> View::GetAuthorBooks(
    const std::string& author_id) const {
  std::vector<detail::BookInfo> result;
  auto books = use_cases_.GetAuthorBooks(author_id);
  result.reserve(books.size());

  for (const auto& book : books) {
    result.push_back({book.book.GetId().ToString(), book.book.GetTitle(),
                      book.author_name, book.book.GetPublicationYear(),
                      book.tags});
  }

  return result;
}

std::vector<detail::BookInfo> View::GetBooksByTitle(
    const std::string& title) const {
  std::vector<detail::BookInfo> result;

  auto books = use_cases_.GetBooksByTitle(title);
  for (const auto& book : books) {
    auto details = use_cases_.GetBookWithDetails(book.GetId().ToString());
    if (details.has_value()) {
      result.push_back({book.GetId().ToString(), book.GetTitle(),
                        details->author_name, book.GetPublicationYear(),
                        details->tags});
    }
  }

  return result;
}

std::vector<std::string> View::NormalizeTags(
    const std::string& tags_input) const {
  std::vector<std::string> tags;
  if (tags_input.empty()) {
    return tags;
  }

  std::vector<std::string> raw_tags;
  boost::algorithm::split(raw_tags, tags_input,
                          boost::algorithm::is_any_of(","));

  std::set<std::string> unique_tags;
  for (auto& tag : raw_tags) {
    boost::algorithm::trim(tag);
    if (tag.empty()) {
      continue;
    }

    std::string normalized;
    bool in_space = false;
    for (char c : tag) {
      if (c == ' ') {
        if (!in_space) {
          normalized += ' ';
          in_space = true;
        }
      } else {
        normalized += c;
        in_space = false;
      }
    }
    if (!normalized.empty() && normalized.back() == ' ') {
      normalized.pop_back();
    }

    if (!normalized.empty()) {
      unique_tags.insert(normalized);
    }
  }

  tags.assign(unique_tags.begin(), unique_tags.end());
  return tags;
}

}  // namespace ui