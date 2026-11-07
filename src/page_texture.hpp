#pragma once
#include "scaler.hpp"

namespace oneam
{
// Owns GPU state on the UI thread; resizing and color processing run off-thread.
class PageTexture
{
public:
   ~PageTexture();
   void clear();
   bool prepare(SDL_Renderer* renderer, std::shared_ptr<const Image> image, bool trim, Tone tone, float scale);
   void draw(SDL_Renderer* renderer, SDL_FRect destination, int rotation);

private:
   std::shared_ptr<const Image> image_;
   SDL_Texture *source_ = nullptr, *filtered_ = nullptr;
   PageScaler scaler_;
   SDL_Rect crop_{};
   Tone tone_{};
   int width_ = 0, height_ = 0;
   bool requested_ = false;
   std::uint64_t due_ = 0;
};
}  // namespace oneam
