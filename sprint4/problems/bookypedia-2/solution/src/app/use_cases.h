#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

struct BookWithDetails {
  domain::Book book;
  std::string author_name;
  std::vector<std::string> tags;
};

class UseCases {
 public:
  virtual void AddAuthor(const std::string& name) = 0;
  virtual void AddBook(const std::string& author_id, const std::string& title,
                       int publication_year,
                       const std::vector<std::string>& tags) = 0;
  virtual std::vector<domain::Author> GetAuthors() const = 0;
  virtual std::vector<BookWithDetails> GetBooks() const = 0;
  virtual std::vector<BookWithDetails> GetAuthorBooks(
      const std::string& author_id) const = 0;
  virtual std::vector<domain::Book> GetBooksByTitle(
      const std::string& title) const = 0;
  virtual bool DeleteAuthor(const std::string& author_id) = 0;
  virtual bool DeleteBook(const std::string& book_id) = 0;
  virtual bool EditAuthor(const std::string& author_id,
                          const std::string& new_name) = 0;
  virtual bool EditBook(const std::string& book_id, const std::string& title,
                        int publication_year,
                        const std::vector<std::string>& tags) = 0;
  virtual std::optional<domain::Author> GetAuthorByName(
      const std::string& name) const = 0;
  virtual std::optional<domain::Book> GetBookById(
      const std::string& book_id) const = 0;
  virtual std::optional<BookWithDetails> GetBookWithDetails(
      const std::string& book_id) const = 0;

 protected:
  ~UseCases() = default;
};

}  // namespace app