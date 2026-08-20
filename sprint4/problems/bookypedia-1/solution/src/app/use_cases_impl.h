#pragma once
#include "../domain/author_fwd.h"
#include "use_cases.h"

namespace app {

// Добавить этот класс перед UseCasesImpl
class DummyBookRepository : public domain::BookRepository {
 public:
  void Save(const domain::Book&) override {}
  std::vector<domain::Book> GetBooks() const override { return {}; }
  std::vector<domain::Book> GetAuthorBooks(const std::string&) const override {
    return {};
  }
};

class UseCasesImpl : public UseCases {
 public:
  explicit UseCasesImpl(domain::AuthorRepository& authors,
                        domain::BookRepository& books)
      : authors_{authors}, books_{books} {}

  // Добавить этот конструктор для тестов
  explicit UseCasesImpl(domain::AuthorRepository& authors)
      : authors_{authors}, books_{dummy_books_} {}

  void AddAuthor(const std::string& name) override;
  void AddBook(const std::string& author_id, const std::string& title,
               int publication_year) override;
  std::vector<domain::Author> GetAuthors() const override;
  std::vector<domain::Book> GetBooks() const override;
  std::vector<domain::Book> GetAuthorBooks(
      const std::string& author_id) const override;

 private:
  domain::AuthorRepository& authors_;
  domain::BookRepository& books_;
  DummyBookRepository dummy_books_;  // Добавить это поле
};

}  // namespace app