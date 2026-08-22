#include "use_cases_impl.h"

#include <iostream>  // Добавить для std::cerr

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
  authors_.Save({AuthorId::New(), name});
}

void UseCasesImpl::AddBook(const std::string& author_id,
                           const std::string& title, int publication_year,
                           const std::vector<std::string>& tags) {
  auto book_id = BookId::New();
  Book book{book_id, author_id, title, publication_year};
  books_.Save(book);
  book_tags_.SaveTags(book_id.ToString(), tags);
}

std::vector<Author> UseCasesImpl::GetAuthors() const {
  return authors_.GetAuthors();
}

std::vector<BookWithDetails> UseCasesImpl::GetBooks() const {
  auto books = books_.GetBooks();
  std::vector<BookWithDetails> result;
  result.reserve(books.size());

  for (const auto& book : books) {
    result.push_back(BookToBookWithDetails(book));
  }

  return result;
}

std::vector<BookWithDetails> UseCasesImpl::GetAuthorBooks(
    const std::string& author_id) const {
  std::cerr << "=== GetAuthorBooks ===" << std::endl;
  std::cerr << "author_id: " << author_id << std::endl;

  auto books = books_.GetAuthorBooks(author_id);
  std::vector<BookWithDetails> result;
  result.reserve(books.size());

  std::cerr << "Found " << books.size() << " books" << std::endl;

  for (const auto& book : books) {
    result.push_back(BookToBookWithDetails(book));
  }

  std::cerr << "======================" << std::endl;

  return result;
}

std::vector<domain::Book> UseCasesImpl::GetBooksByTitle(
    const std::string& title) const {
  return books_.GetBooksByTitle(title);
}

bool UseCasesImpl::DeleteAuthor(const std::string& author_id) {
  // Сначала получаем книги автора
  auto books = books_.GetAuthorBooks(author_id);
  std::vector<std::string> book_ids;
  book_ids.reserve(books.size());
  for (const auto& book : books) {
    book_ids.push_back(book.GetId().ToString());
  }

  // Удаляем теги книг
  if (!book_ids.empty()) {
    book_tags_.DeleteTagsForBooks(book_ids);
  }

  // Удаляем книги автора
  for (const auto& book : books) {
    books_.DeleteBook(book.GetId().ToString());
  }

  // Удаляем автора
  return authors_.DeleteAuthor(author_id);
}

bool UseCasesImpl::DeleteBook(const std::string& book_id) {
  // Удаляем теги книги
  book_tags_.DeleteTagsForBook(book_id);
  // Удаляем книгу
  return books_.DeleteBook(book_id);
}

bool UseCasesImpl::EditAuthor(const std::string& author_id,
                              const std::string& new_name) {
  std::cerr << "=== EditAuthor ===" << std::endl;
  std::cerr << "author_id: " << author_id << std::endl;
  std::cerr << "new_name: " << new_name << std::endl;

  bool result = authors_.UpdateAuthor(author_id, new_name);

  std::cerr << "result: " << (result ? "true" : "false") << std::endl;
  std::cerr << "==================" << std::endl;

  return result;
}

bool UseCasesImpl::EditBook(const std::string& book_id,
                            const std::string& title, int publication_year,
                            const std::vector<std::string>& tags) {
  auto book_opt = books_.GetBookById(book_id);
  if (!book_opt.has_value()) {
    return false;
  }

  auto book = book_opt.value();
  Book updated_book{book.GetId(), book.GetAuthorId(), title, publication_year};
  bool result = books_.UpdateBook(updated_book);
  if (result) {
    if (!tags.empty()) {
      book_tags_.DeleteTagsForBook(book_id);
      book_tags_.SaveTags(book_id, tags);
    }
  }
  return result;
}

std::optional<domain::Author> UseCasesImpl::GetAuthorByName(
    const std::string& name) const {
  std::cerr << "=== GetAuthorByName ===" << std::endl;
  std::cerr << "Searching for name: " << name << std::endl;

  auto result = authors_.GetAuthorByName(name);

  if (result.has_value()) {
    std::cerr << "Found: ID=" << result->GetId().ToString()
              << ", Name=" << result->GetName() << std::endl;
  } else {
    std::cerr << "Not found!" << std::endl;
  }
  std::cerr << "========================" << std::endl;

  return result;
}

std::optional<domain::Book> UseCasesImpl::GetBookById(
    const std::string& book_id) const {
  return books_.GetBookById(book_id);
}

std::optional<BookWithDetails> UseCasesImpl::GetBookWithDetails(
    const std::string& book_id) const {
  auto book_opt = books_.GetBookById(book_id);
  if (!book_opt.has_value()) {
    return std::nullopt;
  }
  return BookToBookWithDetails(book_opt.value());
}

BookWithDetails UseCasesImpl::BookToBookWithDetails(
    const domain::Book& book) const {
  // Получаем имя автора
  std::string author_name;
  auto authors = authors_.GetAuthors();

  std::cerr << "=== BookToBookWithDetails ===" << std::endl;
  std::cerr << "book_id: " << book.GetId().ToString() << std::endl;
  std::cerr << "book author_id: " << book.GetAuthorId() << std::endl;
  std::cerr << "All authors in DB:" << std::endl;

  for (const auto& author : authors) {
    std::cerr << "  Author ID: " << author.GetId().ToString()
              << " -> Name: " << author.GetName() << std::endl;
    if (author.GetId().ToString() == book.GetAuthorId()) {
      author_name = author.GetName();
      std::cerr << "  >>> FOUND! author_name = " << author_name << std::endl;
    }
  }

  std::cerr << "Final author_name: " << author_name << std::endl;
  std::cerr << "=============================" << std::endl;

  // Получаем теги
  auto tags = book_tags_.GetTagsForBook(book.GetId().ToString());

  return BookWithDetails{book, author_name, tags};
}

}  // namespace app