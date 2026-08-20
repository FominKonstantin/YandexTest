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

void AuthorRepositoryImpl::DeleteAuthor(const std::string& author_id) {
  pqxx::work work{connection_};
  work.exec_params(R"(
DELETE FROM authors WHERE id = $1;
)"_zv,
                   author_id);
  work.commit();
}

void AuthorRepositoryImpl::UpdateAuthor(const std::string& author_id,
                                        const std::string& new_name) {
  pqxx::work work{connection_};
  work.exec_params(R"(
UPDATE authors SET name = $2 WHERE id = $1;
)"_zv,
                   author_id, new_name);
  work.commit();
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
SELECT b.id, b.author_id, b.title, b.publication_year 
FROM books b
JOIN authors a ON b.author_id = a.id
ORDER BY b.title, a.name, b.publication_year;
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

void BookRepositoryImpl::DeleteBook(const std::string& book_id) {
  pqxx::work work{connection_};
  work.exec_params(R"(
DELETE FROM books WHERE id = $1;
)"_zv,
                   book_id);
  work.commit();
}

void BookRepositoryImpl::UpdateBook(const std::string& book_id,
                                    const std::string& title,
                                    int publication_year) {
  pqxx::work work{connection_};
  work.exec_params(R"(
UPDATE books SET title = $2, publication_year = $3 WHERE id = $1;
)"_zv,
                   book_id, title, publication_year);
  work.commit();
}

void TagRepositoryImpl::SaveTags(const std::string& book_id,
                                 const std::vector<std::string>& tags) {
  pqxx::work work{connection_};

  work.exec_params(R"(
DELETE FROM book_tags WHERE book_id = $1;
)"_zv,
                   book_id);

  for (const auto& tag : tags) {
    work.exec_params(R"(
INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);
)"_zv,
                     book_id, tag);
  }

  work.commit();
}

std::vector<std::string> TagRepositoryImpl::GetBookTags(
    const std::string& book_id) const {
  pqxx::read_transaction r{connection_};
  auto res = r.exec_params(R"(
SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag;
)"_zv,
                           book_id);

  std::vector<std::string> tags;
  tags.reserve(res.size());
  for (size_t i = 0; i < res.size(); ++i) {
    tags.push_back(res[i][0].as<std::string>());
  }
  return tags;
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