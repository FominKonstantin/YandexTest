#pragma once

#include "../domain/author_fwd.h"
#include "../domain/book_tag.h"
#include "../domain/command_repository.h"
#include "use_cases.h"

namespace app {

class DummyBookRepository : public domain::BookRepository {
 public:
  void Save(const domain::Book&) override {}
  std::vector<domain::Book> GetBooks() const override { return {}; }
  std::vector<domain::Book> GetAuthorBooks(const std::string&) const override {
    return {};
  }
  std::vector<domain::Book> GetBooksByTitle(const std::string&) const override {
    return {};
  }
  bool DeleteBook(const std::string&) override { return false; }
  bool UpdateBook(const domain::Book&) override { return false; }
  std::optional<domain::Book> GetBookById(const std::string&) const override {
    return std::nullopt;
  }
};

class DummyBookTagRepository : public domain::BookTagRepository {
 public:
  void SaveTags(const std::string&, const std::vector<std::string>&) override {}
  std::vector<std::string> GetTagsForBook(const std::string&) const override {
    return {};
  }
  void DeleteTagsForBook(const std::string&) override {}
  void DeleteTagsForBooks(const std::vector<std::string>&) override {}
};

class UseCasesImpl : public UseCases {
 public:
  explicit UseCasesImpl(domain::AuthorRepository& authors,
                        domain::BookRepository& books,
                        domain::BookTagRepository& book_tags,
                        domain::CommandRepository& commands)
      : authors_{authors},
        books_{books},
        book_tags_{book_tags},
        commands_{&commands} {}

  // Нужен существующим модульным тестам AddAuthor.
  explicit UseCasesImpl(domain::AuthorRepository& authors)
      : authors_{authors}, books_{dummy_books_}, book_tags_{dummy_book_tags_} {}

  void AddAuthor(const std::string& name) override;
  void AddBook(const std::string& author_id, const std::string& title,
               int publication_year,
               const std::vector<std::string>& tags) override;
  void AddBookWithNewAuthor(const std::string& author_name,
                            const std::string& title, int publication_year,
                            const std::vector<std::string>& tags) override;
  std::vector<domain::Author> GetAuthors() const override;
  std::vector<BookWithDetails> GetBooks() const override;
  std::vector<BookWithDetails> GetAuthorBooks(
      const std::string& author_id) const override;
  std::vector<domain::Book> GetBooksByTitle(
      const std::string& title) const override;
  bool DeleteAuthor(const std::string& author_id) override;
  bool DeleteBook(const std::string& book_id) override;
  bool EditAuthor(const std::string& author_id,
                  const std::string& new_name) override;
  bool EditBook(const std::string& book_id, const std::string& title,
                int publication_year,
                const std::vector<std::string>& tags) override;
  std::optional<domain::Author> GetAuthorByName(
      const std::string& name) const override;
  std::optional<domain::Book> GetBookById(
      const std::string& book_id) const override;
  std::optional<BookWithDetails> GetBookWithDetails(
      const std::string& book_id) const override;

 private:
  BookWithDetails BookToBookWithDetails(const domain::Book& book) const;

  domain::AuthorRepository& authors_;
  domain::BookRepository& books_;
  domain::BookTagRepository& book_tags_;
  domain::CommandRepository* commands_ = nullptr;
  DummyBookRepository dummy_books_;
  DummyBookTagRepository dummy_book_tags_;
};

}  // namespace app
