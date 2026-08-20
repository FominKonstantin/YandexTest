#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <pqxx/pqxx>
#include <string>

using json = nlohmann::json;
using namespace std::literals;
using pqxx::operator"" _zv;

class BookManager {
 public:
  explicit BookManager(const std::string& conn_string) : conn_(conn_string) {
    // Сначала создаём таблицу
    create_table_if_not_exists();
    // Затем подготавливаем запросы
    prepare_statements();
  }

  void process_commands() {
    std::string line;
    while (std::getline(std::cin, line)) {
      if (line.empty()) continue;

      try {
        json request = json::parse(line);
        std::string action = request["action"];

        if (action == "add_book") {
          handle_add_book(request["payload"]);
        } else if (action == "all_books") {
          handle_all_books();
        } else if (action == "exit") {
          break;
        }
      } catch (const std::exception& e) {
        std::cerr << "Error processing request: " << e.what() << std::endl;
      }
    }
  }

 private:
  pqxx::connection conn_;

  void create_table_if_not_exists() {
    pqxx::work w(conn_);
    w.exec(
        R"(
                CREATE TABLE IF NOT EXISTS books (
                    id SERIAL PRIMARY KEY,
                    title varchar(100) NOT NULL,
                    author varchar(100) NOT NULL,
                    year integer NOT NULL,
                    ISBN char(13) UNIQUE
                )
            )"_zv);
    w.commit();
  }

  void prepare_statements() {
    // Подготовка запроса для добавления книги
    conn_.prepare(
        "add_book",
        "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)"_zv);

    // Подготовка запроса для получения всех книг
    conn_.prepare("all_books",
                  R"(
                SELECT id, title, author, year, ISBN 
                FROM books 
                ORDER BY year DESC, title ASC, author ASC, ISBN ASC
            )"_zv);
  }

  void handle_add_book(const json& payload) {
    try {
      pqxx::work w(conn_);

      std::string title = payload["title"];
      std::string author = payload["author"];
      int year = payload["year"];

      // Обработка ISBN (может быть null)
      if (payload.contains("ISBN") && !payload["ISBN"].is_null()) {
        std::string isbn = payload["ISBN"];
        w.exec_prepared("add_book", title, author, year, isbn);
      } else {
        // ISBN - null
        w.exec_prepared("add_book", title, author, year, nullptr);
      }

      w.commit();

      json response = {{"result", true}};
      std::cout << response.dump() << std::endl;

    } catch (const pqxx::sql_error& e) {
      // Ошибка при добавлении дублирующегося ISBN
      json response = {{"result", false}};
      std::cout << response.dump() << std::endl;
    } catch (const std::exception& e) {
      json response = {{"result", false}};
      std::cout << response.dump() << std::endl;
    }
  }

  void handle_all_books() {
    pqxx::read_transaction r(conn_);

    json books_array = json::array();

    auto result = r.exec_prepared("all_books");

    for (const auto& row : result) {
      json book;
      book["id"] = row["id"].as<int>();
      book["title"] = row["title"].as<std::string>();
      book["author"] = row["author"].as<std::string>();
      book["year"] = row["year"].as<int>();

      // Обработка NULL для ISBN
      if (row["ISBN"].is_null()) {
        book["ISBN"] = nullptr;
      } else {
        book["ISBN"] = row["ISBN"].as<std::string>();
      }

      books_array.push_back(book);
    }

    std::cout << books_array.dump() << std::endl;
  }
};

int main(int argc, const char* argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: book_manager <conn-string>" << std::endl;
      return EXIT_FAILURE;
    }

    BookManager manager(argv[1]);
    manager.process_commands();

    return EXIT_SUCCESS;

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}