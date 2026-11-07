#include "reading.hpp"
#include <algorithm>
#include <cmath>

namespace oneam
{
SDL_Rect detect_content(const Image& im)
{
   SDL_Rect full{0, 0, im.width, im.height};
   if (im.width < 16 || im.height < 16 || im.pixels.size() != std::size_t(im.width) * im.height * 4)
      return full;
   // Uniform near-white or transparent borders only; stop at visible artwork.
   auto background = [&](int x, int y)
   {
      const auto* p = &im.pixels[(std::size_t(y) * im.width + x) * 4];
      return p[3] < 16 || (p[0] > 240 && p[1] > 240 && p[2] > 240);
   };
   auto row = [&](int y, int left, int right)
   {
      for (int x = left; x < right; ++x)
         if (!background(x, y))
            return false;
      return true;
   };
   auto column = [&](int x, int top, int bottom)
   {
      for (int y = top; y < bottom; ++y)
         if (!background(x, y))
            return false;
      return true;
   };
   int l = 0, r = im.width, t = 0, b = im.height;
   while (t < im.height / 4 && row(t, l, r))
      ++t;
   while (b > im.height * 3 / 4 && row(b - 1, l, r))
      --b;
   while (l < im.width / 4 && column(l, t, b))
      ++l;
   while (r > im.width * 3 / 4 && column(r - 1, t, b))
      --r;
   // Retain a small safety border and leave blank pages alone.
   if (l == im.width / 4 && r == im.width * 3 / 4 && t == im.height / 4 && b == im.height * 3 / 4)
      return full;
   l = std::max(0, l - 2);
   t = std::max(0, t - 2);
   r = std::min(im.width, r + 2);
   b = std::min(im.height, b + 2);
   return {l, t, r - l, b - t};
}
SDL_Rect page_crop(const Image& im, bool trim)
{
   return trim && im.content.w > 0 && im.content.h > 0 ? im.content : SDL_Rect{0, 0, im.width, im.height};
}
SDL_FPoint page_size(const Image& im, bool trim, int rotation)
{
   const auto c = page_crop(im, trim);
   return rotation % 2 ? SDL_FPoint{float(c.h), float(c.w)} : SDL_FPoint{float(c.w), float(c.h)};
}
std::size_t spread_start(std::size_t page, bool cover_alone)
{
   if (cover_alone && page == 0)
      return 0;
   const auto offset = cover_alone ? 1u : 0u;
   return offset + ((page - offset) / 2) * 2;
}
std::size_t spread_next(std::size_t page, std::size_t count, bool cover_alone, int direction)
{
   if (!count)
      return 0;
   const auto start = spread_start(std::min(page, count - 1), cover_alone);
   if (direction < 0)
      return start == 0 ? 0 : spread_start(start - 1, cover_alone);
   return std::min(count - 1, start + (cover_alone && start == 0 ? 1 : 2));
}
std::array<std::uint8_t, 256> tone_table(Tone t)
{
   t.brightness = std::clamp(t.brightness, -.5f, .5f);
   t.contrast = std::clamp(t.contrast, .5f, 2.f);
   t.gamma = std::clamp(t.gamma, .5f, 2.f);
   std::array<std::uint8_t, 256> table;
   for (int i = 0; i < 256; ++i)
   {
      const float x = std::clamp((i / 255.f - .5f) * t.contrast + .5f + t.brightness, 0.f, 1.f);
      table[i] = std::uint8_t(std::round(std::pow(x, 1.f / t.gamma) * 255));
   }
   return table;
}
bool text_matches(std::string_view text, std::string_view query)
{
   const std::string haystack(text), needle(query);
   return SDL_strcasestr(haystack.c_str(), needle.c_str()) != nullptr;
}
}  // namespace oneam
