#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace mime_types {

inline std::string GetMimeType(const std::filesystem::path &path) {
  static const std::unordered_map<std::string, std::string> mime_map = {
      {".htm", "text/html"},       {".html", "text/html"},
      {".css", "text/css"},        {".txt", "text/plain"},
      {".js", "text/javascript"},  {".json", "application/json"},
      {".xml", "application/xml"}, {".png", "image/png"},
      {".jpg", "image/jpeg"},      {".jpe", "image/jpeg"},
      {".jpeg", "image/jpeg"},     {".gif", "image/gif"},
      {".bmp", "image/bmp"},       {".ico", "image/vnd.microsoft.icon"},
      {".tiff", "image/tiff"},     {".tif", "image/tiff"},
      {".svg", "image/svg+xml"},   {".svgz", "image/svg+xml"},
      {".mp3", "audio/mpeg"},      {".webp", "image/webp"},
      {".woff", "font/woff"},      {".woff2", "font/woff2"},
      {".ttf", "font/ttf"},        {".otf", "font/otf"}};

  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  auto it = mime_map.find(extension);
  return (it != mime_map.end()) ? it->second : "application/octet-stream";
}

} // namespace mime_types