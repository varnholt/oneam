#pragma once
#include <array>
#include "reader.hpp"

namespace oneam
{
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
SDL_Rect detect_content(const Image& image);
SDL_Rect page_crop(const Image& image, bool trim);
SDL_FPoint page_size(const Image& image, bool trim, int rotation);
std::size_t spread_start(std::size_t page, bool cover_alone);
std::size_t spread_next(std::size_t page, std::size_t count, bool cover_alone, int direction);
std::array<std::uint8_t, 256> tone_table(Tone tone);
bool text_matches(std::string_view text, std::string_view query);
}  // namespace oneam
