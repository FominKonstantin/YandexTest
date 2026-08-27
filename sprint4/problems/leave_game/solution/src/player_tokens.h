#pragma once

#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

#include "tagged.h"

namespace model {

namespace detail {
struct TokenTag {};
} // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
  PlayerTokens() {
    std::random_device rd;
    std::uniform_int_distribution<std::mt19937_64::result_type> dist;

    generator1_.seed(dist(rd));
    generator2_.seed(dist(rd));
  }

  Token GenerateToken() {
    uint64_t part1 = generator1_();
    uint64_t part2 = generator2_();

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << part1
       << std::setw(16) << part2;

    return Token(ss.str());
  }

private:
  std::mt19937_64 generator1_;
  std::mt19937_64 generator2_;
};

} // namespace model