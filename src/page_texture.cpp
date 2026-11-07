#include "page_texture.hpp"
#include <algorithm>
#include <cmath>

namespace oneam
{
PageTexture::~PageTexture()
{
   clear();
}
void PageTexture::clear()
{
   scaler_.clear();
   SDL_DestroyTexture(source_);
   source_ = nullptr;
   SDL_DestroyTexture(filtered_);
   filtered_ = nullptr;
   image_.reset();
   width_ = height_ = 0;
}
bool PageTexture::prepare(SDL_Renderer* r, std::shared_ptr<const Image> im, bool trim, Tone tone, float scale)
{
   bool changed = false;
   if (image_ != im)
   {
      clear();
      image_ = std::move(im);
      if (!image_)
         return true;
      source_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image_->width, image_->height);
      if (source_)
      {
         if (!SDL_UpdateTexture(source_, nullptr, image_->pixels.data(), image_->width * 4))
         {
            SDL_DestroyTexture(source_);
            source_ = nullptr;
         }
         else
         {
            SDL_SetTextureScaleMode(source_, SDL_SCALEMODE_LINEAR);
            SDL_SetTextureBlendMode(source_, SDL_BLENDMODE_BLEND);
         }
      }
      changed = true;
   }
   if (!image_)
      return changed;
   const auto crop = page_crop(*image_, trim);
   const auto factor = std::clamp(scale, .001f, 1.f);
   const int w = std::max(1, int(std::round(crop.w * factor))), h = std::max(1, int(std::round(crop.h * factor)));
   const bool crop_changed = crop.x != crop_.x || crop.y != crop_.y || crop.w != crop_.w || crop.h != crop_.h;
   if (crop_changed || tone != tone_ || w != width_ || h != height_)
   {
      scaler_.clear();
      requested_ = false;
      if (crop_changed || tone != tone_ || factor == 1.f)
      {
         SDL_DestroyTexture(filtered_);
         filtered_ = nullptr;
      }
      crop_ = crop;
      tone_ = tone;
      width_ = w;
      height_ = h;
      due_ = SDL_GetTicks() + 60;
   }
   if (!requested_ && (factor < 1.f || tone != Tone{}) && SDL_GetTicks() >= due_)
   {
      scaler_.request(image_, w, h, crop, tone);
      requested_ = true;
   }
   if (auto update = scaler_.poll(); update && update->source == image_ && update->image.width == w && update->image.height == h)
   {
      auto next = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
      if (next && SDL_UpdateTexture(next, nullptr, update->image.pixels.data(), w * 4))
      {
         SDL_SetTextureScaleMode(next, SDL_SCALEMODE_LINEAR);
         SDL_SetTextureBlendMode(next, SDL_BLENDMODE_BLEND);
         SDL_DestroyTexture(filtered_);
         filtered_ = next;
         changed = true;
      }
      else
         SDL_DestroyTexture(next);
   }
   return changed;
}
void PageTexture::draw(SDL_Renderer* r, SDL_FRect dest, int rotation)
{
   if (!source_)
      return;
   SDL_FRect src{float(crop_.x), float(crop_.y), float(crop_.w), float(crop_.h)};
   if (rotation % 2)
   {
      const auto center = SDL_FPoint{dest.x + dest.w * .5f, dest.y + dest.h * .5f};
      std::swap(dest.w, dest.h);
      dest.x = center.x - dest.w * .5f;
      dest.y = center.y - dest.h * .5f;
   }
   SDL_RenderTextureRotated(r, filtered_ ? filtered_ : source_, filtered_ ? nullptr : &src, &dest, rotation * 90., nullptr, SDL_FLIP_NONE);
}
}  // namespace oneam
