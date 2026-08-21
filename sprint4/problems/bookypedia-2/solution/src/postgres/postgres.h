#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/book_tag.h"  // Новый файл

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
 public:
  explicit AuthorRepositoryImpl(pqxx::connection& connection)
      : connection_{connection} {}

  void Save(const domain::Author& author) override;
  std::vector<domain::Author> GetAuthors() const override;
  std::optional<domain::Author> GetAuthorByName(
      const std::string& name) const override;
  bool DeleteAuthor(const std::string& author_id) override;
  bool UpdateAuthor(const std::string& author_id,
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
  std::vector<domain::Book> GetBooksByTitle(
      const std::string& title) const override;
  bool DeleteBook(const std::string& book_id) override;
  bool UpdateBook(const domain::Book& book) override;
  std::optional<domain::Book> GetBookById(
      const std::string& book_id) const override;

 private:
  pqxx::connection& connection_;
};

class BookTagRepositoryImpl : public domain::BookTagRepository {
 public:
  explicit BookTagRepositoryImpl(pqxx::connection& connection)
      : connection_{connection} {}

  void SaveTags(const std::string& book_id,
                const std::vector<std::string>& tags) override;
  std::vector<std::string> GetTagsForBook(
      const std::string& book_id) const override;
  void DeleteTagsForBook(const std::string& book_id) override;
  void DeleteTagsForBooks(const std::vector<std::string>& book_ids) override;

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

  BookTagRepositoryImpl& GetBookTags() & {
    EnsureTablesCreated();
    return book_tags_;
  }

 private:
  void EnsureTablesCreated();

  pqxx::connection connection_;
  AuthorRepositoryImpl authors_{connection_};
  BookRepositoryImpl books_{connection_};
  BookTagRepositoryImpl book_tags_{connection_};
  bool tables_created_{false};
};

}  // namespace postgres