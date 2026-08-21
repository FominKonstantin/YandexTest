#include "use_cases_impl.h"

#include <stdexcept>

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
  books_.Save({book_id, author_id, title, publication_year, tags});
  if (!tags.empty()) {
    tags_.SaveTags(book_id.ToString(), tags);
  }
}

std::vector<Author> UseCasesImpl::GetAuthors() const {
  return authors_.GetAuthors();
}

std::vector<Book> UseCasesImpl::GetBooks() const { return books_.GetBooks(); }

std::vector<Book> UseCasesImpl::GetAuthorBooks(
    const std::string& author_id) const {
  return books_.GetAuthorBooks(author_id);
}

void UseCasesImpl::DeleteAuthor(const std::string& author_id) {
  // Проверяем, существует ли автор
  auto authors = authors_.GetAuthors();
  bool found = false;
  for (const auto& author : authors) {
    if (author.GetId().ToString() == author_id) {
      found = true;
      break;
    }
  }
  if (!found) {
    throw std::runtime_error("Author not found");
  }
  authors_.DeleteAuthor(author_id);
}

void UseCasesImpl::UpdateAuthor(const std::string& author_id,
                                const std::string& new_name) {
  // Проверяем, существует ли автор
  auto authors = authors_.GetAuthors();
  bool found = false;
  for (const auto& author : authors) {
    if (author.GetId().ToString() == author_id) {
      found = true;
      break;
    }
  }
  if (!found) {
    throw std::runtime_error("Author not found");
  }
  authors_.UpdateAuthor(author_id, new_name);
}

void UseCasesImpl::DeleteBook(const std::string& book_id) {
  // Проверяем, существует ли книга
  auto books = books_.GetBooks();
  bool found = false;
  for (const auto& book : books) {
    if (book.GetId().ToString() == book_id) {
      found = true;
      break;
    }
  }
  if (!found) {
    throw std::runtime_error("Book not found");
  }
  books_.DeleteBook(book_id);
}

void UseCasesImpl::UpdateBook(const std::string& book_id,
                              const std::string& title, int publication_year,
                              const std::vector<std::string>& tags) {
  // Проверяем, существует ли книга
  auto books = books_.GetBooks();
  bool found = false;
  for (const auto& book : books) {
    if (book.GetId().ToString() == book_id) {
      found = true;
      break;
    }
  }
  if (!found) {
    throw std::runtime_error("Book not found");
  }
  books_.UpdateBook(book_id, title, publication_year);
  tags_.SaveTags(book_id, tags);
}

std::vector<std::string> UseCasesImpl::GetBookTags(
    const std::string& book_id) const {
  return tags_.GetBookTags(book_id);
}

}  // namespace app