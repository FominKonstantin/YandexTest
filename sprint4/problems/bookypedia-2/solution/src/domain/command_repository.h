#pragma once

#include <string>
#include <vector>

#include "author.h"
#include "book.h"

namespace domain {

// Операции, которые затрагивают несколько таблиц, выполняются через этот
// интерфейс одной транзакцией.
class CommandRepository {
 public:
  virtual void AddBook(const Book& book,
                       const std::vector<std::string>& tags) = 0;
  virtual void AddBookWithAuthor(const Author& author, const Book& book,
                                 const std::vector<std::string>& tags) = 0;
  virtual bool DeleteAuthor(const std::string& author_id) = 0;
  virtual bool DeleteBook(const std::string& book_id) = 0;
  virtual bool EditAuthor(const std::string& author_id,
                          const std::string& new_name) = 0;
  virtual bool EditBook(const std::string& book_id, const std::string& title,
                        int publication_year,
                        const std::vector<std::string>& tags) = 0;

 protected:
  ~CommandRepository() = default;
};

}  // namespace domain
