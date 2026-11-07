#include "controls.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace oneam::ui {
bool contains(const SDL_FRect &r, float x, float y) {
  return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
bool enabled(Action a, const Context &c) {
  switch (a) {
  case Action::Previous:
    return c.reading && c.book && c.previous;
  case Action::Next:
    return c.reading && c.book && c.next;
  case Action::ZoomOut:
  case Action::ZoomIn:
  case Action::ResetZoom:
  case Action::FitPage:
  case Action::FitWidth:
  case Action::ToggleFit:
    return c.reading && c.image;
  case Action::ReadingMode:
    return c.reading;
  case Action::First:
  case Action::Last:
    return c.book;
  case Action::Folder:
    return c.folders;
  default:
    return true;
  }
}
Icon icon(Action a, const Context &c) {
  switch (a) {
  case Action::Library:
    return Icon::Grid;
  case Action::Open:
    return Icon::Open;
  case Action::Folder:
    return Icon::Folder;
  case Action::Previous:
    return Icon::Left;
  case Action::Next:
    return Icon::Right;
  case Action::First:
    return Icon::First;
  case Action::Last:
    return Icon::Last;
  case Action::ZoomOut:
    return Icon::ZoomOut;
  case Action::ZoomIn:
    return Icon::ZoomIn;
  case Action::ResetZoom:
    return Icon::Reset;
  case Action::FitPage:
    return Icon::FitPage;
  case Action::FitWidth:
  case Action::ToggleFit:
    return Icon::FitWidth;
  case Action::Fullscreen:
    return c.fullscreen ? Icon::Collapse : Icon::Expand;
  case Action::ReadingMode:
    return c.controls_hidden ? Icon::Menu : Icon::Book;
  case Action::Help:
    return Icon::Help;
  case Action::Close:
  case Action::Back:
    return Icon::Close;
  case Action::ScrollUp:
    return Icon::Up;
  case Action::ScrollDown:
    return Icon::Down;
  case Action::Menu:
    return Icon::Menu;
  default:
    return Icon::Book;
  }
}
std::string_view label(Action a, const Context &c) {
  switch (a) {
  case Action::Library:
    return "Library";
  case Action::Open:
    return "Open book";
  case Action::Folder:
    return "Open folder";
  case Action::Previous:
    return "Previous page";
  case Action::Next:
    return "Next page";
  case Action::First:
    return c.reading ? "First page" : "First book";
  case Action::Last:
    return c.reading ? "Last page" : "Last book";
  case Action::ZoomOut:
    return "Zoom out";
  case Action::ZoomIn:
    return "Zoom in";
  case Action::ResetZoom:
    return "Reset zoom";
  case Action::FitPage:
    return "Fit page";
  case Action::FitWidth:
    return "Fit width";
  case Action::Fullscreen:
    return c.fullscreen ? "Exit fullscreen" : "Fullscreen";
  case Action::ReadingMode:
    return c.controls_hidden ? "Show controls" : "Hide controls";
  case Action::Help:
    return "Help";
  case Action::Close:
    return "Close";
  case Action::ScrollUp:
    return "Scroll up";
  case Action::ScrollDown:
    return "Scroll down";
  case Action::Menu:
    return "Menu";
  default:
    return {};
  }
}
std::string_view shortcut(Action a) {
  switch (a) {
  case Action::ReadingMode:
    return "Tab";
  case Action::Open:
    return "Ctrl+O";
  case Action::First:
    return "Home";
  case Action::Last:
    return "End";
  case Action::ResetZoom:
    return "0";
  case Action::Fullscreen:
    return "F11";
  case Action::Help:
    return "F1";
  case Action::Library:
    return "Esc";
  default:
    return {};
  }
}
std::optional<Action> keyboard(SDL_Keycode key, SDL_Keymod mods, bool reading) {
  if ((mods & SDL_KMOD_CTRL) && key == SDLK_O)
    return Action::Open;
  switch (key) {
  case SDLK_ESCAPE:
  case SDLK_AC_BACK:
    return Action::Back;
  case SDLK_F1:
    return Action::Help;
  case SDLK_F11:
    return Action::Fullscreen;
  case SDLK_HOME:
    return Action::First;
  case SDLK_END:
    return Action::Last;
  default:
    break;
  }
  if (!reading)
    return {};
  switch (key) {
  case SDLK_TAB:
    return Action::ReadingMode;
  case SDLK_RIGHT:
  case SDLK_PAGEDOWN:
  case SDLK_SPACE:
    return Action::Next;
  case SDLK_LEFT:
  case SDLK_PAGEUP:
    return Action::Previous;
  case SDLK_UP:
    return Action::ScrollUp;
  case SDLK_DOWN:
    return Action::ScrollDown;
  case SDLK_EQUALS:
  case SDLK_PLUS:
  case SDLK_KP_PLUS:
    return Action::ZoomIn;
  case SDLK_MINUS:
  case SDLK_KP_MINUS:
    return Action::ZoomOut;
  case SDLK_0:
    return Action::ResetZoom;
  case SDLK_W:
    return Action::ToggleFit;
  default:
    return {};
  }
}
namespace {
Control control(Action a, SDL_FRect r, const Context &c) {
  return {a,
          r,
          label(a, c),
          icon(a, c),
          enabled(a, c),
          (a == Action::FitWidth && c.fit_width) || (a == Action::FitPage && !c.fit_width)};
}
} // namespace
std::vector<Control> toolbar(SDL_FRect safe, const Context &c) {
  std::vector<Control> result;
  float right = safe.x + safe.w - 12;
  if (c.reading && c.controls_hidden) {
    auto restore = control(Action::ReadingMode, {right - 136, safe.y + 12, 136, 48}, c);
    restore.label = "Controls";
    return {restore};
  }
  const float menu_width = safe.w >= 300 ? 104.f : 48.f;
  auto menu = control(Action::Menu, {right - menu_width, safe.y + 12, menu_width, 48}, c);
  if (menu_width == 48)
    menu.label = {};
  result.push_back(menu);
  right -= menu_width + 8;
  const float open_width = safe.w >= 360 ? 104.f : 48.f;
  auto open = control(Action::Open, {right - open_width, safe.y + 12, open_width, 48}, c);
  open.label = open_width == 48 ? "" : "Open";
  result.push_back(open);
  right -= open_width + 8;
  if (c.folders && safe.w >= 650) {
    auto folder = control(Action::Folder, {right - 104, safe.y + 12, 104, 48}, c);
    folder.label = "Folder";
    result.push_back(folder);
  }
  if (c.reading) {
    const float w = safe.w >= 320 ? 112.f : 48.f;
    auto library = control(Action::Library, {safe.x + 12, safe.y + 12, w, 48}, c);
    if (w == 48)
      library.label = {};
    result.push_back(library);
    if (safe.w >= 540) {
      auto read = control(Action::ReadingMode, {safe.x + 132, safe.y + 12, 136, 48}, c);
      read.label = "Read";
      result.push_back(read);
    }
  }
  return result;
}
std::vector<Control> navigation(SDL_FRect safe, const Context &c) {
  if (!c.reading || c.controls_hidden)
    return {};
  const float w = safe.w >= 460 ? 128.f : 56.f;
  auto previous = control(Action::Previous, {safe.x + 12, safe.y + safe.h - 60, w, 48}, c);
  auto next = control(Action::Next, {safe.x + safe.w - w - 12, safe.y + safe.h - 60, w, 48}, c);
  previous.label = safe.w >= 460 ? "Previous" : "";
  next.label = safe.w >= 460 ? "Next" : "";
  return {previous, next};
}
MenuLayout menu(SDL_FRect safe, const Context &c, float scroll) {
  MenuLayout result;
  std::vector<Action> actions;
  if (c.reading)
    actions = {Action::ReadingMode, Action::Previous, Action::Next,      Action::First,   Action::Last,
               Action::ZoomOut,     Action::ZoomIn,   Action::ResetZoom, Action::FitPage, Action::FitWidth};
  else
    actions = {Action::First, Action::Last};
  actions.insert(actions.end(), {Action::ScrollUp, Action::ScrollDown, Action::Library, Action::Open});
  if (c.folders)
    actions.push_back(Action::Folder);
  actions.push_back(Action::Fullscreen);
  const int columns = safe.w >= 640 ? 2 : 1;
  const float width = std::min(safe.w - 24, columns == 2 ? 560.f : 340.f);
  const float content = static_cast<float>((actions.size() + columns - 1) / columns) * 52;
  const float height = std::min(content + 84, safe.h - 92);
  result.panel = {safe.x + safe.w - width - 12, safe.y + 80, width, height};
  result.viewport = {result.panel.x + 12, result.panel.y + 68, width - 24, std::max(1.f, height - 80)};
  result.content_height = content;
  result.max_scroll = std::max(0.f, content - result.viewport.h);
  result.scroll = std::clamp(scroll, 0.f, result.max_scroll);
  result.fixed = {control(Action::Close, {result.panel.x + width - 60, result.panel.y + 10, 48, 48}, c),
                  control(Action::Help, {result.panel.x + width - 116, result.panel.y + 10, 48, 48}, c)};
  for (auto &fixed : result.fixed)
    fixed.label = {};
  const float cell = (result.viewport.w - 8 * (columns - 1)) / columns;
  for (std::size_t i = 0; i < actions.size(); ++i)
    result.items.push_back(
        control(actions[i],
                {result.viewport.x + static_cast<float>(i % columns) * (cell + 8),
                 result.viewport.y + static_cast<float>(i / columns) * 52 - result.scroll, cell, 48},
                c));
  return result;
}
std::optional<Action> hit(const std::vector<Control> &controls, float x, float y) {
  for (const auto &control : controls)
    if (control.enabled && contains(control.rect, x, y))
      return control.action;
  return {};
}
std::optional<Action> hit(const MenuLayout &menu, float x, float y) {
  if (auto fixed = hit(menu.fixed, x, y))
    return fixed;
  if (contains(menu.viewport, x, y))
    return hit(menu.items, x, y);
  return {};
}
} // namespace oneam::ui
