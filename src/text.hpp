#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include <string_view>

namespace oneam
{
class Text
{
public:
   explicit Text(SDL_Renderer* renderer);
   ~Text();
   bool valid() const;
   void draw(std::string_view text, float x, float y, float size, SDL_Color color, float max_width = 10000);
   float width(std::string_view text, float size);

private:
   struct Impl;
   std::unique_ptr<Impl> impl_;
};
}  // namespace oneam
