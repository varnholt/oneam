#include "reader.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace oneam {
namespace {
std::filesystem::path path(const std::string& value) { return std::filesystem::path(std::u8string(value.begin(), value.end())); }
std::filesystem::path file(const std::string& directory, const std::string& name) { return path(directory) / name; }
bool replace_file(const std::filesystem::path& destination, const std::string& text) {
  std::error_code ec;
  std::filesystem::create_directories(destination.parent_path(), ec);
  auto temp = destination; temp += ".tmp";
  { std::ofstream out(temp, std::ios::binary | std::ios::trunc); out << text; out.close(); if (!out) return false; }
  // SDL's rename replaces an existing destination on every supported platform.
  const auto a = temp.u8string(), b = destination.u8string();
  return SDL_RenamePath(reinterpret_cast<const char*>(a.c_str()), reinterpret_cast<const char*>(b.c_str()));
}
}
std::string book_key(const std::string& source) {
  std::string identity = source;
  if (!source.starts_with("content://")) {
    std::error_code ec;
    const auto absolute = std::filesystem::weakly_canonical(path(source), ec);
    if (!ec) { const auto utf8 = absolute.u8string(); identity.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size()); }
    const auto size = std::filesystem::file_size(path(source), ec);
    if (!ec) identity += ":" + std::to_string(size);
    const auto modified = std::filesystem::last_write_time(path(source), ec);
    if (!ec) identity += ":" + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(modified.time_since_epoch()).count());
  }
  std::uint64_t hash = 14695981039346656037ull;
  for (const unsigned char c : identity) { hash ^= c; hash *= 1099511628211ull; }
  std::ostringstream out; out << std::hex << std::setw(16) << std::setfill('0') << hash;
  return out.str();
}
std::size_t Settings::position(const std::string& key) const {
  if (key.size() != 16 || key.find_first_not_of("0123456789abcdef") != key.npos) return 0;
  std::ifstream in(file(directory_, key + ".pos"));
  std::uint64_t page = 0;
  if (!(in >> page) || page > 100000) return 0;
  return static_cast<std::size_t>(page);
}
bool Settings::save_position(const std::string& key, std::size_t page) const {
  if (key.size() != 16 || key.find_first_not_of("0123456789abcdef") != key.npos) return false;
  return replace_file(file(directory_, key + ".pos"), std::to_string(page) + "\n");
}
std::vector<std::string> Settings::recent() const {
  std::error_code ec;
  const auto length = std::filesystem::file_size(file(directory_, "recent.txt"), ec);
  if (!ec && length > 1024 * 1024) return {};
  std::ifstream in(file(directory_, "recent.txt"), std::ios::binary);
  std::vector<std::string> result;
  for (std::string line; result.size() < 20 && std::getline(in, line); ) {
    if (line.size() > 32768) break;
    if (!line.empty()) result.push_back(std::move(line));
  }
  return result;
}
bool Settings::remember(const std::string& source) const {
  if (source.empty() || source.find_first_of("\r\n") != source.npos || source.size() > 32768) return false;
  auto entries = recent();
  if (!entries.empty() && entries.front() == source) return true;
  std::erase(entries, source); entries.insert(entries.begin(), source);
  if (entries.size() > 20) entries.resize(20);
  std::string text;
  for (const auto& entry : entries) text += entry + "\n";
  return replace_file(file(directory_, "recent.txt"), text);
}
std::string Settings::title(const std::string& key) const {
  if (key.size() != 16 || key.find_first_not_of("0123456789abcdef") != key.npos) return {};
  std::ifstream input(file(directory_, key + ".name"), std::ios::binary);
  std::string value;
  for (char c; value.size() < 4096 && input.get(c) && c != '\n';) value.push_back(c);
  return value;
}
bool Settings::save_title(const std::string& key, const std::string& title) const {
  if (key.size() != 16 || key.find_first_not_of("0123456789abcdef") != key.npos) return false;
  if (title.size() > 4096 || title.find_first_of("\r\n") != title.npos) return false;
  return replace_file(file(directory_, key + ".name"), title);
}
std::string Settings::folder() const {
  std::ifstream input(file(directory_, "folder.txt"), std::ios::binary);
  std::string value;
  for (char c; value.size() < 32768 && input.get(c) && c != '\n';) value.push_back(c);
  return value;
}
bool Settings::save_folder(const std::string& path) const {
  if (path.size() > 32768 || path.find_first_of("\r\n") != path.npos) return false;
  return replace_file(file(directory_, "folder.txt"), path);
}
}
