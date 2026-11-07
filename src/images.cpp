#include "reader.hpp"
#include "reading.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 16384
#include <stb_image.h>
#include <limits>

namespace oneam
{
Result<Image> decode_image(std::span<const std::uint8_t> bytes)
{
   if (bytes.empty() || bytes.size() > max_encoded_bytes)
      return std::unexpected("Invalid image size.");
   int w = 0, h = 0, channels = 0;
   const auto size = static_cast<int>(bytes.size());
   if (!stbi_info_from_memory(bytes.data(), size, &w, &h, &channels))
      return std::unexpected("This page is damaged or uses an unsupported image format.");
   if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h) * 4 > max_decoded_bytes)
      return std::unexpected("Page exceeds the 96 MiB / 16384 pixel image limit.");
   std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
      stbi_load_from_memory(bytes.data(), size, &w, &h, &channels, 4), stbi_image_free
   );
   if (!decoded)
      return std::unexpected("Could not decode this page.");
   const auto count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4;
   Image image{w, h, {}};
   image.pixels.assign(decoded.get(), decoded.get() + count);
   image.content = detect_content(image);
   return image;
}
}  // namespace oneam
