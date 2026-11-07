#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include "reader.hpp"
#include "text.hpp"
#include "controls.hpp"
#include "icons.hpp"
#include <sstream>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>
#include <unordered_map>

namespace {
using namespace oneam;
constexpr SDL_Color ink{233, 234, 237, 255}, muted{157, 164, 179, 255}, accent{137, 194, 176, 255};
struct TextureDeleter {
  void operator()(SDL_Texture *texture) const { SDL_DestroyTexture(texture); }
};
using Texture = std::unique_ptr<SDL_Texture, TextureDeleter>;
struct Inbox {
  std::mutex mutex;
  std::optional<std::string> selected;
  std::optional<std::vector<std::string>> library;
  std::string error;
  std::atomic<bool> alive{true}, scanning{false};
  bool dialog = false, folder = false;
};
struct Dialog {
  std::shared_ptr<Inbox> inbox;
  bool folder;
};
void SDLCALL selected(void *data, const char *const *paths, int) {
  std::unique_ptr<Dialog> context(static_cast<Dialog *>(data));
  auto &inbox = *context->inbox;
  std::lock_guard lock(inbox.mutex);
  inbox.dialog = false;
  if (!inbox.alive)
    return;
  if (!paths)
    inbox.error = SDL_GetError();
  else if (paths[0]) {
    inbox.selected = paths[0];
    inbox.folder = context->folder;
  }
}
std::string basename(const std::string &source) {
  auto end = source.find_last_of("/\\");
  return source.substr(end == source.npos ? 0 : end + 1);
}
std::string utf8(const std::filesystem::path &path) {
  const auto value = path.u8string();
  return {reinterpret_cast<const char *>(value.data()), value.size()};
}
struct Scan {
  std::shared_ptr<Inbox> inbox;
  std::string path;
};
int scan_folder(void *pointer) {
  std::unique_ptr<Scan> job(static_cast<Scan *>(pointer));
  std::vector<std::string> books;
  std::error_code ec;
  for (std::filesystem::directory_iterator
           it(std::filesystem::path(std::u8string(job->path.begin(), job->path.end())), ec),
       end;
       !ec && it != end && job->inbox->alive; it.increment(ec)) {
    if (it->is_regular_file(ec) && is_book(utf8(it->path())))
      books.push_back(utf8(it->path()));
    if (books.size() >= 10000)
      break;
  }
  std::sort(books.begin(), books.end(), [](const auto &a, const auto &b) { return natural_less(a, b); });
  std::lock_guard lock(job->inbox->mutex);
  job->inbox->library = std::move(books);
  if (ec)
    job->inbox->error = ec.message();
  job->inbox->scanning = false;
  return 0;
}
struct App {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  SDL_Cursor *hand_cursor = nullptr;
  std::unique_ptr<Text> text;
  std::unique_ptr<Settings> settings;
  std::unique_ptr<Reader> reader, covers;
  std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
  SDL_Thread *scanner = nullptr;
  std::shared_ptr<const Book> book;
  std::shared_ptr<const Image> image;
  Texture texture;
  std::string source, key, error, notice;
  std::vector<std::string> library;
  std::unordered_map<std::string, Texture> thumbnails;
  std::unordered_map<std::string, std::string> titles;
  std::string cover_source;
  std::uint64_t cover_generation = 0, generation = 0;
  std::size_t page = 0;
  std::size_t shelf_selection = 0;
  bool shelf = true, busy = false, fit_width = false, help = false, menu_open = false;
  float menu_scroll = 0, help_scroll = 0, pointer_x = -1, pointer_y = -1;
  bool smoke_menu = false, smoke_help = false;
  bool dirty = true, dragging = false, pinching = false, background = false;
  float width = 1000, height = 760, zoom = 1, pan_x = 0, pan_y = 0, scroll = 0, scroll_target = 0;
  float touch_x = 0, touch_y = 0, touch_start_x = 0, touch_start_y = 0, pinch_distance = 0;
  SDL_FingerID first_finger = 0, second_finger = 0;
  bool has_first = false, has_second = false;
  float second_x = 0, second_y = 0;
  float safe_x = 0, safe_y = 0, safe_w = 1000, safe_h = 760;
  float ui_scale = 1;
  std::uint64_t last_tick = 0;
  std::uint64_t redraw_until = 0;
  int smoke_frames = 0;
  std::string smoke_output;
  std::string startup_source;
  std::string prefs;

  ~App() {
    inbox->alive = false;
    if (scanner)
      SDL_WaitThread(scanner, nullptr);
    covers.reset();
    reader.reset();
    texture.reset();
    thumbnails.clear();
    text.reset();
    SDL_DestroyCursor(hand_cursor);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
  }
  bool init(int argc, char **argv) {
    SDL_SetAppMetadata("one a.m.", "2.0.0", "com.varnholt.oneam");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (!SDL_Init(SDL_INIT_VIDEO))
      return false;
    if (!SDL_CreateWindowAndRenderer(
            "one a.m.", 1100, 800, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer))
      return false;
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    hand_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    SDL_SetWindowMinimumSize(window, 320, 280);
    SDL_SetRenderLogicalPresentation(renderer, 1100, 800, SDL_LOGICAL_PRESENTATION_DISABLED);
    char *preference_path = SDL_GetPrefPath("varnholt", "oneam");
    if (preference_path) {
      prefs = preference_path;
      SDL_free(preference_path);
    }
    for (int i = 1; i < argc; ++i)
      if (std::string_view(argv[i]) == "--smoke") {
        prefs += "smoke/";
        break;
      }
    if (prefs.empty())
      return SDL_SetError("Cannot locate writable application settings.");
    settings = std::make_unique<Settings>(prefs);
    reader = std::make_unique<Reader>();
    covers = std::make_unique<Reader>(false, 0);
    text = std::make_unique<Text>(renderer);
    if (!text->valid())
      return SDL_SetError("Missing assets/NotoSans.ttf. Rebuild or reinstall oneam.");
    library = settings->recent();
    for (const auto &entry : library) {
      auto title = settings->title(book_key(entry));
      if (!title.empty())
        titles[entry] = std::move(title);
    }
#ifndef SDL_PLATFORM_ANDROID
    if (const auto folder = settings->folder(); !folder.empty())
      start_scan(folder);
#endif
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--smoke" && i + 1 < argc) {
        smoke_output = argv[++i];
        smoke_frames = 180;
      } else if (arg == "--smoke-menu")
        smoke_menu = true;
      else if (arg == "--smoke-help")
        smoke_help = true;
      else if (arg == "--size" && i + 2 < argc) {
        const int w = std::stoi(argv[++i]);
        const int h = std::stoi(argv[++i]);
        SDL_SetWindowSize(window, w, h);
      } else
        startup_source = arg;
    }
    if (!startup_source.empty())
      open(startup_source);
    menu_open = smoke_menu;
    help = smoke_help;
    last_tick = SDL_GetTicks();
    return true;
  }
  void dialog(bool folder = false) {
    {
      std::lock_guard lock(inbox->mutex);
      if (inbox->dialog)
        return;
      inbox->dialog = true;
    }
    static const SDL_DialogFileFilter filters[] = {{"Comic books", "cbz;cbr;cb7;cbt;zip;rar;7z;tar"},
                                                   {"All files", "*"}};
    auto *context = new Dialog{inbox, folder};
    if (folder)
      SDL_ShowOpenFolderDialog(selected, context, window, nullptr, false);
    else
      SDL_ShowOpenFileDialog(selected, context, window, filters, 2, nullptr, false);
  }
  void open(const std::string &path) {
    if (path.empty())
      return;
    menu_open = help = false;
    source = path;
    key = book_key(source);
    page = settings->position(key);
    book.reset();
    image.reset();
    texture.reset();
    shelf = false;
    error.clear();
    notice.clear();
    zoom = 1;
    pan_x = pan_y = 0;
    busy = true;
    generation = reader->request(source, page, true);
    dirty = true;
  }
  void start_scan(const std::string &path) {
    if (inbox->scanning) {
      notice = "A folder is already being scanned.";
      dirty = true;
      return;
    }
    if (scanner)
      SDL_WaitThread(scanner, nullptr);
    inbox->scanning = true;
    shelf = true;
    auto *scan = new Scan{inbox, path};
    scanner = SDL_CreateThread(scan_folder, "oneam-library", scan);
    if (!scanner) {
      delete scan;
      inbox->scanning = false;
      error = SDL_GetError();
    } else if (!settings->save_folder(path))
      notice = "Library folder could not be saved.";
    dirty = true;
  }
  std::string title(const std::string &path) const {
    auto found = titles.find(path);
    return found == titles.end() ? basename(path) : found->second;
  }
  void navigate(long long target) {
    if (!book || book->pages.empty())
      return;
    const auto next =
        static_cast<std::size_t>(std::clamp(target, 0ll, static_cast<long long>(book->pages.size() - 1)));
    if (next == page)
      return;
    page = next;
    busy = true;
    error.clear();
    generation = reader->request(source, page);
    dirty = true;
  }
  void resize() {
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    ui_scale =
        std::max(1.f, SDL_GetWindowDisplayScale(window) / std::max(.1f, SDL_GetWindowPixelDensity(window)));
    width = static_cast<float>(std::max(w, 1)) / ui_scale;
    height = static_cast<float>(std::max(h, 1)) / ui_scale;
    SDL_Rect area{};
    if (!SDL_GetWindowSafeArea(window, &area))
      area = {0, 0, w, h};
    safe_x = static_cast<float>(area.x) / ui_scale;
    safe_y = static_cast<float>(area.y) / ui_scale;
    safe_w = static_cast<float>(area.w) / ui_scale;
    safe_h = static_cast<float>(area.h) / ui_scale;
    int pixels_w = 0, pixels_h = 0;
    SDL_GetRenderOutputSize(renderer, &pixels_w, &pixels_h);
    SDL_SetRenderScale(renderer, pixels_w / width, pixels_h / height);
    scroll_target = std::clamp(scroll_target, 0.f, shelf_max());
    scroll = std::clamp(scroll, 0.f, shelf_max());
    clamp_pan();
    dirty = true;
    redraw_until = SDL_GetTicks() + 400;
  }
  float top() const { return safe_y + 72; }
  float bottom() const { return safe_y + safe_h - 72; }
  float view_height() const { return std::max(1.f, bottom() - top()); }
  float base_scale() const {
    if (!image)
      return 1;
    const float sx = std::max(1.f, safe_w - 32) / image->width;
    return fit_width ? sx : std::min(sx, view_height() / image->height);
  }
  void clamp_pan() {
    if (!image)
      return;
    const float scale = base_scale() * zoom;
    const float max_x = std::max(0.f, (image->width * scale - safe_w + 32) * .5f);
    const float max_y = std::max(0.f, image->height * scale - view_height());
    pan_x = std::clamp(pan_x, -max_x, max_x);
    pan_y = std::clamp(pan_y, 0.f, max_y);
  }
  void zoom_by(float factor) {
    zoom = std::clamp(zoom * factor, .25f, 8.f);
    clamp_pan();
    dirty = true;
  }
  void fullscreen() {
    SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
    dirty = true;
  }
  bool inside(float x, float y, const SDL_FRect &r) const {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
  }
  int columns() const { return std::max(1, static_cast<int>((safe_w - 40) / 184)); }
  float card_width() const { return (safe_w - 40) / columns(); }
  float shelf_heading_height() const { return view_height() < 330 ? 56.f : 90.f; }
  float card_height() const { return std::clamp(view_height() - shelf_heading_height() - 12, 120.f, 246.f); }
  float row_height() const { return card_height() + 14; }
  float shelf_max() const {
    const auto rows = (library.size() + columns() - 1) / columns();
    return std::max(0.f,
                    static_cast<float>(rows) * row_height() - view_height() + shelf_heading_height() + 20);
  }
  void select_book(long long index) {
    if (library.empty())
      return;
    shelf_selection =
        static_cast<std::size_t>(std::clamp(index, 0ll, static_cast<long long>(library.size() - 1)));
    const float y = shelf_heading_height() + static_cast<float>(shelf_selection / columns()) * row_height();
    if (y < scroll_target)
      scroll_target = y;
    if (y + card_height() > scroll_target + view_height())
      scroll_target = y + card_height() - view_height();
    scroll_target = std::clamp(scroll_target, 0.f, shelf_max());
    dirty = true;
  }
  SDL_FRect safe() const { return {safe_x, safe_y, safe_w, safe_h}; }
  ui::Context context() const {
    return {!shelf,
            shelf ? !library.empty() : bool(book),
            bool(image),
#ifdef SDL_PLATFORM_ANDROID
            false,
#else
            true,
#endif
            book && page > 0,
            book && page + 1 < book->pages.size(),
            bool(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN),
            fit_width};
  }
  SDL_FRect help_panel() const {
    const float w = std::min(safe_w - 24, 560.f), h = std::min(safe_h - 92, 440.f);
    return {safe_x + (safe_w - w) * .5f, safe_y + 80, w, h};
  }
  SDL_FRect help_viewport() const {
    auto r = help_panel();
    return {r.x + 20, r.y + 72, r.w - 40, std::max(1.f, r.h - 88)};
  }
  ui::Control help_close() const {
    auto r = help_panel();
    return {ui::Action::Close, {r.x + r.w - 60, r.y + 10, 48, 48}, {}, ui::Icon::Close};
  }
  std::vector<std::string> help_lines() {
#ifdef SDL_PLATFORM_ANDROID
    const std::array paragraphs{"Choose Open to pick a comic.",
                                "Use the bottom arrows to turn pages, or swipe across the page.",
                                "Pinch to zoom. Drag to pan or scroll.",
                                "Menu contains zoom, first and last page, fit modes and fullscreen.",
                                "Choose Library or Android Back to return to your books.",
                                "Scroll this panel to read more."};
#else
    const std::array paragraphs{
        "Open a book with Open or Ctrl+O. Folder opens your library directory.",
        "Use the bottom arrows or Left / Right to turn pages. Home / End jumps to the first / last page.",
        "Use the mouse wheel or drag to scroll. Ctrl+wheel zooms.",
        "Menu contains every reader command, including zoom, reset, fit modes and fullscreen.",
        "F11 toggles fullscreen. F1 opens help. Escape closes a panel or returns to the library.",
        "Keyboard shortcuts are optional. Every command is available with the mouse."};
#endif
    std::vector<std::string> result;
    const float available = help_viewport().w;
    for (const char *paragraph : paragraphs) {
      std::istringstream words(paragraph);
      std::string word, line;
      while (words >> word) {
        auto candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && text->width(candidate, 18) > available) {
          result.push_back(line);
          line = word;
        } else
          line = std::move(candidate);
      }
      if (!line.empty())
        result.push_back(line);
      result.emplace_back();
    }
    return result;
  }
  void scroll_view(float amount) {
    if (help)
      help_scroll = std::clamp(help_scroll + amount, 0.f,
                               std::max(0.f, help_lines().size() * 26.f - help_viewport().h));
    else if (menu_open) {
      auto layout = ui::menu(safe(), context(), menu_scroll);
      menu_scroll = std::clamp(menu_scroll + amount, 0.f, layout.max_scroll);
    } else if (shelf)
      scroll_target = std::clamp(scroll_target + amount, 0.f, shelf_max());
    else {
      pan_y += amount;
      clamp_pan();
    }
    dirty = true;
  }
  void perform(ui::Action action) {
    using A = ui::Action;
    if (!ui::enabled(action, context()))
      return;
    if (action == A::Menu) {
      menu_open = !menu_open;
      help = false;
      menu_scroll = 0;
      dirty = true;
      return;
    }
    if (action == A::Help) {
      help = !help;
      menu_open = false;
      help_scroll = 0;
      dirty = true;
      return;
    }
    if (action == A::Close) {
      help = menu_open = false;
      dirty = true;
      return;
    }
    if (action == A::Back) {
      if (help)
        help = false;
      else if (menu_open)
        menu_open = false;
      else
        shelf = true;
      dirty = true;
      return;
    }
    menu_open = false;
    switch (action) {
    case A::Open:
      dialog();
      break;
    case A::Folder:
      dialog(true);
      break;
    case A::Library:
      shelf = true;
      help = false;
      break;
    case A::Previous:
      navigate(static_cast<long long>(page) - 1);
      break;
    case A::Next:
      navigate(static_cast<long long>(page) + 1);
      break;
    case A::First:
      if (shelf)
        select_book(0);
      else
        navigate(0);
      break;
    case A::Last:
      if (shelf)
        select_book(static_cast<long long>(library.size()) - 1);
      else
        navigate(static_cast<long long>(book->pages.size()) - 1);
      break;
    case A::ZoomOut:
      zoom_by(1 / 1.2f);
      break;
    case A::ZoomIn:
      zoom_by(1.2f);
      break;
    case A::ResetZoom:
      zoom = 1;
      pan_x = pan_y = 0;
      break;
    case A::ToggleFit:
      fit_width = !fit_width;
      zoom = 1;
      pan_x = pan_y = 0;
      break;
    case A::FitPage:
      fit_width = false;
      zoom = 1;
      pan_x = pan_y = 0;
      break;
    case A::FitWidth:
      fit_width = true;
      zoom = 1;
      pan_x = pan_y = 0;
      break;
    case A::Fullscreen:
      fullscreen();
      break;
    case A::ScrollUp:
      if (shelf)
        scroll_target = std::max(0.f, scroll_target - 70);
      else {
        pan_y -= 70;
        clamp_pan();
      }
      break;
    case A::ScrollDown:
      if (shelf)
        scroll_target = std::min(shelf_max(), scroll_target + 70);
      else {
        pan_y += 70;
        clamp_pan();
      }
      break;
    default:
      break;
    }
    dirty = true;
  }
  std::optional<ui::Action> control_at(float x, float y) const {
    if (help) {
      if (ui::contains(help_close().rect, x, y))
        return ui::Action::Close;
      return {};
    }
    if (menu_open)
      return ui::hit(ui::menu(safe(), context(), menu_scroll), x, y);
    if (auto action = ui::hit(ui::toolbar(safe(), context()), x, y))
      return action;
    return ui::hit(ui::navigation(safe(), context()), x, y);
  }
  void click(float x, float y) {
    if (auto action = control_at(x, y)) {
      perform(*action);
      return;
    }
    if (help) {
      if (!ui::contains(help_panel(), x, y))
        perform(ui::Action::Close);
      return;
    }
    if (menu_open) {
      if (!ui::contains(ui::menu(safe(), context(), menu_scroll).panel, x, y))
        perform(ui::Action::Close);
      return;
    }
    if (y < top() || y >= bottom())
      return;
    if (shelf) {
      if (library.empty()) {
        dialog();
        return;
      }
      const float origin = top() + shelf_heading_height() - scroll;
      for (std::size_t i = 0; i < library.size(); ++i) {
        SDL_FRect r{safe_x + 20 + static_cast<float>(i % columns()) * card_width(),
                    origin + static_cast<float>(i / columns()) * row_height(), card_width() - 12,
                    card_height()};
        if (inside(x, y, r)) {
          open(library[i]);
          break;
        }
      }
    } else {
      if (x < safe_x + safe_w * .25f)
        perform(ui::Action::Previous);
      else if (x > safe_x + safe_w * .75f)
        perform(ui::Action::Next);
    }
  }
  void keydown(const SDL_KeyboardEvent &event) {
    if ((help || menu_open) && (event.key == SDLK_UP || event.key == SDLK_DOWN || event.key == SDLK_PAGEUP ||
                                event.key == SDLK_PAGEDOWN)) {
      scroll_view((event.key == SDLK_UP || event.key == SDLK_PAGEUP) ? -70.f : 70.f);
      return;
    }
    if (auto action = ui::keyboard(event.key, event.mod, !shelf)) {
      if (event.repeat && (*action == ui::Action::Open || *action == ui::Action::Help ||
                           *action == ui::Action::Fullscreen || *action == ui::Action::Back))
        return;
      perform(*action);
      return;
    }
    if (!shelf)
      return;
    if (event.key == SDLK_O && !event.repeat)
      perform(ui::Action::Open);
    if (event.key == SDLK_RETURN && !event.repeat) {
      if (library.empty())
        dialog();
      else
        open(library[std::min(shelf_selection, library.size() - 1)]);
    }
    if (event.key == SDLK_DOWN)
      select_book(static_cast<long long>(shelf_selection) + columns());
    if (event.key == SDLK_UP)
      select_book(static_cast<long long>(shelf_selection) - columns());
    if (event.key == SDLK_RIGHT)
      select_book(static_cast<long long>(shelf_selection) + 1);
    if (event.key == SDLK_LEFT)
      select_book(static_cast<long long>(shelf_selection) - 1);
    if (event.key == SDLK_PAGEDOWN)
      scroll_view(view_height());
    if (event.key == SDLK_PAGEUP)
      scroll_view(-view_height());
  }
  SDL_AppResult event(SDL_Event &event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      event.button.x /= ui_scale;
      event.button.y /= ui_scale;
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
      event.motion.x /= ui_scale;
      event.motion.y /= ui_scale;
      event.motion.xrel /= ui_scale;
      event.motion.yrel /= ui_scale;
    }
    switch (event.type) {
    case SDL_EVENT_QUIT:
      return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
      keydown(event.key);
      break;
    case SDL_EVENT_DROP_FILE:
      if (event.drop.data)
        open(event.drop.data);
      break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
      resize();
      break;
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
      dirty = true;
      redraw_until = SDL_GetTicks() + 400;
      break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      dragging = has_first = has_second = pinching = false;
      break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
      pointer_x = pointer_y = -1;
      dirty = true;
      SDL_SetCursor(SDL_GetDefaultCursor());
      break;
    case SDL_EVENT_RENDER_DEVICE_RESET:
      texture.reset();
      thumbnails.clear();
      text = std::make_unique<Text>(renderer);
      upload();
      resize();
      break;
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
      background = true;
      break;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
      background = false;
      resize();
      break;
    case SDL_EVENT_LOW_MEMORY:
      thumbnails.clear();
      reader->trim_cache();
      covers->trim_cache();
      dirty = true;
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      if (!help && !menu_open && (SDL_GetModState() & SDL_KMOD_CTRL))
        zoom_by(std::pow(1.12f, event.wheel.y));
      else
        scroll_view(-event.wheel.y * 70);
      dirty = true;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      if (event.button.button == SDL_BUTTON_RIGHT) {
        perform(ui::Action::Menu);
        break;
      }
      if (event.button.button == SDL_BUTTON_LEFT) {
        dragging = true;
        touch_start_x = touch_x = event.button.x;
        touch_start_y = touch_y = event.button.y;
      }
      break;
    case SDL_EVENT_MOUSE_MOTION: {
      const auto previous_hover = control_at(pointer_x, pointer_y);
      pointer_x = event.motion.x;
      pointer_y = event.motion.y;
      if (previous_hover != control_at(pointer_x, pointer_y))
        dirty = true;
      if (hand_cursor)
        SDL_SetCursor(control_at(pointer_x, pointer_y) ? hand_cursor : SDL_GetDefaultCursor());
      if (dragging) {
        if (help || menu_open || shelf)
          scroll_view(-event.motion.yrel);
        else {
          pan_x += event.motion.xrel;
          pan_y -= event.motion.yrel;
          clamp_pan();
        }
        touch_x = event.motion.x;
        touch_y = event.motion.y;
        dirty = true;
      }
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (dragging && event.button.button == SDL_BUTTON_LEFT) {
        dragging = false;
        if (std::hypot(event.button.x - touch_start_x, event.button.y - touch_start_y) < 8)
          click(event.button.x, event.button.y);
      }
      break;
    case SDL_EVENT_FINGER_DOWN:
      if (!has_first) {
        has_first = true;
        first_finger = event.tfinger.fingerID;
        touch_start_x = touch_x = event.tfinger.x * width;
        touch_start_y = touch_y = event.tfinger.y * height;
        pinching = false;
      } else if (!has_second) {
        has_second = true;
        second_finger = event.tfinger.fingerID;
        second_x = event.tfinger.x * width;
        second_y = event.tfinger.y * height;
        pinch_distance = std::hypot(second_x - touch_x, second_y - touch_y);
        pinching = true;
      }
      break;
    case SDL_EVENT_FINGER_MOTION: {
      const float x = event.tfinger.x * width, y = event.tfinger.y * height;
      if (has_second) {
        if (event.tfinger.fingerID == first_finger) {
          touch_x = x;
          touch_y = y;
        } else if (event.tfinger.fingerID == second_finger) {
          second_x = x;
          second_y = y;
        }
        const float distance = std::hypot(second_x - touch_x, second_y - touch_y);
        if (!shelf && !help && !menu_open && pinch_distance > 1)
          zoom_by(distance / pinch_distance);
        pinch_distance = distance;
      } else if (event.tfinger.fingerID == first_finger) {
        if (help || menu_open || shelf)
          scroll_view(-(y - touch_y));
        else {
          pan_x += x - touch_x;
          pan_y -= y - touch_y;
          clamp_pan();
        }
        touch_x = x;
        touch_y = y;
      }
      dirty = true;
      break;
    }
    case SDL_EVENT_FINGER_UP:
      if (event.tfinger.fingerID == second_finger && has_second)
        has_second = false;
      else if (event.tfinger.fingerID == first_finger && has_first) {
        has_first = false;
        const float dx = event.tfinger.x * width - touch_start_x,
                    dy = event.tfinger.y * height - touch_start_y;
        if (!pinching) {
          if (std::hypot(dx, dy) < 12)
            click(touch_start_x, touch_start_y);
          else if (!shelf && !menu_open && !help && touch_start_y >= top() && touch_start_y < bottom() &&
                   zoom <= 1.01f && std::abs(dx) > 70 && std::abs(dx) > std::abs(dy) * 1.5f)
            navigate(static_cast<long long>(page) + (dx < 0 ? 1 : -1));
        }
        has_second = false;
      }
      break;
    default:
      break;
    }
    return SDL_APP_CONTINUE;
  }
  void upload() {
    texture.reset();
    if (!image)
      return;
    texture.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image->width,
                                    image->height));
    if (!texture || !SDL_UpdateTexture(texture.get(), nullptr, image->pixels.data(), image->width * 4)) {
      error = SDL_GetError();
      texture.reset();
      return;
    }
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_LINEAR);
  }
  void tick() {
    {
      std::optional<std::string> chosen;
      bool folder = false;
      {
        std::lock_guard lock(inbox->mutex);
        chosen = std::move(inbox->selected);
        inbox->selected.reset();
        folder = inbox->folder;
        if (!inbox->error.empty()) {
          error = std::move(inbox->error);
          inbox->error.clear();
          dirty = true;
        }
        if (inbox->library) {
          library = std::move(*inbox->library);
          inbox->library.reset();
          scroll = scroll_target = 0;
          shelf_selection = 0;
          thumbnails.clear();
          cover_source.clear();
          cover_generation = 0;
          dirty = true;
        }
      }
      if (chosen) {
        if (folder) {
          start_scan(*chosen);
        } else
          open(*chosen);
      }
    }
    if (auto result = reader->poll(); result && result->generation == generation) {
      busy = false;
      book = result->book;
      page = result->page;
      error = result->error;
      image = result->image;
      upload();
      pan_x = pan_y = 0;
      clamp_pan();
      if (image) {
        if (!titles.contains(source) || titles[source] != book->title) {
          titles[source] = book->title;
          settings->save_title(key, book->title);
        }
        if (!settings->save_position(key, page))
          notice = "Reading position could not be saved.";
        if (!settings->remember(source))
          notice = "Recent books could not be saved.";
        if (std::find(library.begin(), library.end(), source) == library.end())
          library.insert(library.begin(), source);
        const auto window_title = title(source) + "  |  " + std::to_string(page + 1) + " / " +
                                  std::to_string(book->pages.size()) + "  |  one a.m.";
        SDL_SetWindowTitle(window, window_title.c_str());
        SDL_Log("Page %zu: %.2f ms%s", page + 1, result->milliseconds, result->cached ? " (cache)" : "");
      }
      dirty = true;
    }
    if (auto result = covers->poll(); result && result->generation == cover_generation) {
      if (result->book)
        titles[cover_source] = result->book->title;
      Texture thumbnail;
      if (result->image) {
        const auto &im = *result->image;
        const float scale = std::min(384.f / im.width, 512.f / im.height);
        const int w = std::max(1, static_cast<int>(im.width * scale)),
                  h = std::max(1, static_cast<int>(im.height * scale));
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4);
        for (int y = 0; y < h; ++y)
          for (int x = 0; x < w; ++x) {
            const auto src = (static_cast<std::size_t>(y * im.height / h) * im.width + x * im.width / w) * 4;
            const auto dst = (static_cast<std::size_t>(y) * w + x) * 4;
            std::copy_n(im.pixels.data() + src, 4, pixels.data() + dst);
          }
        thumbnail.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h));
        if (thumbnail) {
          SDL_UpdateTexture(thumbnail.get(), nullptr, pixels.data(), w * 4);
          SDL_SetTextureScaleMode(thumbnail.get(), SDL_SCALEMODE_LINEAR);
        }
      }
      if (thumbnails.size() >= 48)
        thumbnails.clear();
      thumbnails.insert_or_assign(cover_source, std::move(thumbnail));
      cover_source.clear();
      dirty = true;
    }
    const auto now = SDL_GetTicks();
    const auto dt = std::min(.05f, (now - last_tick) / 1000.f);
    last_tick = now;
    if (std::abs(scroll_target - scroll) > .1f) {
      scroll += (scroll_target - scroll) * std::min(1.f, dt * 18);
      dirty = true;
    } else
      scroll = scroll_target;
  }
  void fill(SDL_FRect r, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &r);
  }
  void control(const ui::Control &item, bool menu_item = false) {
    const bool hover = item.enabled && ui::contains(item.rect, pointer_x, pointer_y);
    const SDL_Color surface = item.active ? SDL_Color{43, 72, 64, 255}
                              : hover     ? SDL_Color{51, 59, 71, 255}
                                          : SDL_Color{32, 38, 48, 255};
    ui::rounded_rect(renderer, item.rect, 10, hover ? SDL_Color{80, 103, 111, 255} : surface);
    SDL_FRect inner{item.rect.x + 1, item.rect.y + 1, item.rect.w - 2, item.rect.h - 2};
    ui::rounded_rect(renderer, inner, 9, surface);
    const auto foreground = !item.enabled ? SDL_Color{93, 103, 119, 255} : item.active ? accent : ink;
    const float icon_x = item.label.empty() ? item.rect.x + (item.rect.w - 24) * .5f : item.rect.x + 13;
    ui::draw_icon(renderer, item.icon, {icon_x, item.rect.y + 12, 24, 24}, foreground);
    float reserved = 0;
#ifndef SDL_PLATFORM_ANDROID
    const auto hint = menu_item ? ui::shortcut(item.action) : std::string_view{};
    if (!hint.empty() && item.rect.w >= 230) {
      reserved = text->width(hint, 14) + 16;
      text->draw(hint, item.rect.x + item.rect.w - reserved, item.rect.y + 17, 14, muted);
    }
#else
    (void)menu_item;
#endif
    if (!item.label.empty())
      text->draw(item.label, item.rect.x + 47, item.rect.y + 15, 18, foreground, item.rect.w - 59 - reserved);
  }
  void scroll_indicator(SDL_FRect viewport, float offset, float maximum, float content) {
    if (maximum <= 0)
      return;
    const float h = std::min(viewport.h, std::max(24.f, viewport.h * viewport.h / content));
    ui::rounded_rect(renderer, {viewport.x + viewport.w + 5, viewport.y, 3, viewport.h}, 1.5f,
                     {51, 59, 70, 255});
    ui::rounded_rect(renderer,
                     {viewport.x + viewport.w + 5, viewport.y + (viewport.h - h) * offset / maximum, 3, h},
                     1.5f, {132, 156, 155, 255});
  }
  void render_menu() {
    const auto layout = ui::menu(safe(), context(), menu_scroll);
    menu_scroll = layout.scroll;
    fill(safe(), {0, 0, 0, 120});
    ui::rounded_rect(renderer, {layout.panel.x + 2, layout.panel.y + 4, layout.panel.w, layout.panel.h}, 14,
                     {0, 0, 0, 100});
    ui::rounded_rect(renderer, layout.panel, 14, {27, 33, 43, 255});
    text->draw(layout.panel.w >= 280 ? "Controls" : "Menu", layout.panel.x + 20, layout.panel.y + 22, 23, ink,
               layout.panel.w - 140);
    for (const auto &item : layout.fixed)
      control(item);
    SDL_Rect clip{static_cast<int>(layout.viewport.x), static_cast<int>(layout.viewport.y),
                  static_cast<int>(layout.viewport.w), static_cast<int>(layout.viewport.h)};
    SDL_SetRenderClipRect(renderer, &clip);
    for (const auto &item : layout.items)
      if (item.rect.y + item.rect.h > layout.viewport.y &&
          item.rect.y < layout.viewport.y + layout.viewport.h)
        control(item, true);
    SDL_SetRenderClipRect(renderer, nullptr);
    scroll_indicator(layout.viewport, layout.scroll, layout.max_scroll, layout.content_height);
  }
  void render_help() {
    const auto panel = help_panel(), viewport = help_viewport();
    auto lines = help_lines();
    const float content = static_cast<float>(lines.size()) * 26;
    const float maximum = std::max(0.f, content - viewport.h);
    help_scroll = std::clamp(help_scroll, 0.f, maximum);
    fill(safe(), {0, 0, 0, 120});
    ui::rounded_rect(renderer, panel, 14, {27, 33, 43, 255});
    ui::draw_icon(renderer, ui::Icon::Help, {panel.x + 20, panel.y + 20, 26, 26}, accent);
    text->draw(panel.w >= 300 ? "Reader help" : "Help", panel.x + 58, panel.y + 22, 23, ink, panel.w - 130);
    SDL_Rect clip{static_cast<int>(viewport.x), static_cast<int>(viewport.y), static_cast<int>(viewport.w),
                  static_cast<int>(viewport.h)};
    SDL_SetRenderClipRect(renderer, &clip);
    float y = viewport.y - help_scroll;
    for (const auto &line : lines) {
      if (y + 26 > viewport.y && y < viewport.y + viewport.h)
        text->draw(line, viewport.x, y, 18, ink, viewport.w);
      y += 26;
    }
    SDL_SetRenderClipRect(renderer, nullptr);
    scroll_indicator(viewport, help_scroll, maximum, content);
    control(help_close());
  }
  void render() {
    SDL_SetRenderDrawColor(renderer, 19, 22, 28, 255);
    SDL_RenderClear(renderer);
    fill({safe_x, safe_y, safe_w, 72}, {25, 30, 39, 255});
    if (shelf) {
      ui::draw_icon(renderer, ui::Icon::Book, {safe_x + 20, safe_y + 22, 28, 28}, accent);
      float brand_edge = safe_x + safe_w;
      for (const auto &item : ui::toolbar(safe(), context()))
        brand_edge = std::min(brand_edge, item.rect.x - 12);
      if (brand_edge - (safe_x + 60) >= text->width("one a.m.", 27))
        text->draw("one a.m.", safe_x + 60, safe_y + 22, 27, ink);
    }
    for (const auto &item : ui::toolbar(safe(), context()))
      control(item);
    SDL_Rect clip{static_cast<int>(safe_x), static_cast<int>(top()), static_cast<int>(safe_w),
                  static_cast<int>(view_height())};
    SDL_SetRenderClipRect(renderer, &clip);
    if (shelf)
      render_shelf();
    else if (texture && image) {
      const float scale = base_scale() * zoom, w = image->width * scale, h = image->height * scale;
      SDL_FRect destination{safe_x + (safe_w - w) * .5f + pan_x,
                            top() + std::max(0.f, (view_height() - h) * .5f) - pan_y, w, h};
      SDL_RenderTexture(renderer, texture.get(), nullptr, &destination);
    }
    if (!shelf && (busy || !error.empty())) {
      fill({safe_x + 20, top() + 22, safe_w - 40, 78}, {35, 40, 50, 245});
      text->draw(busy ? "Opening your page..." : "This book needs attention", safe_x + 36, top() + 32, 23,
                 ink, safe_w - 72);
      text->draw(busy ? "You can keep navigating while it loads." : error, safe_x + 36, top() + 63, 18, muted,
                 safe_w - 72);
    }
    SDL_SetRenderClipRect(renderer, nullptr);
    fill({safe_x, bottom(), safe_w, 72}, {25, 30, 39, 255});
    if (shelf) {
      ui::draw_icon(renderer, ui::Icon::Book, {safe_x + 20, bottom() + 24, 24, 24}, muted);
      const auto count = std::to_string(library.size()) + (library.size() == 1 ? " book" : " books");
      text->draw(notice.empty() ? count : notice, safe_x + 56, bottom() + 27, 18, muted, safe_w - 76);
    } else {
      const auto buttons = ui::navigation(safe(), context());
      for (const auto &item : buttons)
        control(item);
      const float left = buttons.front().rect.x + buttons.front().rect.w + 12;
      const float available = std::max(1.f, buttons.back().rect.x - left - 12);
      const auto position =
          book ? std::to_string(page + 1) + " / " + std::to_string(book->pages.size()) : "Opening";
      text->draw(position, left + std::max(0.f, (available - text->width(position, 21)) * .5f), bottom() + 14,
                 21, ink, available);
      const auto caption = notice.empty() ? (busy ? "Loading page" : title(source)) : notice;
      text->draw(caption, left, bottom() + 43, 14, muted, available);
    }
    if (menu_open)
      render_menu();
    if (help)
      render_help();
    if (smoke_frames == 1 && !smoke_output.empty()) {
      SDL_Surface *snapshot = SDL_RenderReadPixels(renderer, nullptr);
      if (snapshot) {
        SDL_SaveBMP(snapshot, smoke_output.c_str());
        SDL_DestroySurface(snapshot);
      }
    }
    SDL_RenderPresent(renderer);
    dirty = false;
  }
  void render_shelf() {
    const float origin = top() + shelf_heading_height() - scroll;
    text->draw("Your reading shelf", safe_x + 22, top() + 16 - scroll,
               shelf_heading_height() < 90 ? 26.f : 30.f, ink);
#ifdef SDL_PLATFORM_ANDROID
    if (shelf_heading_height() >= 90)
      text->draw("Tap a cover to continue reading.", safe_x + 22, top() + 57 - scroll, 18, muted,
                 safe_w - 44);
#else
    if (shelf_heading_height() >= 90)
      text->draw(inbox->scanning ? "Looking for books..."
                                 : "Pick up where you left off. Menu has all controls.",
                 safe_x + 22, top() + 57 - scroll, 18, muted, safe_w - 44);
#endif
    if (library.empty()) {
      text->draw("Your next good read.", safe_x + 28, top() + 150, 28, ink, safe_w - 56);
      text->draw("Choose Open to pick a comic.", safe_x + 28, top() + 198, 19, muted, safe_w - 56);
      if (!error.empty())
        text->draw(error, safe_x + 28, top() + 244, 18, accent, safe_w - 56);
    }
    for (std::size_t i = 0; i < library.size(); ++i) {
      const float x = safe_x + 20 + static_cast<float>(i % columns()) * card_width();
      const float y = origin + static_cast<float>(i / columns()) * row_height();
      if (y + card_height() < top() || y > bottom())
        continue;
      const float w = card_width() - 12;
      ui::rounded_rect(renderer, {x, y, w, card_height()}, 12,
                       i == shelf_selection ? SDL_Color{43, 64, 60, 255} : SDL_Color{30, 35, 44, 255});
      if (auto found = thumbnails.find(library[i]); found != thumbnails.end() && found->second) {
        float tw = 0, th = 0;
        SDL_GetTextureSize(found->second.get(), &tw, &th);
        const float cover_height = card_height() - 50;
        const float scale = std::min((w - 20) / tw, cover_height / th);
        SDL_FRect destination{x + (w - tw * scale) * .5f, y + 10 + (cover_height - th * scale) * .5f,
                              tw * scale, th * scale};
        SDL_RenderTexture(renderer, found->second.get(), nullptr, &destination);
      } else {
        fill({x + 12, y + 12, w - 24, card_height() - 58}, {43, 53, 65, 255});
        ui::draw_icon(renderer, ui::Icon::Book,
                      {x + (w - 44) * .5f, y + (card_height() - 50 - 44) * .5f + 10, 44, 44}, muted);
        if (cover_source.empty() && !thumbnails.contains(library[i]) && !busy) {
          cover_source = library[i];
          cover_generation = covers->request(cover_source, 0);
        }
      }
      text->draw(title(library[i]), x + 10, y + card_height() - 34, 17, ink, w - 20);
    }
  }
  SDL_AppResult iterate() {
    tick();
    if (background) {
      SDL_Delay(40);
      return SDL_APP_CONTINUE;
    }
    // Android can replace the surface after the first resize notification.
    // Repaint through that short transition, then return to idle rendering.
    if (SDL_GetTicks() < redraw_until)
      dirty = true;
    if (smoke_frames == 1)
      dirty = true;
    if (dirty)
      render();
    else
      SDL_Delay(12);
    if (smoke_frames > 0 && --smoke_frames == 0) {
      if (!startup_source.empty() && (!image || busy || !error.empty()))
        return SDL_APP_FAILURE;
      return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
  }
};
} // namespace
SDL_AppResult SDL_AppInit(void **state, int argc, char **argv) {
  try {
    auto app = std::make_unique<App>();
    if (!app->init(argc, argv)) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "one a.m.", SDL_GetError(), nullptr);
      return SDL_APP_FAILURE;
    }
    app->resize();
    *state = app.release();
    return SDL_APP_CONTINUE;
  } catch (const std::exception &ex) {
    SDL_Log("Startup failed: %s", ex.what());
    return SDL_APP_FAILURE;
  }
}
SDL_AppResult SDL_AppEvent(void *state, SDL_Event *event) {
  try {
    return static_cast<App *>(state)->event(*event);
  } catch (const std::exception &ex) {
    SDL_Log("Event failed: %s", ex.what());
    return SDL_APP_FAILURE;
  }
}
SDL_AppResult SDL_AppIterate(void *state) {
  try {
    return static_cast<App *>(state)->iterate();
  } catch (const std::exception &ex) {
    SDL_Log("Reader failed: %s", ex.what());
    return SDL_APP_FAILURE;
  }
}
void SDL_AppQuit(void *state, SDL_AppResult) {
  delete static_cast<App *>(state);
  SDL_Quit();
}
