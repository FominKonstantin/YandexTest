#pragma once
#include <string>
#include <vector>

namespace domain {

class BookTagRepository {
 public:
  virtual void SaveTags(const std::string& book_id,
                        const std::vector<std::string>& tags) = 0;
  virtual std::vector<std::string> GetTagsForBook(
      const std::string& book_id) const = 0;
  virtual void DeleteTagsForBook(const std::string& book_id) = 0;
  virtual void DeleteTagsForBooks(const std::vector<std::string>& book_ids) = 0;

 protected:
  ~BookTagRepository() = default;
};

}  // namespace domain