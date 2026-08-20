#pragma once

#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

class UseCases {
 public:
  virtual void AddAuthor(const std::string& name) = 0;
  virtual void AddBook(const std::string& author_id, const std::string& title,
                       int publication_year,
                       const std::vector<std::string>& tags) = 0;
  virtual std::vector<domain::Author> GetAuthors() const = 0;
  virtual std::vector<domain::Book> GetBooks() const = 0;
  virtual std::vector<domain::Book> GetAuthorBooks(
      const std::string& author_id) const = 0;
  virtual void DeleteAuthor(const std::string& author_id) = 0;
  virtual void UpdateAuthor(const std::string& author_id,
                            const std::string& new_name) = 0;
  virtual void DeleteBook(const std::string& book_id) = 0;
  virtual void UpdateBook(const std::string& book_id, const std::string& title,
                          int publication_year,
                          const std::vector<std::string>& tags) = 0;
  virtual std::vector<std::string> GetBookTags(
      const std::string& book_id) const = 0;

 protected:
  ~UseCases() = default;
};

}  // namespace app