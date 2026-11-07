#include "reader.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <stdexcept>

namespace oneam {
namespace {
unsigned char fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
bool digit(char c) { return c >= '0' && c <= '9'; }
std::string extension(std::string_view name) {
  const auto dot = name.find_last_of('.');
  if (dot == name.npos) return {};
  std::string ext(name.substr(dot));
  for (char& c : ext) c = static_cast<char>(fold(static_cast<unsigned char>(c)));
  return ext;
}
}
bool natural_less(std::string_view a, std::string_view b) {
  std::size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    if (digit(a[i]) && digit(b[j])) {
      const auto ai = i, bj = j;
      while (i < a.size() && a[i] == '0') ++i;
      while (j < b.size() && b[j] == '0') ++j;
      const auto as = i, bs = j;
      while (i < a.size() && digit(a[i])) ++i;
      while (j < b.size() && digit(b[j])) ++j;
      if (i - as != j - bs) return i - as < j - bs;
      const auto cmp = a.substr(as, i - as).compare(b.substr(bs, j - bs));
      if (cmp) return cmp < 0;
      if (i - ai != j - bj) return i - ai < j - bj;
    } else {
      const auto ac = fold(static_cast<unsigned char>(a[i++]));
      const auto bc = fold(static_cast<unsigned char>(b[j++]));
      if (ac != bc) return ac < bc;
    }
  }
  if (i != a.size() || j != b.size()) return i == a.size();
  return a < b;
}
bool is_image(std::string_view name) {
  const auto ext = extension(name);
  return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".gif";
}
bool is_book(std::string_view name) {
  const auto ext = extension(name);
  return ext == ".cbz" || ext == ".cbr" || ext == ".cb7" || ext == ".cbt" ||
    ext == ".zip" || ext == ".rar" || ext == ".7z" || ext == ".tar";
}
std::shared_ptr<const Image> ImageCache::get(std::size_t page) {
  auto it = std::find_if(entries_.begin(), entries_.end(), [page](const Entry& e) { return e.page == page; });
  if (it == entries_.end()) return {};
  entries_.splice(entries_.begin(), entries_, it);
  return entries_.front().image;
}
void ImageCache::put(std::size_t page, std::shared_ptr<const Image> image) {
  if (!image) return;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) if (it->page == page) {
    bytes_ -= it->image->pixels.size(); entries_.erase(it); break;
  }
  if (image->pixels.size() > budget_) return;
  while (bytes_ + image->pixels.size() > budget_ && !entries_.empty()) {
    bytes_ -= entries_.back().image->pixels.size(); entries_.pop_back();
  }
  bytes_ += image->pixels.size(); entries_.push_front({page, std::move(image)});
}
void ImageCache::clear() { entries_.clear(); bytes_ = 0; }
Reader::Reader(bool prefetch, std::size_t budget) : prefetch_(prefetch), budget_(budget) {
  thread_ = SDL_CreateThread(run, "oneam-reader", this);
  if (!thread_) throw std::runtime_error(SDL_GetError());
}
Reader::~Reader() {
  stopping_ = true; ++generation_; ready_.notify_all(); SDL_WaitThread(thread_, nullptr);
}
std::uint64_t Reader::request(std::string source, std::size_t page, bool reload) {
  std::lock_guard lock(mutex_);
  const auto generation = ++generation_;
  pending_ = Request{std::move(source), page, generation, reload}; update_.reset(); ready_.notify_one();
  return generation;
}
std::optional<Update> Reader::poll() {
  std::lock_guard lock(mutex_);
  auto result = std::move(update_); update_.reset(); return result;
}
void Reader::trim_cache() {
  std::lock_guard lock(mutex_); trim_requested_ = true; ready_.notify_one();
}
int Reader::run(void* self) { static_cast<Reader*>(self)->work(); return 0; }
void Reader::work() {
  Archive archive;
  ImageCache cache(budget_);
  std::shared_ptr<const Book> book;
  while (!stopping_) {
    Request request;
    {
      std::unique_lock lock(mutex_);
      ready_.wait(lock, [this] { return stopping_ || trim_requested_ || pending_.has_value(); });
      if (stopping_) break;
      if (trim_requested_) { cache.clear(); archive.reset(); trim_requested_ = false; }
      if (!pending_) continue;
      request = std::move(*pending_); pending_.reset();
    }
    const auto start = std::chrono::steady_clock::now();
    Cancel cancel = [this, id = request.generation] { return stopping_ || generation_ != id; };
    Update result; result.generation = request.generation;
    try {
      if (request.reload || !book || book->source != request.source) {
        cache.clear(); book.reset();
        auto indexed = archive.index(request.source, cancel);
        if (!indexed) result.error = indexed.error();
        else book = std::make_shared<Book>(std::move(*indexed));
      }
      result.book = book;
      if (book && !cancel()) {
        result.page = std::min(request.page, book->pages.size() - 1);
        result.image = cache.get(result.page); result.cached = !!result.image;
        if (!result.image) {
          auto bytes = archive.read(*book, result.page, cancel);
          if (!bytes) result.error = bytes.error();
          else if (!cancel()) {
            auto decoded = decode_image(*bytes);
            if (!decoded) result.error = decoded.error();
            else { result.image = std::make_shared<Image>(std::move(*decoded)); cache.put(result.page, result.image); }
          }
        }
      }
      result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
      if (cancel()) continue;
      {
        std::lock_guard lock(mutex_);
        if (!cancel()) update_ = result;
      }
      // Prefetch adjacent pages only after publishing the requested page. A newer
      // request cancels archive reads; only the latest generation may reach the UI.
      if (prefetch_ && book && result.image) {
        for (auto page : {result.page + 1, result.page == 0 ? book->pages.size() : result.page - 1}) {
          if (cancel()) break;
          if (page >= book->pages.size() || cache.get(page)) continue;
          auto bytes = archive.read(*book, page, cancel);
          if (bytes && !cancel()) {
            auto image = decode_image(*bytes);
            if (image && !cancel()) cache.put(page, std::make_shared<Image>(std::move(*image)));
          }
        }
      }
    } catch (const std::exception& ex) {
      std::lock_guard lock(mutex_);
      if (!cancel()) { result.error = ex.what(); result.image.reset(); update_ = std::move(result); }
    }
  }
}
}
