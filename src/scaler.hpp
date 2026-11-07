#pragma once
#include "reader.hpp"
#include <thread>

namespace oneam {
Result<Image> resize_image(const Image &source, int width, int height);

// Only the newest request is published. Filtering never blocks the UI thread.
class PageScaler {
public:
  struct Update {
    std::shared_ptr<const Image> source;
    Image image;
  };
  PageScaler();
  ~PageScaler();
  void request(std::shared_ptr<const Image> source, int width, int height);
  std::optional<Update> poll();
  void clear();

private:
  struct Job {
    std::shared_ptr<const Image> source;
    int width, height;
    std::uint64_t generation;
  };
  std::mutex mutex_;
  std::condition_variable condition_;
  std::optional<Job> pending_;
  std::optional<Update> update_;
  std::uint64_t generation_ = 0;
  bool stopping_ = false;
  std::thread worker_;
  void run();
};
} // namespace oneam
