#include "icons.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace oneam::ui {
namespace {
SDL_FColor color(SDL_Color c, float alpha = 1) {
  return {c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f * alpha};
}
void disk(SDL_Renderer *renderer, float x, float y, float radius, SDL_Color tint) {
  constexpr int segments = 24;
  std::vector<SDL_Vertex> vertices;
  std::vector<int> indices;
  vertices.push_back({{x, y}, color(tint), {0, 0}});
  for (int i = 0; i <= segments; ++i) {
    const float angle = static_cast<float>(i) * 6.283185307f / segments;
    vertices.push_back({{x + std::cos(angle) * radius, y + std::sin(angle) * radius}, color(tint), {0, 0}});
    vertices.push_back({{x + std::cos(angle) * (radius + .6f), y + std::sin(angle) * (radius + .6f)},
                        color(tint, 0),
                        {0, 0}});
    if (i) {
      int a = 1 + (i - 1) * 2, b = 1 + i * 2;
      indices.insert(indices.end(), {0, a, b, a, a + 1, b + 1, a, b + 1, b});
    }
  }
  SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()), indices.data(),
                     static_cast<int>(indices.size()));
}
void segment(SDL_Renderer *renderer, SDL_FPoint a, SDL_FPoint b, float stroke, SDL_Color tint) {
  const float length = std::hypot(b.x - a.x, b.y - a.y);
  if (length < .001f)
    return;
  const float nx = -(b.y - a.y) / length, ny = (b.x - a.x) / length, r = stroke * .5f;
  std::array<SDL_Vertex, 8> v;
  const std::array<float, 4> offsets{-r - .6f, -r, r, r + .6f};
  for (int i = 0; i < 4; ++i) {
    const auto c = color(tint, (i == 0 || i == 3) ? 0.f : 1.f);
    v[i * 2] = {{a.x + nx * offsets[i], a.y + ny * offsets[i]}, c, {0, 0}};
    v[i * 2 + 1] = {{b.x + nx * offsets[i], b.y + ny * offsets[i]}, c, {0, 0}};
  }
  constexpr int indices[]{0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4, 4, 5, 6, 5, 7, 6};
  SDL_RenderGeometry(renderer, nullptr, v.data(), 8, indices, 18);
  disk(renderer, a.x, a.y, r, tint);
  disk(renderer, b.x, b.y, r, tint);
}
} // namespace
void rounded_rect(SDL_Renderer *renderer, SDL_FRect r, float radius, SDL_Color c) {
  radius = std::min({radius, r.w * .5f, r.h * .5f});
  SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
  SDL_FRect h{r.x + radius, r.y, r.w - 2 * radius, r.h}, v{r.x, r.y + radius, r.w, r.h - 2 * radius};
  SDL_RenderFillRect(renderer, &h);
  SDL_RenderFillRect(renderer, &v);
  // Filled corner fans keep the control geometry independent of image assets.
  for (int corner = 0; corner < 4; ++corner) {
    const float x = r.x + (corner == 0 || corner == 3 ? radius : r.w - radius);
    const float y = r.y + (corner < 2 ? radius : r.h - radius);
    const float start = 3.141592654f + (corner * 1.570796327f);
    std::array<SDL_Vertex, 10> points;
    std::array<int, 24> indices;
    points[0] = {{x, y}, color(c), {0, 0}};
    for (int i = 0; i <= 8; ++i) {
      const float angle = start + i * 1.570796327f / 8;
      points[i + 1] = {{x + std::cos(angle) * radius, y + std::sin(angle) * radius}, color(c), {0, 0}};
      if (i) {
        indices[(i - 1) * 3] = 0;
        indices[(i - 1) * 3 + 1] = i;
        indices[(i - 1) * 3 + 2] = i + 1;
      }
    }
    SDL_RenderGeometry(renderer, nullptr, points.data(), 10, indices.data(), 24);
  }
}
void draw_icon(SDL_Renderer *renderer, Icon icon, SDL_FRect r, SDL_Color c) {
  const float scale = std::min(r.w, r.h) / 24;
  auto point = [&](float x, float y) { return SDL_FPoint{r.x + x * scale, r.y + y * scale}; };
  auto path = [&](std::initializer_list<SDL_FPoint> points) {
    auto a = points.begin();
    for (auto b = a + 1; b != points.end(); ++a, ++b)
      segment(renderer, point(a->x, a->y), point(b->x, b->y), 1.8f * scale, c);
  };
  auto circle = [&](float x, float y, float radius) {
    constexpr int n = 48;
    for (int i = 0; i < n; ++i) {
      float a = i * 6.283185307f / n, b = (i + 1) * 6.283185307f / n;
      segment(renderer, point(x + std::cos(a) * radius, y + std::sin(a) * radius),
              point(x + std::cos(b) * radius, y + std::sin(b) * radius), 1.8f * scale, c);
    }
  };
  switch (icon) {
  case Icon::Book:
    path({{3, 5}, {8, 4}, {12, 6}, {16, 4}, {21, 5}, {21, 20}, {16, 19}, {12, 21}, {8, 19}, {3, 20}, {3, 5}});
    path({{12, 6}, {12, 21}});
    break;
  case Icon::Grid:
    for (float x : {4.f, 14.f})
      for (float y : {4.f, 14.f})
        path({{x, y}, {x + 6, y}, {x + 6, y + 6}, {x, y + 6}, {x, y}});
    break;
  case Icon::Folder:
  case Icon::Open:
    path({{3, 8}, {3, 5}, {9, 5}, {11, 8}, {20, 8}, {20, 11}});
    path({{3, 10}, {21, 10}, {18, 20}, {3, 20}, {3, 10}});
    if (icon == Icon::Open) {
      path({{11, 14}, {15, 14}, {15, 18}});
      path({{10, 19}, {15, 14}});
    }
    break;
  case Icon::Left:
  case Icon::First:
    path({{15, 5}, {8, 12}, {15, 19}});
    if (icon == Icon::First)
      path({{4, 5}, {4, 19}});
    break;
  case Icon::Right:
  case Icon::Last:
    path({{9, 5}, {16, 12}, {9, 19}});
    if (icon == Icon::Last)
      path({{20, 5}, {20, 19}});
    break;
  case Icon::ZoomIn:
  case Icon::ZoomOut:
    circle(10, 10, 6.5f);
    path({{15, 15}, {21, 21}});
    path({{7, 10}, {13, 10}});
    if (icon == Icon::ZoomIn)
      path({{10, 7}, {10, 13}});
    break;
  case Icon::Reset:
    for (int i = 0; i < 48; ++i) {
      const float a = (180.f - i * 315.f / 48) * .0174532925f,
                  b = (180.f - (i + 1) * 315.f / 48) * .0174532925f;
      segment(renderer, point(12 + std::cos(a) * 8.5f, 12 + std::sin(a) * 8.5f),
              point(12 + std::cos(b) * 8.5f, 12 + std::sin(b) * 8.5f), 1.8f * scale, c);
    }
    path({{6, 6}, {3, 8}, {3, 3}});
    path({{3, 8}, {8, 8}});
    break;
  case Icon::FitPage:
    path({{7, 3}, {17, 3}, {17, 21}, {7, 21}, {7, 3}});
    path({{3, 8}, {3, 3}, {5, 3}});
    path({{21, 16}, {21, 21}, {19, 21}});
    break;
  case Icon::FitWidth:
    path({{3, 5}, {3, 19}});
    path({{21, 5}, {21, 19}});
    path({{4, 12}, {20, 12}});
    path({{7, 9}, {4, 12}, {7, 15}});
    path({{17, 9}, {20, 12}, {17, 15}});
    break;
  case Icon::Expand:
    path({{8, 3}, {3, 3}, {3, 8}});
    path({{16, 3}, {21, 3}, {21, 8}});
    path({{3, 16}, {3, 21}, {8, 21}});
    path({{21, 16}, {21, 21}, {16, 21}});
    break;
  case Icon::Collapse:
    path({{3, 8}, {8, 8}, {8, 3}});
    path({{16, 3}, {16, 8}, {21, 8}});
    path({{3, 16}, {8, 16}, {8, 21}});
    path({{16, 21}, {16, 16}, {21, 16}});
    break;
  case Icon::Help:
    circle(12, 12, 9);
    path({{9, 9}, {10, 7}, {13, 7}, {15, 9}, {14, 11}, {12, 12}, {12, 14}});
    disk(renderer, point(12, 17).x, point(12, 17).y, .95f * scale, c);
    break;
  case Icon::Close:
    path({{6, 6}, {18, 18}});
    path({{18, 6}, {6, 18}});
    break;
  case Icon::Menu:
    path({{4, 6}, {20, 6}});
    path({{4, 12}, {20, 12}});
    path({{4, 18}, {20, 18}});
    break;
  case Icon::Up:
    path({{5, 15}, {12, 8}, {19, 15}});
    break;
  case Icon::Down:
    path({{5, 9}, {12, 16}, {19, 9}});
    break;
  }
}
} // namespace oneam::ui
