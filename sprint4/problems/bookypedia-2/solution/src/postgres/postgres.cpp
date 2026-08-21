#include "postgres.h"

#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
  pqxx::work work{connection_};
  work.exec_params(
      R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
      author.GetId().ToString(), author.GetName());
  work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAuthors() const {
  pqxx::read_transaction r{connection_};
  auto res = r.exec(R"(
SELECT id, name 
FROM authors 
ORDER BY name;
)"_zv);

  std::vector<domain::Author> authors;
  authors.reserve(res.size());

  for (size_t i = 0; i < res.size(); ++i) {
    authors.emplace_back(
        domain::AuthorId::FromString(res[i][0].as<std::string>()),
        res[i][1].as<std::string>());
  }

  return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::GetAuthorByName(
    const std::string& name) const {
  pqxx::read_transaction r{connection_};
  auto res = r.exec_params(
      R"(
SELECT id, name 
FROM authors 
WHERE name = $1;
)"_zv,
      name);

  if (res.empty()) {
    return std::nullopt;
  }

  return domain::Author(
      domain::AuthorId::FromString(res[0][0].as<std::string>()),
      res[0][1].as<std::string>());
}

bool AuthorRepositoryImpl::DeleteAuthor(const std::string& author_id) {
  pqxx::work work{connection_};
  auto res = work.exec_params(
      R"(
DELETE FROM authors 
WHERE id = $1;
)"_zv,
      author_id);
  work.commit();
  return res.affected_rows() > 0;
}

bool AuthorRepositoryImpl::UpdateAuthor(const std::string& author_id,
                                        const std::string& new_name) {
  pqxx::work work{connection_};
  auto res = work.exec_params(
      R"(
UPDATE authors 
SET name = $1 
WHERE id = $2;
)"_zv,
      new_name, author_id);
  work.commit();
  return res.affected_rows() > 0;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
  pqxx::work work{connection_};
  work.exec_params(
      R"(
INSERT INTO books (id, author_id, title, publication_year) 
VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET 
    author_id=$2, 
    title=$3, 
    publication_year=$4;
)"_zv,
      book.GetId().ToString(), book.GetAuthorId(), book.GetTitle(),
      book.GetPublicationYear());
  work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetBooks() const {
  pqxx::read_transaction r{connection_};
  auto res = r.exec(R"(
SELECT id, author_id, title, publication_year 
FROM books 
ORDER BY title;
)"_zv);

  std::vector<domain::Book> books;
  books.reserve(res.size());

  for (size_t i = 0; i < res.size(); ++i) {
    books.emplace_back(domain::BookId::FromString(res[i][0].as<std::string>()),
                       res[i][1].as<std::string>(), res[i][2].as<std::string>(),
                       res[i][3].as<int>());
  }

  return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetAuthorBooks(
    const std::string& author_id) const {
  pqxx::read_transaction r{connection_};
  auto res = r.exec_params(
      R"(
SELECT id, author_id, title, publication_year 
FROM books 
WHERE author_id = $1
ORDER BY publication_year, title;
)"_zv,
      author_id);

  std::vector<domain::Book> books;
  books.reserve(res.size());

  for (size_t i = 0; i < res.size(); ++i) {
    books.emplace_back(domain::BookId::FromString(res[i][0].as<std::string>()),
                       res[i][1].as<std::string>(), res[i][2].as<std::string>(),
                       res[i][3].as<int>());
  }

  return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooksByTitle(
    const std::string& title) const {
  pqxx::read_transaction r{connection_};
  auto res = r.exec_params(
      R"(
SELECT b.id, b.author_id, b.title, b.publication_year, a.name
FROM books b
JOIN authors a ON b.author_id = a.id
WHERE b.title = $1
ORDER BY a.name, b.publication_year;
)"_zv,
      title);

  std::vector<domain::Book> books;
  books.reserve(res.size());

  for (size_t i = 0; i < res.size(); ++i) {
    books.emplace_back(domain::BookId::FromString(res[i][0].as<std::string>()),
                       res[i][1].as<std::string>(), res[i][2].as<std::string>(),
                       res[i][3].as<int>());
  }

  return books;
}

bool BookRepositoryImpl::DeleteBook(const std::string& book_id) {
  pqxx::work work{connection_};
  auto res = work.exec_params(
      R"(
DELETE FROM books 
WHERE id = $1;
)"_zv,
      book_id);
  work.commit();
  return res.affected_rows() > 0;
}

bool BookRepositoryImpl::UpdateBook(const domain::Book& book) {
  pqxx::work work{connection_};
  auto res = work.exec_params(
      R"(
UPDATE books 
SET author_id = $1, title = $2, publication_year = $3 
WHERE id = $4;
)"_zv,
      book.GetAuthorId(), book.GetTitle(), book.GetPublicationYear(),
      book.GetId().ToString());
  work.commit();
  return res.affected_rows() > 0;
}

std::optional<domain::Book> BookRepositoryImpl::GetBookById(
    const std::string& book_id) const {
  pqxx::read_transaction r{connection_};
  auto res = r.exec_params(
      R"(
SELECT id, author_id, title, publication_year 
FROM books 
WHERE id = $1;
)"_zv,
      book_id);

  if (res.empty()) {
    return std::nullopt;
  }

  return domain::Book(domain::BookId::FromString(res[0][0].as<std::string>()),
                      res[0][1].as<std::string>(), res[0][2].as<std::string>(),
                      res[0][3].as<int>());
}

void BookTagRepositoryImpl::SaveTags(const std::string& book_id,
                                     const std::vector<std::string>& tags) {
  if (tags.empty()) return;

  pqxx::work work{connection_};
  for (const auto& tag : tags) {
    work.exec_params(
        R"(
INSERT INTO book_tags (book_id, tag) 
VALUES ($1, $2)
ON CONFLICT (book_id, tag) DO NOTHING;
)"_zv,
        book_id, tag);
  }
  work.commit();
}

std::vector<std::string> BookTagRepositoryImpl::GetTagsForBook(
    const std::string& book_id) const {
  pqxx::read_transaction r{connection_};
  auto res = r.exec_params(
      R"(
SELECT tag 
FROM book_tags 
WHERE book_id = $1
ORDER BY tag;
)"_zv,
      book_id);

  std::vector<std::string> tags;
  tags.reserve(res.size());

  for (size_t i = 0; i < res.size(); ++i) {
    tags.push_back(res[i][0].as<std::string>());
  }

  return tags;
}

void BookTagRepositoryImpl::DeleteTagsForBook(const std::string& book_id) {
  pqxx::work work{connection_};
  work.exec_params(
      R"(
DELETE FROM book_tags 
WHERE book_id = $1;
)"_zv,
      book_id);
  work.commit();
}

void BookTagRepositoryImpl::DeleteTagsForBooks(
    const std::vector<std::string>& book_ids) {
  if (book_ids.empty()) return;

  pqxx::work work{connection_};
  for (const auto& book_id : book_ids) {
    work.exec_params(
        R"(
DELETE FROM book_tags 
WHERE book_id = $1;
)"_zv,
        book_id);
  }
  work.commit();
}

void Database::EnsureTablesCreated() {
  if (tables_created_) return;

  pqxx::work work{connection_};
  work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);

  work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title varchar(100) NOT NULL,
    publication_year integer
);
)"_zv);

  work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag varchar(30) NOT NULL,
    PRIMARY KEY (book_id, tag)
);
)"_zv);

  work.commit();
  tables_created_ = true;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {}

}  // namespace postgres