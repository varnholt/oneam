#include "text.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace oneam
{
namespace
{
int next(std::string_view text, std::size_t& i)
{
   const auto c = static_cast<unsigned char>(text[i++]);
   if (c < 128)
      return c;
   const int count = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc2 ? 1 : 0;
   if (!count || i + count > text.size())
      return 0xfffd;
   int value = c & ((1 << (6 - count)) - 1);
   for (int j = 0; j < count; ++j)
   {
      const auto byte = static_cast<unsigned char>(text[i]);
      if ((byte & 0xc0) != 0x80)
         return 0xfffd;
      ++i;
      value = (value << 6) | (byte & 0x3f);
   }
   if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
      return 0xfffd;
   return value;
}
}  // namespace
struct Text::Impl
{
   SDL_Renderer* renderer;
   std::vector<unsigned char> bytes;
   stbtt_fontinfo font{};
   struct Glyph
   {
      SDL_Texture* texture = nullptr;
      int w = 0, h = 0, x = 0, y = 0;
      float advance = 0;
   };
   std::unordered_map<std::uint64_t, Glyph> glyphs;
   bool loaded = false;
   explicit Impl(SDL_Renderer* r) : renderer(r)
   {
      std::string filename = "NotoSans.ttf";
#ifndef SDL_PLATFORM_ANDROID
      filename = std::string(SDL_GetBasePath()) + "assets/" + filename;
#endif
      size_t length = 0;
      auto* data = static_cast<unsigned char*>(SDL_LoadFile(filename.c_str(), &length));
      if (data)
      {
         bytes.assign(data, data + length);
         SDL_free(data);
      }
      if (!bytes.empty())
         loaded = stbtt_InitFont(&font, bytes.data(), 0) != 0;
   }
   ~Impl()
   {
      for (auto& [id, glyph] : glyphs)
         SDL_DestroyTexture(glyph.texture);
   }
   Glyph& glyph(int codepoint, int size)
   {
      const auto key = (static_cast<std::uint64_t>(size) << 32) | static_cast<unsigned>(codepoint);
      if (auto found = glyphs.find(key); found != glyphs.end())
         return found->second;
      // Bound glyph textures when browsing directories with many distinct names.
      if (glyphs.size() >= 2048)
      {
         for (auto& [id, g] : glyphs)
            SDL_DestroyTexture(g.texture);
         glyphs.clear();
      }
      Glyph result;
      const float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(size));
      int advance = 0;
      stbtt_GetCodepointHMetrics(&font, codepoint, &advance, nullptr);
      result.advance = advance * scale;
      unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, codepoint, &result.w, &result.h, &result.x, &result.y);
      if (bitmap && result.w > 0 && result.h > 0)
      {
         std::vector<unsigned char> rgba(static_cast<std::size_t>(result.w) * result.h * 4, 255);
         for (std::size_t p = 0; p < rgba.size() / 4; ++p)
            rgba[p * 4 + 3] = bitmap[p];
         result.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, result.w, result.h);
         if (result.texture)
         {
            SDL_UpdateTexture(result.texture, nullptr, rgba.data(), result.w * 4);
            SDL_SetTextureBlendMode(result.texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(result.texture, SDL_SCALEMODE_LINEAR);
         }
      }
      stbtt_FreeBitmap(bitmap, nullptr);
      return glyphs.emplace(key, result).first->second;
   }
};
Text::Text(SDL_Renderer* renderer) : impl_(std::make_unique<Impl>(renderer))
{
}
Text::~Text() = default;
bool Text::valid() const
{
   return impl_->loaded;
}
void Text::draw(std::string_view text, float x, float y, float size, SDL_Color color, float max_width)
{
   if (!valid())
      return;
   float scale_x = 1, scale_y = 1;
   SDL_GetRenderScale(impl_->renderer, &scale_x, &scale_y);
   const float density = std::max(1.f, scale_y);
   const int raster_size = static_cast<int>(size * density + .5f);
   const bool clipped = width(text, size) > max_width;
   const float dots = clipped ? width("...", size) : 0;
   const float start = x;
   for (std::size_t i = 0; i < text.size();)
   {
      const int cp = next(text, i);
      auto& glyph = impl_->glyph(cp, raster_size);
      if (x + glyph.advance / density > start + max_width - dots)
         break;
      if (glyph.texture)
      {
         SDL_SetTextureColorMod(glyph.texture, color.r, color.g, color.b);
         SDL_SetTextureAlphaMod(glyph.texture, color.a);
         SDL_FRect dst{x + glyph.x / density, y + size * .8f + glyph.y / density, glyph.w / density, glyph.h / density};
         SDL_RenderTexture(impl_->renderer, glyph.texture, nullptr, &dst);
      }
      x += glyph.advance / density;
   }
   if (clipped && dots <= max_width)
      draw("...", x, y, size, color, dots + 1);
}
float Text::width(std::string_view text, float size)
{
   if (!valid())
      return 0;
   float scale_x = 1, scale_y = 1;
   SDL_GetRenderScale(impl_->renderer, &scale_x, &scale_y);
   const float density = std::max(1.f, scale_y);
   float result = 0;
   for (std::size_t i = 0; i < text.size();)
      result += impl_->glyph(next(text, i), static_cast<int>(size * density + .5f)).advance / density;
   return result;
}
}  // namespace oneam
