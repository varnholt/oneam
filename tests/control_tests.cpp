#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include "controls.hpp"

using namespace oneam::ui;
namespace
{
void require(bool condition, const char* message)
{
   if (!condition)
      throw std::runtime_error(message);
}
void layout_test(SDL_FRect safe, Context context)
{
   const auto top = toolbar(safe, context);
   const auto bottom = navigation(safe, context);
   for (const auto& controls : {top, bottom})
      for (const auto& item : controls)
      {
         require(item.rect.w >= 48 && item.rect.h >= 48, "touch targets at least 48 logical pixels");
         require(item.rect.x >= safe.x && item.rect.x + item.rect.w <= safe.x + safe.w, "toolbar stays in safe area");
         require(item.rect.y >= safe.y && item.rect.y + item.rect.h <= safe.y + safe.h, "control stays in safe area vertically");
         const auto actual = hit(controls, item.rect.x + item.rect.w * .5f, item.rect.y + item.rect.h * .5f);
         require(item.enabled ? actual == item.action : !actual, "rendered toolbar geometry is hit-tested consistently");
         for (const auto& other : controls)
            if (&other != &item)
            {
               const bool overlap = item.rect.x < other.rect.x + other.rect.w && item.rect.x + item.rect.w > other.rect.x &&
                                    item.rect.y < other.rect.y + other.rect.h && item.rect.y + item.rect.h > other.rect.y;
               require(!overlap, "toolbar controls never overlap");
            }
      }
   auto layout = menu(safe, context, 0);
   require(layout.panel.x >= safe.x && layout.panel.x + layout.panel.w <= safe.x + safe.w, "menu fits horizontally");
   require(layout.panel.y >= safe.y && layout.panel.y + layout.panel.h <= safe.y + safe.h, "menu fits vertically");
   for (const auto& item : layout.items)
   {
      require(item.rect.h >= 48 && item.rect.w >= 48, "menu touch target");
      // Each action must be fully reachable after scrolling, including landscape.
      const float scroll = std::clamp(item.rect.y - layout.viewport.y, 0.f, layout.max_scroll);
      auto scrolled = menu(safe, context, scroll);
      const auto found =
         std::find_if(scrolled.items.begin(), scrolled.items.end(), [&](const auto& value) { return value.action == item.action; });
      const float x = found->rect.x + found->rect.w * .5f, y = found->rect.y + found->rect.h * .5f;
      require(contains(scrolled.viewport, x, y), "every menu item can scroll into view");
      const auto action = hit(scrolled, x, y);
      require(found->enabled ? action == found->action : !action, "every menu action uses its visible hit box");
   }
   for (auto fixed : layout.fixed)
   {
      require(hit(layout, fixed.rect.x + 24, fixed.rect.y + 24) == fixed.action, "help and close always reachable");
   }
   if (layout.max_scroll > 0)
   {
      const auto& item = layout.items.back();
      require(!hit(layout, item.rect.x + item.rect.w * .5f, item.rect.y + item.rect.h * .5f), "clipped menu entries cannot activate");
   }
}
void parity_test(Context context)
{
   auto layout = menu({0, 0, 360, 640}, context, 0);
   auto reachable = [&](Action action)
   {
      for (auto item : layout.items)
         if (item.action == action)
            return true;
      for (auto item : layout.fixed)
         if (item.action == action)
            return true;
      return false;
   };
   const std::array keys{SDLK_RIGHT, SDLK_LEFT,   SDLK_PAGEDOWN, SDLK_PAGEUP,  SDLK_SPACE, SDLK_HOME,     SDLK_END, SDLK_UP,
                         SDLK_DOWN,  SDLK_EQUALS, SDLK_PLUS,     SDLK_KP_PLUS, SDLK_MINUS, SDLK_KP_MINUS, SDLK_0,   SDLK_W,
                         SDLK_F1,    SDLK_F11,    SDLK_ESCAPE,   SDLK_TAB,     SDLK_G,     SDLK_B,        SDLK_R};
   for (auto key : keys)
      if (auto action = keyboard(key, SDL_KMOD_NONE, context.reading))
      {
         if (*action == Action::Back)
            require(reachable(Action::Close) && reachable(Action::Library), "back has pointer equivalents");
         else if (*action == Action::ToggleFit)
            require(reachable(Action::FitPage) && reachable(Action::FitWidth), "both fit states reachable");
         else
            require(reachable(*action), "every keyboard command has a menu equivalent");
      }
   require(reachable(*keyboard(SDLK_O, SDL_KMOD_CTRL, context.reading)), "open shortcut reachable");
}
}  // namespace
int main()
{
   try
   {
      for (bool reading : {false, true})
         for (bool folders : {false, true})
         {
            Context context{reading, true, true, folders, true, true, false, false};
            for (auto safe :
                 {SDL_FRect{0, 0, 240, 280}, SDL_FRect{12, 24, 320, 640}, SDL_FRect{24, 16, 640, 240}, SDL_FRect{0, 0, 1100, 800}})
               layout_test(safe, context);
            for (int width = 240; width <= 1100; width += 4)
               layout_test({0, 0, static_cast<float>(width), 640}, context);
            parity_test(context);
            if (reading)
            {
               context.controls_hidden = true;
               for (int width = 240; width <= 1100; width += 4)
                  layout_test({0, 0, static_cast<float>(width), 640}, context);
               require(navigation({0, 0, 360, 640}, context).empty(), "reading mode hides footer controls");
               auto floating = toolbar({0, 0, 360, 640}, context);
               require(
                  floating.size() == 1 && floating.front().action == Action::ReadingMode,
                  "reading mode always has a visible mouse/touch exit"
               );
               parity_test(context);
            }
         }
      Context empty;
      require(!enabled(Action::First, empty) && !enabled(Action::ResetZoom, empty), "empty-state reader actions disabled");
      Context first{true, true, true, false, false, true};
      require(!enabled(Action::Previous, first) && enabled(Action::Next, first), "page bounds reflected by controls");
      std::cout << "Mouse/touch control parity and responsive layouts passed.\n";
      return 0;
   }
   catch (const std::exception& error)
   {
      std::cerr << error.what() << '\n';
      return 1;
   }
}
