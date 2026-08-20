#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/tag.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
 public:
  explicit AuthorRepositoryImpl(pqxx::connection& connection)
      : connection_{connection} {}

  void Save(const domain::Author& author) override;
  std::vector<domain::Author> GetAuthors() const override;
  void DeleteAuthor(const std::string& author_id) override;
  void UpdateAuthor(const std::string& author_id,
                    const std::string& new_name) override;

 private:
  pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
 public:
  explicit BookRepositoryImpl(pqxx::connection& connection)
      : connection_{connection} {}

  void Save(const domain::Book& book) override;
  std::vector<domain::Book> GetBooks() const override;
  std::vector<domain::Book> GetAuthorBooks(
      const std::string& author_id) const override;
  void DeleteBook(const std::string& book_id) override;

 private:
  pqxx::connection& connection_;
};

class TagRepositoryImpl : public domain::TagRepository {
 public:
  explicit TagRepositoryImpl(pqxx::connection& connection)
      : connection_{connection} {}

  void SaveTags(const std::string& book_id,
                const std::vector<std::string>& tags) override;
  std::vector<std::string> GetBookTags(
      const std::string& book_id) const override;

 private:
  pqxx::connection& connection_;
};

class Database {
 public:
  explicit Database(pqxx::connection connection);

  AuthorRepositoryImpl& GetAuthors() & {
    EnsureTablesCreated();
    return authors_;
  }

  BookRepositoryImpl& GetBooks() & {
    EnsureTablesCreated();
    return books_;
  }

  TagRepositoryImpl& GetTags() & {
    EnsureTablesCreated();
    return tags_;
  }

 private:
  void EnsureTablesCreated();

  pqxx::connection connection_;
  AuthorRepositoryImpl authors_{connection_};
  BookRepositoryImpl books_{connection_};
  TagRepositoryImpl tags_{connection_};
  bool tables_created_{false};
};

}  // namespace postgres