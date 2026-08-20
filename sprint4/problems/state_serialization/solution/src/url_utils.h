#pragma once

#include <cctype>
#include <string>

namespace url_utils {

inline std::string UrlDecode(const std::string &encoded) {
  std::string decoded;
  decoded.reserve(encoded.size());

  for (size_t i = 0; i < encoded.size(); ++i) {
    if (encoded[i] == '%' && i + 2 < encoded.size()) {
      unsigned char high = encoded[i + 1];
      unsigned char low = encoded[i + 2];

      if (std::isxdigit(high) && std::isxdigit(low)) {
        unsigned char value = 0;
        if (high >= '0' && high <= '9')
          value = (high - '0') * 16;
        else if (high >= 'A' && high <= 'F')
          value = (high - 'A' + 10) * 16;
        else if (high >= 'a' && high <= 'f')
          value = (high - 'a' + 10) * 16;

        if (low >= '0' && low <= '9')
          value += (low - '0');
        else if (low >= 'A' && low <= 'F')
          value += (low - 'A' + 10);
        else if (low >= 'a' && low <= 'f')
          value += (low - 'a' + 10);

        decoded.push_back(static_cast<char>(value));
        i += 2;
        continue;
      }
    }

    if (encoded[i] == '+') {
      decoded.push_back(' ');
    } else {
      decoded.push_back(encoded[i]);
    }
  }

  return decoded;
}

} // namespace url_utils