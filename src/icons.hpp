#pragma once
#include "controls.hpp"

namespace oneam::ui
{
void rounded_rect(SDL_Renderer* renderer, SDL_FRect rect, float radius, SDL_Color color);
void draw_icon(SDL_Renderer* renderer, Icon icon, SDL_FRect rect, SDL_Color color);
}  // namespace oneam::ui
