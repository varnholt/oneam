#pragma once
#include <SDL3/SDL.h>
#include <optional>
#include <string_view>
#include <vector>

namespace oneam::ui {
enum class Action {
  None,
  Menu,
  Close,
  Open,
  Folder,
  Library,
  Previous,
  Next,
  First,
  Last,
  ZoomOut,
  ZoomIn,
  ResetZoom,
  FitPage,
  FitWidth,
  ToggleFit,
  Fullscreen,
  Help,
  ScrollUp,
  ScrollDown,
  Back
};
enum class Icon {
  Book,
  Grid,
  Open,
  Folder,
  Left,
  Right,
  First,
  Last,
  ZoomOut,
  ZoomIn,
  Reset,
  FitPage,
  FitWidth,
  Expand,
  Collapse,
  Help,
  Close,
  Menu,
  Up,
  Down
};
struct Context {
  bool reading = false, book = false, image = false, folders = true;
  bool previous = false, next = false, fullscreen = false, fit_width = false;
};
struct Control {
  Action action = Action::None;
  SDL_FRect rect{};
  std::string_view label;
  Icon icon = Icon::Book;
  bool enabled = true, active = false;
};
struct MenuLayout {
  SDL_FRect panel{}, viewport{};
  std::vector<Control> fixed, items;
  float max_scroll = 0, scroll = 0, content_height = 0;
};
bool contains(const SDL_FRect &rect, float x, float y);
bool enabled(Action action, const Context &context);
Icon icon(Action action, const Context &context);
std::string_view label(Action action, const Context &context);
std::string_view shortcut(Action action);
std::optional<Action> keyboard(SDL_Keycode key, SDL_Keymod modifiers, bool reading);
std::vector<Control> toolbar(SDL_FRect safe, const Context &context);
std::vector<Control> navigation(SDL_FRect safe, const Context &context);
MenuLayout menu(SDL_FRect safe, const Context &context, float scroll);
std::optional<Action> hit(const std::vector<Control> &controls, float x, float y);
std::optional<Action> hit(const MenuLayout &menu, float x, float y);
} // namespace oneam::ui
