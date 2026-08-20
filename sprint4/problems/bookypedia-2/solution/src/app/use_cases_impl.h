#pragma once
#include "../domain/author_fwd.h"
#include "../domain/tag.h"
#include "use_cases.h"

namespace app {

class DummyBookRepository : public domain::BookRepository {
 public:
  void Save(const domain::Book&) override {}
  std::vector<domain::Book> GetBooks() const override { return {}; }
  std::vector<domain::Book> GetAuthorBooks(const std::string&) const override {
    return {};
  }
  void DeleteBook(const std::string&) override {}
  void UpdateBook(const std::string&, const std::string&, int) override {}
};

class DummyTagRepository : public domain::TagRepository {
 public:
  void SaveTags(const std::string&, const std::vector<std::string>&) override {}
  std::vector<std::string> GetBookTags(const std::string&) const override {
    return {};
  }
};

class UseCasesImpl : public UseCases {
 public:
  explicit UseCasesImpl(domain::AuthorRepository& authors,
                        domain::BookRepository& books,
                        domain::TagRepository& tags)
      : authors_{authors}, books_{books}, tags_{tags} {}

  explicit UseCasesImpl(domain::AuthorRepository& authors)
      : authors_{authors}, books_{dummy_books_}, tags_{dummy_tags_} {}

  void AddAuthor(const std::string& name) override;
  void AddBook(const std::string& author_id, const std::string& title,
               int publication_year,
               const std::vector<std::string>& tags) override;
  std::vector<domain::Author> GetAuthors() const override;
  std::vector<domain::Book> GetBooks() const override;
  std::vector<domain::Book> GetAuthorBooks(
      const std::string& author_id) const override;
  void DeleteAuthor(const std::string& author_id) override;
  void UpdateAuthor(const std::string& author_id,
                    const std::string& new_name) override;
  void DeleteBook(const std::string& book_id) override;
  void UpdateBook(const std::string& book_id, const std::string& title,
                  int publication_year,
                  const std::vector<std::string>& tags) override;
  std::vector<std::string> GetBookTags(
      const std::string& book_id) const override;

 private:
  domain::AuthorRepository& authors_;
  domain::BookRepository& books_;
  domain::TagRepository& tags_;
  DummyBookRepository dummy_books_;
  DummyTagRepository dummy_tags_;
};

}  // namespace app