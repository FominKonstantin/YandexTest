#include "view.h"

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <unordered_map>

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
  for (auto& value : vector) {
    out << i++ << " " << value << std::endl;
  }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input,
           std::ostream& output)
    : menu_{menu}, use_cases_{use_cases}, input_{input}, output_{output} {
  menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s,
                  std::bind(&View::AddAuthor, this, ph::_1));
  menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                  std::bind(&View::AddBook, this, ph::_1));
  menu_.AddAction("ShowAuthors"s, {}, "Show authors"s,
                  std::bind(&View::ShowAuthors, this));
  menu_.AddAction("ShowBooks"s, {}, "Show books"s,
                  std::bind(&View::ShowBooks, this));
  menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                  std::bind(&View::ShowAuthorBooks, this));
  menu_.AddAction("DeleteAuthor"s, {}, "Delete author"s,
                  std::bind(&View::DeleteAuthor, this, ph::_1));
  menu_.AddAction("EditAuthor"s, {}, "Edit author"s,
                  std::bind(&View::EditAuthor, this, ph::_1));
  menu_.AddAction("ShowBook"s, {}, "Show book details"s,
                  std::bind(&View::ShowBook, this, ph::_1));
  menu_.AddAction("DeleteBook"s, {}, "Delete book"s,
                  std::bind(&View::DeleteBook, this, ph::_1));
  menu_.AddAction("EditBook"s, {}, "Edit book"s,
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
    std::string line;
    std::getline(cmd_input, line);
    std::istringstream iss(line);

    int year;
    std::string title;
    iss >> year;
    std::getline(iss, title);
    boost::algorithm::trim(title);

    if (title.empty()) {
      output_ << "Failed to add book"sv << std::endl;
      return true;
    }

    std::string author_name;
    output_ << "Enter author name or empty line to select from list:"
            << std::endl;
    std::getline(input_, author_name);
    boost::algorithm::trim(author_name);

    std::string author_id;

    if (author_name.empty()) {
      auto id = SelectAuthor();
      if (!id.has_value()) {
        output_ << "Failed to add book"sv << std::endl;
        return true;
      }
      author_id = id.value();
    } else {
      auto id = SelectAuthorByName(author_name);
      if (!id.has_value()) {
        output_ << "No author found. Do you want to add " << author_name
                << " (y/n)?" << std::endl;
        std::string answer;
        std::getline(input_, answer);
        boost::algorithm::trim(answer);
        if (answer != "y" && answer != "Y") {
          output_ << "Failed to add book"sv << std::endl;
          return true;
        }
        use_cases_.AddAuthor(author_name);
        auto new_id = SelectAuthorByName(author_name);
        if (!new_id.has_value()) {
          output_ << "Failed to add book"sv << std::endl;
          return true;
        }
        author_id = new_id.value();
      } else {
        author_id = id.value();
      }
    }

    output_ << "Enter tags (comma separated):" << std::endl;
    std::string tags_input;
    std::getline(input_, tags_input);
    auto tags = ParseTags(tags_input);

    use_cases_.AddBook(author_id, title, year, tags);

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
    if (name.empty()) {
      auto id = SelectAuthor();
      if (!id.has_value()) {
        return true;
      }
      author_id = id.value();
    } else {
      auto id = SelectAuthorByName(name);
      if (!id.has_value()) {
        output_ << "Failed to delete author"sv << std::endl;
        return true;
      }
      author_id = id.value();
    }

    use_cases_.DeleteAuthor(author_id);
  } catch (const std::exception&) {
    output_ << "Failed to delete author"sv << std::endl;
  }
  return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
  try {
    std::string name;
    std::getline(cmd_input, name);
    boost::algorithm::trim(name);

    std::string author_id;
    if (name.empty()) {
      auto id = SelectAuthor();
      if (!id.has_value()) {
        return true;
      }
      author_id = id.value();
    } else {
      auto id = SelectAuthorByName(name);
      if (!id.has_value()) {
        output_ << "Failed to edit author"sv << std::endl;
        return true;
      }
      author_id = id.value();
    }

    output_ << "Enter new name:" << std::endl;
    std::string new_name;
    std::getline(input_, new_name);
    boost::algorithm::trim(new_name);

    if (new_name.empty()) {
      output_ << "Failed to edit author"sv << std::endl;
      return true;
    }

    use_cases_.UpdateAuthor(author_id, new_name);
  } catch (const std::exception&) {
    output_ << "Failed to edit author"sv << std::endl;
  }
  return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
  try {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    std::optional<detail::BookInfo> book;
    if (title.empty()) {
      book = SelectBook();
    } else {
      book = SelectBookByTitle(title);
    }

    if (book.has_value()) {
      PrintBookInfo(book.value());
    }
  } catch (const std::exception&) {
    // Ничего не выводим при ошибке
  }
  return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
  try {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    if (title.empty()) {
      auto books = GetBooks();
      if (books.empty()) {
        output_ << "Failed to delete book"sv << std::endl;
        return true;
      }
      output_ << "Select book:" << std::endl;
      PrintVector(output_, books);
      output_ << "Enter the book # or empty line to cancel:" << std::endl;

      std::string str;
      if (!std::getline(input_, str) || str.empty()) {
        output_ << "Failed to delete book"sv << std::endl;
        return true;
      }

      int idx;
      try {
        idx = std::stoi(str);
      } catch (std::exception const&) {
        output_ << "Failed to delete book"sv << std::endl;
        return true;
      }

      --idx;
      if (idx < 0 || idx >= books.size()) {
        output_ << "Failed to delete book"sv << std::endl;
        return true;
      }

      use_cases_.DeleteBook(books[idx].id);
    } else {
      auto all_books = GetBooks();
      std::vector<detail::BookInfo> matches;
      for (const auto& b : all_books) {
        if (b.title == title) {
          matches.push_back(b);
        }
      }

      if (matches.empty()) {
        output_ << "Failed to delete book"sv << std::endl;
        return true;
      }

      if (matches.size() == 1) {
        use_cases_.DeleteBook(matches[0].id);
      } else {
        output_ << "Multiple books found with title '" << title
                << "':" << std::endl;
        PrintVector(output_, matches);
        output_ << "Enter the book # or empty line to cancel:" << std::endl;

        std::string str;
        if (!std::getline(input_, str) || str.empty()) {
          output_ << "Failed to delete book"sv << std::endl;
          return true;
        }

        int idx;
        try {
          idx = std::stoi(str);
        } catch (std::exception const&) {
          output_ << "Failed to delete book"sv << std::endl;
          return true;
        }

        --idx;
        if (idx < 0 || idx >= matches.size()) {
          output_ << "Failed to delete book"sv << std::endl;
          return true;
        }

        use_cases_.DeleteBook(matches[idx].id);
      }
    }
  } catch (const std::exception&) {
    output_ << "Failed to delete book"sv << std::endl;
  }
  return true;
}

bool View::EditBook(std::istream& cmd_input) const {
  try {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    std::optional<detail::BookInfo> book;
    if (title.empty()) {
      book = SelectBook();
    } else {
      book = SelectBookByTitle(title);
    }

    if (!book.has_value()) {
      output_ << "Book not found"sv << std::endl;
      return true;
    }

    output_ << "Enter new title or empty line to use the current one ("
            << book->title << "):" << std::endl;
    std::string new_title;
    std::getline(input_, new_title);
    boost::algorithm::trim(new_title);
    if (new_title.empty()) {
      new_title = book->title;
    }

    output_ << "Enter publication year or empty line to use the current one ("
            << book->publication_year << "):" << std::endl;
    std::string year_str;
    std::getline(input_, year_str);
    boost::algorithm::trim(year_str);
    int new_year = book->publication_year;
    if (!year_str.empty()) {
      try {
        new_year = std::stoi(year_str);
      } catch (...) {
        // Если не число, оставляем старый год
      }
    }

    std::string tags_prompt = "Enter tags";
    if (!book->tags.empty()) {
      tags_prompt += " (current tags: ";
      for (size_t i = 0; i < book->tags.size(); ++i) {
        if (i > 0) tags_prompt += ", ";
        tags_prompt += book->tags[i];
      }
      tags_prompt += ")";
    }
    tags_prompt += ":";
    output_ << tags_prompt << std::endl;

    std::string tags_input;
    std::getline(input_, tags_input);
    boost::algorithm::trim(tags_input);

    std::vector<std::string> new_tags;
    if (!tags_input.empty()) {
      new_tags = ParseTags(tags_input);
    } else {
      new_tags = book->tags;
    }

    use_cases_.UpdateBook(book->id, new_title, new_year, new_tags);

  } catch (const std::exception&) {
    output_ << "Failed to edit book"sv << std::endl;
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
    return std::nullopt;
  }

  --author_idx;
  if (author_idx < 0 or author_idx >= authors.size()) {
    return std::nullopt;
  }

  return authors[author_idx].id;
}

std::optional<std::string> View::SelectAuthorByName(
    const std::string& name) const {
  auto authors = use_cases_.GetAuthors();
  for (const auto& author : authors) {
    if (author.GetName() == name) {
      return author.GetId().ToString();
    }
  }
  return std::nullopt;
}

std::optional<detail::BookInfo> View::SelectBook() const {
  auto books = GetBooks();
  if (books.empty()) {
    return std::nullopt;
  }
  output_ << "Select book:" << std::endl;
  PrintVector(output_, books);
  output_ << "Enter the book # or empty line to cancel:" << std::endl;

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
  if (idx < 0 || idx >= books.size()) {
    return std::nullopt;
  }

  return books[idx];
}

std::optional<detail::BookInfo> View::SelectBookByTitle(
    const std::string& title) const {
  auto all_books = GetBooks();
  std::vector<detail::BookInfo> matches;

  for (const auto& book : all_books) {
    if (book.title == title) {
      matches.push_back(book);
    }
  }

  if (matches.empty()) {
    return std::nullopt;
  }

  if (matches.size() == 1) {
    return matches[0];
  }

  output_ << "Multiple books found with title '" << title << "':" << std::endl;
  PrintVector(output_, matches);
  output_ << "Enter the book # or empty line to cancel:" << std::endl;

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
  if (idx < 0 || idx >= matches.size()) {
    return std::nullopt;
  }

  return matches[idx];
}

std::vector<std::string> View::ParseTags(const std::string& tags_input) const {
  std::vector<std::string> result;
  std::stringstream ss(tags_input);
  std::string tag;

  while (std::getline(ss, tag, ',')) {
    boost::algorithm::trim(tag);
    // Удаляем лишние пробелы внутри тега
    std::string normalized;
    bool prev_space = false;
    for (char c : tag) {
      if (c == ' ') {
        if (!prev_space) {
          normalized += c;
          prev_space = true;
        }
      } else {
        normalized += c;
        prev_space = false;
      }
    }
    boost::algorithm::trim(normalized);

    if (!normalized.empty()) {
      if (std::find(result.begin(), result.end(), normalized) == result.end()) {
        result.push_back(normalized);
      }
    }
  }

  std::sort(result.begin(), result.end());
  return result;
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

  auto authors = use_cases_.GetAuthors();
  std::unordered_map<std::string, std::string> author_names;
  for (const auto& author : authors) {
    author_names[author.GetId().ToString()] = author.GetName();
  }

  for (const auto& book : books) {
    std::string author_name;
    auto it = author_names.find(book.GetAuthorId());
    if (it != author_names.end()) {
      author_name = it->second;
    }
    auto tags = use_cases_.GetBookTags(book.GetId().ToString());
    result.push_back({book.GetId().ToString(), book.GetTitle(), author_name,
                      book.GetAuthorId(), book.GetPublicationYear(), tags});
  }

  return result;
}

std::vector<detail::BookInfo> View::GetAuthorBooks(
    const std::string& author_id) const {
  std::vector<detail::BookInfo> result;
  auto books = use_cases_.GetAuthorBooks(author_id);
  result.reserve(books.size());

  auto authors = use_cases_.GetAuthors();
  std::unordered_map<std::string, std::string> author_names;
  for (const auto& author : authors) {
    author_names[author.GetId().ToString()] = author.GetName();
  }

  for (const auto& book : books) {
    std::string author_name;
    auto it = author_names.find(book.GetAuthorId());
    if (it != author_names.end()) {
      author_name = it->second;
    }
    auto tags = use_cases_.GetBookTags(book.GetId().ToString());
    result.push_back({book.GetId().ToString(), book.GetTitle(), author_name,
                      book.GetAuthorId(), book.GetPublicationYear(), tags});
  }

  return result;
}

void View::PrintBookInfo(const detail::BookInfo& book) const {
  output_ << "Title: " << book.title << std::endl;
  output_ << "Author: " << book.author_name << std::endl;
  output_ << "Publication year: " << book.publication_year << std::endl;
  if (!book.tags.empty()) {
    output_ << "Tags: ";
    for (size_t i = 0; i < book.tags.size(); ++i) {
      if (i > 0) output_ << ", ";
      output_ << book.tags[i];
    }
    output_ << std::endl;
  }
}

}  // namespace ui