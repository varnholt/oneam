#pragma once
#include <SDL3/SDL.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace oneam
{
template <class T>
using Result = std::expected<T, std::string>;
using Cancel = std::function<bool()>;
inline constexpr std::size_t max_encoded_bytes = 64 * 1024 * 1024;
inline constexpr std::size_t max_decoded_bytes = 96 * 1024 * 1024;
inline constexpr std::size_t cache_budget = 128 * 1024 * 1024;
bool natural_less(std::string_view a, std::string_view b);
bool is_image(std::string_view name);
bool is_book(std::string_view name);
enum class ReadingMode
{
   Single,
   Spread,
   Continuous
};
struct Tone
{
   float brightness = 0, contrast = 1, gamma = 1;
   bool operator==(const Tone&) const = default;
};
struct ViewSettings
{
   ReadingMode layout = ReadingMode::Single;
   bool manga = false, cover_alone = true, trim = false, fit_width = false, controls_hidden = false;
   int rotation = 0;
   float zoom = 1;
   Tone tone;
   bool operator==(const ViewSettings&) const = default;
};
struct Page
{
   std::string name;
   std::size_t entry = 0;
};
std::string display_name(const std::string& source);
struct Metadata
{
   std::string title, series, number, writer, summary;
   bool manga_rtl = false;
};
Metadata parse_comic_info(std::string_view xml);
std::string metadata_title(const Metadata& metadata);
struct Book
{
   std::string source;
   std::vector<Page> pages;
   std::string title;
   std::string format;
   Metadata metadata{};
};
struct Image
{
   int width = 0, height = 0;
   std::vector<std::uint8_t> pixels;
   SDL_Rect content{};
};
Result<Image> decode_image(std::span<const std::uint8_t> bytes);

// Stream callbacks accept desktop UTF-8 paths and Android content:// URIs.
class Archive
{
public:
   Archive();
   ~Archive();
   Archive(const Archive&) = delete;
   Archive& operator=(const Archive&) = delete;
   Result<Book> index(const std::string& source, const Cancel& cancel = {});
   Result<std::vector<std::uint8_t>> read(const Book& book, std::size_t page, const Cancel& cancel = {});
   void reset();

private:
   struct Impl;
   std::unique_ptr<Impl> impl_;
};

class ImageCache
{
public:
   explicit ImageCache(std::size_t budget = cache_budget) : budget_(budget)
   {
   }
   std::shared_ptr<const Image> get(std::size_t page);
   void put(std::size_t page, std::shared_ptr<const Image> image);
   void clear();
   std::size_t bytes() const
   {
      return bytes_;
   }

private:
   struct Entry
   {
      std::size_t page;
      std::shared_ptr<const Image> image;
   };
   std::list<Entry> entries_;
   std::size_t bytes_ = 0, budget_;
};

struct Request
{
   std::string source;
   std::size_t page = 0;
   std::uint64_t generation = 0;
   bool reload = false;
   int thumbnail = 0;
};
struct Update
{
   std::uint64_t generation = 0;
   std::shared_ptr<const Book> book;
   std::shared_ptr<const Image> image;
   std::size_t page = 0;
   std::string error;
   double milliseconds = 0;
   bool cached = false;
};
class Reader
{
public:
   explicit Reader(bool prefetch = true, std::size_t budget = cache_budget);
   ~Reader();
   std::uint64_t request(std::string source, std::size_t page, bool reload = false, int thumbnail = 0);
   std::optional<Update> poll();
   void trim_cache();

private:
   static int run(void* self);
   void work();
   std::mutex mutex_;
   std::condition_variable ready_;
   std::optional<Request> pending_;
   std::optional<Update> update_;
   bool trim_requested_ = false;
   std::atomic<std::uint64_t> generation_{0};
   std::atomic<bool> stopping_{false};
   SDL_Thread* thread_ = nullptr;
   bool prefetch_;
   std::size_t budget_;
};

// Fingerprint includes full source identity and file metadata, never just basename.
std::string book_key(const std::string& source);
class Settings
{
public:
   explicit Settings(std::string directory) : directory_(std::move(directory))
   {
   }
   std::size_t position(const std::string& key) const;
   bool save_position(const std::string& key, std::size_t page) const;
   std::vector<std::string> recent() const;
   bool remember(const std::string& source) const;
   std::string title(const std::string& key) const;
   bool save_title(const std::string& key, const std::string& title) const;
   std::string folder() const;
   bool save_folder(const std::string& path) const;
   struct Progress
   {
      std::size_t page = 0, total = 0;
      bool finished = false;
   };
   Progress progress(const std::string& key) const;
   bool save_progress(const std::string& key, Progress progress) const;
   std::vector<std::size_t> bookmarks(const std::string& key) const;
   bool save_bookmarks(const std::string& key, std::vector<std::size_t> pages) const;
   std::string preference(const std::string& name) const;
   bool save_preference(const std::string& name, const std::string& value) const;
   std::optional<ViewSettings> view(const std::string& key) const;
   bool save_view(const std::string& key, const ViewSettings& view) const;

private:
   std::string directory_;
};
}  // namespace oneam
