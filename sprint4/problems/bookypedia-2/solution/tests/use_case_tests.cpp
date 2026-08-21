#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
  std::vector<domain::Author> saved_authors;

  void Save(const domain::Author& author) override {
    saved_authors.emplace_back(author);
  }

  std::vector<domain::Author> GetAuthors() const override {
    return saved_authors;
  }

  std::optional<domain::Author> GetAuthorByName(
      const std::string& name) const override {
    for (const auto& author : saved_authors) {
      if (author.GetName() == name) {
        return author;
      }
    }
    return std::nullopt;
  }

  bool DeleteAuthor(const std::string& author_id) override {
    for (auto it = saved_authors.begin(); it != saved_authors.end(); ++it) {
      if (it->GetId().ToString() == author_id) {
        saved_authors.erase(it);
        return true;
      }
    }
    return false;
  }

  bool UpdateAuthor(const std::string& author_id,
                    const std::string& new_name) override {
    for (auto& author : saved_authors) {
      if (author.GetId().ToString() == author_id) {
        // Создаем нового автора с обновленным именем
        // Так как Author неизменяемый, заменяем его
        saved_authors.erase(
            std::remove_if(saved_authors.begin(), saved_authors.end(),
                           [&author_id](const domain::Author& a) {
                             return a.GetId().ToString() == author_id;
                           }),
            saved_authors.end());
        saved_authors.emplace_back(domain::AuthorId::FromString(author_id),
                                   new_name);
        return true;
      }
    }
    return false;
  }
};

struct Fixture {
  MockAuthorRepository authors;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
  GIVEN("Use cases") {
    app::UseCasesImpl use_cases{authors};

    WHEN("Adding an author") {
      const auto author_name = "Joanne Rowling";
      use_cases.AddAuthor(author_name);

      THEN("author with the specified name is saved to repository") {
        REQUIRE(authors.saved_authors.size() == 1);
        CHECK(authors.saved_authors.at(0).GetName() == author_name);
        CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
      }
    }
  }
}