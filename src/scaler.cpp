#include "scaler.hpp"
#include <utility>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

namespace oneam {
Result<Image> resize_image(const Image &source, int width, int height) {
  auto valid = [](int w, int h) {
    return w > 0 && h > 0 && w <= 16384 && h <= 16384 &&
           static_cast<std::uint64_t>(w) * h * 4 <= max_decoded_bytes;
  };
  if (!valid(width, height) || !valid(source.width, source.height) ||
      source.pixels.size() != static_cast<std::size_t>(source.width) * source.height * 4)
    return std::unexpected("Invalid image dimensions for filtering.");
  Image output{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
  // Mitchell downsampling integrates the full source footprint, in linear light.
  if (!stbir_resize_uint8_srgb(source.pixels.data(), source.width, source.height, 0, output.pixels.data(),
                               width, height, 0, STBIR_RGBA))
    return std::unexpected("Could not filter this page.");
  return output;
}
PageScaler::PageScaler() : worker_([this] { run(); }) {
}
PageScaler::~PageScaler() {
  {
    std::lock_guard lock(mutex_);
    stopping_ = true;
  }
  condition_.notify_one();
  worker_.join();
}
void PageScaler::request(std::shared_ptr<const Image> source, int width, int height) {
  if (!source) {
    clear();
    return;
  }
  std::lock_guard lock(mutex_);
  pending_ = Job{std::move(source), width, height, ++generation_};
  update_.reset();
  condition_.notify_one();
}
void PageScaler::clear() {
  std::lock_guard lock(mutex_);
  ++generation_;
  pending_.reset();
  update_.reset();
}
std::optional<PageScaler::Update> PageScaler::poll() {
  std::lock_guard lock(mutex_);
  return std::exchange(update_, {});
}
void PageScaler::run() {
  for (;;) {
    std::optional<Job> job;
    {
      std::unique_lock lock(mutex_);
      condition_.wait(lock, [this] { return stopping_ || pending_; });
      if (stopping_)
        return;
      job = std::exchange(pending_, {});
    }
    Result<Image> output = std::unexpected("Page filtering failed.");
    try {
      output = resize_image(*job->source, job->width, job->height);
    } catch (const std::exception &) {
      // Keep the source texture usable if temporary filtering allocation fails.
    }
    std::lock_guard lock(mutex_);
    if (!stopping_ && job->generation == generation_ && output)
      update_ = Update{std::move(job->source), std::move(*output)};
  }
}
} // namespace oneam
