#include "use_cases_impl.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
  authors_.Save({AuthorId::New(), name});
}

void UseCasesImpl::AddBook(const std::string& author_id,
                           const std::string& title, int publication_year,
                           const std::vector<std::string>& tags) {
  Book book{BookId::New(), author_id, title, publication_year};
  if (commands_) {
    commands_->AddBook(book, tags);
    return;
  }

  books_.Save(book);
  book_tags_.SaveTags(book.GetId().ToString(), tags);
}

void UseCasesImpl::AddBookWithNewAuthor(
    const std::string& author_name, const std::string& title,
    int publication_year, const std::vector<std::string>& tags) {
  Author author{AuthorId::New(), author_name};
  Book book{BookId::New(), author.GetId().ToString(), title, publication_year};

  if (commands_) {
    commands_->AddBookWithAuthor(author, book, tags);
    return;
  }

  authors_.Save(author);
  books_.Save(book);
  book_tags_.SaveTags(book.GetId().ToString(), tags);
}

std::vector<Author> UseCasesImpl::GetAuthors() const {
  return authors_.GetAuthors();
}

std::vector<BookWithDetails> UseCasesImpl::GetBooks() const {
  auto books = books_.GetBooks();
  const auto authors = authors_.GetAuthors();

  std::unordered_map<std::string, std::string> author_names;
  author_names.reserve(authors.size());
  for (const auto& author : authors) {
    author_names.emplace(author.GetId().ToString(), author.GetName());
  }

  std::vector<BookWithDetails> result;
  result.reserve(books.size());
  for (const auto& book : books) {
    std::string author_name;
    if (const auto it = author_names.find(book.GetAuthorId());
        it != author_names.end()) {
      author_name = it->second;
    }
    result.push_back({book, std::move(author_name),
                      book_tags_.GetTagsForBook(book.GetId().ToString())});
  }

  std::sort(result.begin(), result.end(),
            [](const BookWithDetails& lhs, const BookWithDetails& rhs) {
              if (lhs.book.GetTitle() != rhs.book.GetTitle()) {
                return lhs.book.GetTitle() < rhs.book.GetTitle();
              }
              if (lhs.author_name != rhs.author_name) {
                return lhs.author_name < rhs.author_name;
              }
              if (lhs.book.GetPublicationYear() !=
                  rhs.book.GetPublicationYear()) {
                return lhs.book.GetPublicationYear() <
                       rhs.book.GetPublicationYear();
              }
              return lhs.book.GetId().ToString() < rhs.book.GetId().ToString();
            });

  return result;
}

std::vector<BookWithDetails> UseCasesImpl::GetAuthorBooks(
    const std::string& author_id) const {
  auto books = books_.GetAuthorBooks(author_id);
  std::vector<BookWithDetails> result;
  result.reserve(books.size());
  for (const auto& book : books) {
    result.push_back(BookToBookWithDetails(book));
  }
  return result;
}

std::vector<domain::Book> UseCasesImpl::GetBooksByTitle(
    const std::string& title) const {
  return books_.GetBooksByTitle(title);
}

bool UseCasesImpl::DeleteAuthor(const std::string& author_id) {
  if (commands_) {
    return commands_->DeleteAuthor(author_id);
  }

  const auto books = books_.GetAuthorBooks(author_id);
  std::vector<std::string> book_ids;
  book_ids.reserve(books.size());
  for (const auto& book : books) {
    book_ids.push_back(book.GetId().ToString());
  }
  book_tags_.DeleteTagsForBooks(book_ids);
  for (const auto& book : books) {
    books_.DeleteBook(book.GetId().ToString());
  }
  return authors_.DeleteAuthor(author_id);
}

bool UseCasesImpl::DeleteBook(const std::string& book_id) {
  if (commands_) {
    return commands_->DeleteBook(book_id);
  }

  book_tags_.DeleteTagsForBook(book_id);
  return books_.DeleteBook(book_id);
}

bool UseCasesImpl::EditAuthor(const std::string& author_id,
                              const std::string& new_name) {
  if (commands_) {
    return commands_->EditAuthor(author_id, new_name);
  }
  return authors_.UpdateAuthor(author_id, new_name);
}

bool UseCasesImpl::EditBook(const std::string& book_id,
                            const std::string& title, int publication_year,
                            const std::vector<std::string>& tags) {
  if (commands_) {
    return commands_->EditBook(book_id, title, publication_year, tags);
  }

  const auto book = books_.GetBookById(book_id);
  if (!book) {
    return false;
  }
  const Book updated_book{book->GetId(), book->GetAuthorId(), title,
                          publication_year};
  if (!books_.UpdateBook(updated_book)) {
    return false;
  }
  book_tags_.DeleteTagsForBook(book_id);
  book_tags_.SaveTags(book_id, tags);
  return true;
}

std::optional<domain::Author> UseCasesImpl::GetAuthorByName(
    const std::string& name) const {
  return authors_.GetAuthorByName(name);
}

std::optional<domain::Book> UseCasesImpl::GetBookById(
    const std::string& book_id) const {
  return books_.GetBookById(book_id);
}

std::optional<BookWithDetails> UseCasesImpl::GetBookWithDetails(
    const std::string& book_id) const {
  const auto book = books_.GetBookById(book_id);
  if (!book) {
    return std::nullopt;
  }
  return BookToBookWithDetails(*book);
}

BookWithDetails UseCasesImpl::BookToBookWithDetails(
    const domain::Book& book) const {
  std::string author_name;
  const auto authors = authors_.GetAuthors();
  for (const auto& author : authors) {
    if (author.GetId().ToString() == book.GetAuthorId()) {
      author_name = author.GetName();
      break;
    }
  }

  return {book, std::move(author_name),
          book_tags_.GetTagsForBook(book.GetId().ToString())};
}

}  // namespace app
