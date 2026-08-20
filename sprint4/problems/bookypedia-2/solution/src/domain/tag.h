#pragma once
#include <string>
#include <vector>

namespace domain {

class Tag {
 public:
  explicit Tag(std::string name) : name_(std::move(name)) {}

  const std::string& GetName() const noexcept { return name_; }

 private:
  std::string name_;
};

class TagRepository {
 public:
  virtual void SaveTags(const std::string& book_id,
                        const std::vector<std::string>& tags) = 0;
  virtual std::vector<std::string> GetBookTags(
      const std::string& book_id) const = 0;

 protected:
  ~TagRepository() = default;
};

}  // namespace domain