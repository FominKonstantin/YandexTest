#include "view.h"

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <cctype>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>

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

namespace {

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& values) {
  int index = 1;
  for (const auto& value : values) {
    out << index++ << " " << value << std::endl;
  }
}

std::string JoinTags(const std::vector<std::string>& tags) {
  std::ostringstream result;
  for (size_t i = 0; i < tags.size(); ++i) {
    if (i != 0) {
      result << ", ";
    }
    result << tags[i];
  }
  return result.str();
}

}  // namespace

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
    use_cases_.AddAuthor(name);
  } catch (const std::exception&) {
    output_ << "Failed to add author"sv << std::endl;
  }
  return true;
}

bool View::AddBook(std::istream& cmd_input) const {
  try {
    const auto params = GetBookParams(cmd_input);
    if (!params) {
      return true;
    }

    if (!params->new_author_name.empty()) {
      use_cases_.AddBookWithNewAuthor(
          params->new_author_name, params->title, params->publication_year,
          params->tags);
    } else {
      use_cases_.AddBook(params->author_id, params->title,
                         params->publication_year, params->tags);
    }
  } catch (const std::exception&) {
    output_ << "Failed to add book"sv << std::endl;
  }
  return true;
}

bool View::ShowAuthors() const {
  const auto authors = GetAuthors();
  if (!authors.empty()) {
    PrintVector(output_, authors);
  }
  return true;
}

bool View::ShowBooks() const {
  const auto books = GetBooks();
  if (!books.empty()) {
    PrintVector(output_, books);
  }
  return true;
}

bool View::ShowAuthorBooks() const {
  try {
    const auto author_id = SelectAuthor();
    if (!author_id) {
      return true;
    }

    const auto books = GetAuthorBooks(*author_id);
    int index = 1;
    for (const auto& book : books) {
      // В этой команде сохраняем формат предыдущей версии Bookypedia.
      output_ << index++ << " " << book.title << ", " << book.publication_year
              << std::endl;
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
      const auto author = use_cases_.GetAuthorByName(name);
      if (!author) {
        output_ << "Failed to delete author"sv << std::endl;
        return true;
      }
      author_id = author->GetId().ToString();
    } else {
      const auto selected = SelectAuthor();
      if (!selected) {
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
    std::string book_id;

    if (!title.empty()) {
      books = GetBooksByTitle(title);
      if (books.empty()) {
        output_ << "Book not found"sv << std::endl;
        return true;
      }

      if (books.size() == 1) {
        book_id = books.front().id;
      } else {
        PrintVector(output_, books);
        const auto selected = SelectBookFromList(
            books, "Enter the book # or empty line to cancel:");
        if (!selected) {
          return true;
        }
        book_id = *selected;
      }
    } else {
      books = GetBooks();
      if (books.empty()) {
        return true;
      }
      PrintVector(output_, books);
      const auto selected = SelectBookFromList(
          books, "Enter the book # or empty line to cancel:");
      if (!selected) {
        return true;
      }
      book_id = *selected;
    }

    if (!use_cases_.DeleteBook(book_id)) {
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
      const auto author = use_cases_.GetAuthorByName(name);
      if (!author) {
        output_ << "Failed to edit author"sv << std::endl;
        return true;
      }
      author_id = author->GetId().ToString();
    } else {
      const auto selected = SelectAuthor();
      if (!selected) {
        return true;
      }
      author_id = *selected;
    }

    output_ << "Enter new name:" << std::endl;
    std::string new_name;
    if (!std::getline(input_, new_name)) {
      output_ << "Failed to edit author"sv << std::endl;
      return true;
    }
    boost::algorithm::trim(new_name);
    if (new_name.empty()) {
      output_ << "Failed to edit author"sv << std::endl;
      return true;
    }

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
      if (books.size() == 1) {
        book_id = books.front().id;
      } else {
        PrintVector(output_, books);
        const auto selected = SelectBookFromList(
            books, "Enter the book # or empty line to cancel:");
        if (!selected) {
          output_ << "Book not found"sv << std::endl;
          return true;
        }
        book_id = *selected;
      }
    } else {
      books = GetBooks();
      if (books.empty()) {
        output_ << "Book not found"sv << std::endl;
        return true;
      }
      PrintVector(output_, books);
      const auto selected = SelectBookFromList(
          books, "Enter the book # or empty line to cancel:");
      if (!selected) {
        output_ << "Book not found"sv << std::endl;
        return true;
      }
      book_id = *selected;
    }

    const auto book_details = use_cases_.GetBookWithDetails(book_id);
    if (!book_details) {
      output_ << "Book not found"sv << std::endl;
      return true;
    }

    const auto& current_book = book_details->book;
    const auto& current_tags = book_details->tags;

    output_ << "Enter new title or empty line to use the current one ("
            << current_book.GetTitle() << "):" << std::endl;
    std::string new_title;
    if (!std::getline(input_, new_title)) {
      output_ << "Book not found"sv << std::endl;
      return true;
    }
    boost::algorithm::trim(new_title);
    if (new_title.empty()) {
      new_title = current_book.GetTitle();
    }

    output_ << "Enter publication year or empty line to use the current one ("
            << current_book.GetPublicationYear() << "):" << std::endl;
    std::string year_string;
    if (!std::getline(input_, year_string)) {
      output_ << "Book not found"sv << std::endl;
      return true;
    }
    boost::algorithm::trim(year_string);

    int new_year = current_book.GetPublicationYear();
    if (!year_string.empty()) {
      size_t parsed = 0;
      try {
        new_year = std::stoi(year_string, &parsed);
      } catch (const std::exception&) {
        output_ << "Failed to edit book"sv << std::endl;
        return true;
      }
      if (parsed != year_string.size()) {
        output_ << "Failed to edit book"sv << std::endl;
        return true;
      }
    }

    output_ << "Enter tags (current tags: " << JoinTags(current_tags) << "):"
            << std::endl;
    std::string tags_string;
    if (!std::getline(input_, tags_string)) {
      output_ << "Failed to edit book"sv << std::endl;
      return true;
    }
    const auto new_tags = NormalizeTags(tags_string);

    if (!use_cases_.EditBook(book_id, new_title, new_year, new_tags)) {
      output_ << "Book not found"sv << std::endl;
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
      if (books.size() == 1) {
        book_id = books.front().id;
      } else {
        PrintVector(output_, books);
        const auto selected = SelectBookFromList(
            books, "Enter the book # or empty line to cancel:");
        if (!selected) {
          return true;
        }
        book_id = *selected;
      }
    } else {
      books = GetBooks();
      if (books.empty()) {
        return true;
      }
      PrintVector(output_, books);
      const auto selected = SelectBookFromList(
          books, "Enter the book # or empty line to cancel:");
      if (!selected) {
        return true;
      }
      book_id = *selected;
    }

    const auto details = use_cases_.GetBookWithDetails(book_id);
    if (!details) {
      return true;
    }

    output_ << "Title: " << details->book.GetTitle() << std::endl;
    output_ << "Author: " << details->author_name << std::endl;
    output_ << "Publication year: " << details->book.GetPublicationYear()
            << std::endl;
    if (!details->tags.empty()) {
      output_ << "Tags: " << JoinTags(details->tags) << std::endl;
    }
  } catch (const std::exception&) {
    // Для ShowBook задание не задаёт отдельного сообщения об ошибке.
  }
  return true;
}

std::optional<detail::AddBookParams> View::GetBookParams(
    std::istream& cmd_input) const {
  detail::AddBookParams params;

  if (!(cmd_input >> params.publication_year)) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  }
  std::getline(cmd_input, params.title);
  boost::algorithm::trim(params.title);
  if (params.title.empty()) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  }

  output_ << "Enter author name or empty line to select from list:"
          << std::endl;
  std::string author_input;
  if (!std::getline(input_, author_input)) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  }
  boost::algorithm::trim(author_input);

  if (!author_input.empty()) {
    const auto author = use_cases_.GetAuthorByName(author_input);
    if (author) {
      params.author_id = author->GetId().ToString();
    } else {
      output_ << "No author found. Do you want to add " << author_input
              << " (y/n)?" << std::endl;
      std::string answer;
      if (!std::getline(input_, answer)) {
        output_ << "Failed to add book"sv << std::endl;
        return std::nullopt;
      }
      boost::algorithm::trim(answer);
      if (answer != "y" && answer != "Y") {
        output_ << "Failed to add book"sv << std::endl;
        return std::nullopt;
      }
      params.new_author_name = author_input;
    }
  } else {
    const auto selected = SelectAuthor();
    if (!selected) {
      return std::nullopt;
    }
    params.author_id = *selected;
  }

  output_ << "Enter tags (comma separated):" << std::endl;
  std::string tags_input;
  if (!std::getline(input_, tags_input)) {
    output_ << "Failed to add book"sv << std::endl;
    return std::nullopt;
  }
  params.tags = NormalizeTags(tags_input);
  return params;
}

std::optional<std::string> View::SelectAuthor() const {
  output_ << "Select author:" << std::endl;
  const auto authors = GetAuthors();
  if (authors.empty()) {
    output_ << "No authors available" << std::endl;
    return std::nullopt;
  }

  PrintVector(output_, authors);
  output_ << "Enter author # or empty line to cancel" << std::endl;

  std::string value;
  if (!std::getline(input_, value)) {
    return std::nullopt;
  }
  boost::algorithm::trim(value);
  if (value.empty()) {
    return std::nullopt;
  }

  size_t parsed = 0;
  int index = 0;
  try {
    index = std::stoi(value, &parsed);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (parsed != value.size()) {
    return std::nullopt;
  }

  --index;
  if (index < 0 || index >= static_cast<int>(authors.size())) {
    return std::nullopt;
  }
  return authors[index].id;
}

std::optional<std::string> View::SelectBookFromList(
    const std::vector<detail::BookInfo>& books,
    const std::string& prompt) const {
  output_ << prompt << std::endl;

  std::string value;
  if (!std::getline(input_, value)) {
    return std::nullopt;
  }
  boost::algorithm::trim(value);
  if (value.empty()) {
    return std::nullopt;
  }

  size_t parsed = 0;
  int index = 0;
  try {
    index = std::stoi(value, &parsed);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (parsed != value.size()) {
    return std::nullopt;
  }

  --index;
  if (index < 0 || index >= static_cast<int>(books.size())) {
    return std::nullopt;
  }
  return books[index].id;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
  const auto authors = use_cases_.GetAuthors();
  std::vector<detail::AuthorInfo> result;
  result.reserve(authors.size());
  for (const auto& author : authors) {
    result.push_back({author.GetId().ToString(), author.GetName()});
  }
  return result;
}

std::vector<detail::BookInfo> View::GetBooks() const {
  const auto books = use_cases_.GetBooks();
  std::vector<detail::BookInfo> result;
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
  const auto books = use_cases_.GetAuthorBooks(author_id);
  std::vector<detail::BookInfo> result;
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
  const auto books = use_cases_.GetBooksByTitle(title);
  std::vector<detail::BookInfo> result;
  result.reserve(books.size());
  for (const auto& book : books) {
    const auto details =
        use_cases_.GetBookWithDetails(book.GetId().ToString());
    if (!details) {
      continue;
    }
    result.push_back({book.GetId().ToString(), book.GetTitle(),
                      details->author_name, book.GetPublicationYear(),
                      details->tags});
  }
  return result;
}

std::vector<std::string> View::NormalizeTags(
    const std::string& tags_input) const {
  std::vector<std::string> raw_tags;
  boost::algorithm::split(raw_tags, tags_input,
                          boost::algorithm::is_any_of(","));

  std::set<std::string> unique_tags;
  for (auto tag : raw_tags) {
    boost::algorithm::trim(tag);
    if (tag.empty()) {
      continue;
    }

    std::string normalized;
    normalized.reserve(tag.size());
    bool previous_was_space = false;
    for (const unsigned char ch : tag) {
      if (std::isspace(ch)) {
        if (!previous_was_space) {
          normalized.push_back(' ');
          previous_was_space = true;
        }
      } else {
        normalized.push_back(static_cast<char>(ch));
        previous_was_space = false;
      }
    }
    boost::algorithm::trim(normalized);
    if (!normalized.empty()) {
      unique_tags.insert(std::move(normalized));
    }
  }

  return {unique_tags.begin(), unique_tags.end()};
}

}  // namespace ui
