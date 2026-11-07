#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include "reader.hpp"
#include "text.hpp"
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
struct TextureDeleter { void operator()(SDL_Texture* texture) const { SDL_DestroyTexture(texture); } };
using Texture = std::unique_ptr<SDL_Texture, TextureDeleter>;
struct Inbox {
  std::mutex mutex;
  std::optional<std::string> selected;
  std::optional<std::vector<std::string>> library;
  std::string error;
  std::atomic<bool> alive{true}, scanning{false};
  bool dialog = false, folder = false;
};
struct Dialog { std::shared_ptr<Inbox> inbox; bool folder; };
void SDLCALL selected(void* data, const char* const* paths, int) {
  std::unique_ptr<Dialog> context(static_cast<Dialog*>(data));
  auto& inbox = *context->inbox;
  std::lock_guard lock(inbox.mutex);
  inbox.dialog = false;
  if (!inbox.alive) return;
  if (!paths) inbox.error = SDL_GetError();
  else if (paths[0]) { inbox.selected = paths[0]; inbox.folder = context->folder; }
}
std::string basename(const std::string& source) {
  auto end = source.find_last_of("/\\");
  return source.substr(end == source.npos ? 0 : end + 1);
}
std::string utf8(const std::filesystem::path& path) {
  const auto value = path.u8string();
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}
struct Scan { std::shared_ptr<Inbox> inbox; std::string path; };
int scan_folder(void* pointer) {
  std::unique_ptr<Scan> job(static_cast<Scan*>(pointer));
  std::vector<std::string> books;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(std::filesystem::path(std::u8string(job->path.begin(), job->path.end())), ec), end;
       !ec && it != end && job->inbox->alive; it.increment(ec)) {
    if (it->is_regular_file(ec) && is_book(utf8(it->path()))) books.push_back(utf8(it->path()));
    if (books.size() >= 10000) break;
  }
  std::sort(books.begin(), books.end(), [](const auto& a, const auto& b) { return natural_less(a, b); });
  std::lock_guard lock(job->inbox->mutex);
  job->inbox->library = std::move(books);
  if (ec) job->inbox->error = ec.message();
  job->inbox->scanning = false;
  return 0;
}
struct App {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  std::unique_ptr<Text> text;
  std::unique_ptr<Settings> settings;
  std::unique_ptr<Reader> reader, covers;
  std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
  SDL_Thread* scanner = nullptr;
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
  bool shelf = true, busy = false, fit_width = false, help = false;
  bool dirty = true, dragging = false, pinching = false, background = false;
  float width = 1000, height = 760, zoom = 1, pan_x = 0, pan_y = 0, scroll = 0, scroll_target = 0;
  float touch_x = 0, touch_y = 0, touch_start_x = 0, touch_start_y = 0, pinch_distance = 0;
  SDL_FingerID first_finger = 0, second_finger = 0;
  bool has_first = false, has_second = false;
  float second_x = 0, second_y = 0;
  float safe_x = 0, safe_y = 0, safe_w = 1000, safe_h = 760;
  float ui_scale = 1;
  std::uint64_t last_tick = 0;
  int smoke_frames = 0;
  std::string smoke_output;
  std::string startup_source;
  std::string prefs;

  ~App() {
    inbox->alive = false;
    if (scanner) SDL_WaitThread(scanner, nullptr);
    covers.reset(); reader.reset();
    texture.reset(); thumbnails.clear(); text.reset();
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
  }
  bool init(int argc, char** argv) {
    SDL_SetAppMetadata("oneam", "2.0.0", "com.varnholt.oneam");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (!SDL_Init(SDL_INIT_VIDEO)) return false;
    if (!SDL_CreateWindowAndRenderer("oneam", 1100, 800, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer)) return false;
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, 1100, 800, SDL_LOGICAL_PRESENTATION_DISABLED);
    char* preference_path = SDL_GetPrefPath("varnholt", "oneam");
    if (preference_path) { prefs = preference_path; SDL_free(preference_path); }
    if (prefs.empty()) return SDL_SetError("Cannot locate writable application settings.");
    settings = std::make_unique<Settings>(prefs);
    reader = std::make_unique<Reader>(); covers = std::make_unique<Reader>(false, 0);
    text = std::make_unique<Text>(renderer);
    if (!text->valid()) return SDL_SetError("Missing assets/NotoSans.ttf. Rebuild or reinstall oneam.");
    library = settings->recent();
    for (const auto& entry : library) {
      auto title = settings->title(book_key(entry));
      if (!title.empty()) titles[entry] = std::move(title);
    }
#ifndef SDL_PLATFORM_ANDROID
    if (const auto folder = settings->folder(); !folder.empty()) start_scan(folder);
#endif
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--smoke" && i + 1 < argc) { smoke_output = argv[++i]; smoke_frames = 180; }
      else startup_source = arg;
    }
    if (!startup_source.empty()) open(startup_source);
    last_tick = SDL_GetTicks();
    return true;
  }
  void dialog(bool folder = false) {
    { std::lock_guard lock(inbox->mutex); if (inbox->dialog) return; inbox->dialog = true; }
    static const SDL_DialogFileFilter filters[] = {{"Comic books", "cbz;cbr;cb7;cbt;zip;rar;7z;tar"}, {"All files", "*"}};
    auto* context = new Dialog{inbox, folder};
    if (folder) SDL_ShowOpenFolderDialog(selected, context, window, nullptr, false);
    else SDL_ShowOpenFileDialog(selected, context, window, filters, 2, nullptr, false);
  }
  void open(const std::string& path) {
    if (path.empty()) return;
    source = path; key = book_key(source); page = settings->position(key);
    book.reset(); image.reset(); texture.reset(); shelf = false; error.clear(); notice.clear();
    zoom = 1; pan_x = pan_y = 0; busy = true;
    generation = reader->request(source, page, true); dirty = true;
  }
  void start_scan(const std::string& path) {
    if (inbox->scanning) { notice = "A folder is already being scanned."; dirty = true; return; }
    if (scanner) SDL_WaitThread(scanner, nullptr);
    inbox->scanning = true; shelf = true;
    auto* scan = new Scan{inbox, path}; scanner = SDL_CreateThread(scan_folder, "oneam-library", scan);
    if (!scanner) { delete scan; inbox->scanning = false; error = SDL_GetError(); }
    else if (!settings->save_folder(path)) notice = "Library folder could not be saved.";
    dirty = true;
  }
  std::string title(const std::string& path) const {
    auto found = titles.find(path);
    return found == titles.end() ? basename(path) : found->second;
  }
  void navigate(long long target) {
    if (!book || book->pages.empty()) return;
    const auto next = static_cast<std::size_t>(std::clamp(target, 0ll, static_cast<long long>(book->pages.size() - 1)));
    if (next == page) return;
    page = next; busy = true; error.clear();
    generation = reader->request(source, page); dirty = true;
  }
  void resize() {
    int w = 0, h = 0; SDL_GetWindowSize(window, &w, &h);
    ui_scale = std::max(1.f, SDL_GetWindowDisplayScale(window) / std::max(.1f, SDL_GetWindowPixelDensity(window)));
    width = static_cast<float>(std::max(w, 1)) / ui_scale; height = static_cast<float>(std::max(h, 1)) / ui_scale;
    SDL_Rect area{};
    if (!SDL_GetWindowSafeArea(window, &area)) area = {0, 0, w, h};
    safe_x = static_cast<float>(area.x) / ui_scale; safe_y = static_cast<float>(area.y) / ui_scale;
    safe_w = static_cast<float>(area.w) / ui_scale; safe_h = static_cast<float>(area.h) / ui_scale;
    int pixels_w = 0, pixels_h = 0; SDL_GetRenderOutputSize(renderer, &pixels_w, &pixels_h);
    SDL_SetRenderScale(renderer, pixels_w / width, pixels_h / height);
    scroll_target = std::clamp(scroll_target, 0.f, shelf_max());
    scroll = std::clamp(scroll, 0.f, shelf_max());
    clamp_pan(); dirty = true;
  }
  float top() const { return safe_y + 70; }
  float bottom() const { return safe_y + safe_h - 44; }
  float view_height() const { return std::max(1.f, bottom() - top()); }
  float base_scale() const {
    if (!image) return 1;
    const float sx = std::max(1.f, safe_w - 32) / image->width;
    return fit_width ? sx : std::min(sx, view_height() / image->height);
  }
  void clamp_pan() {
    if (!image) return;
    const float scale = base_scale() * zoom;
    const float max_x = std::max(0.f, (image->width * scale - safe_w + 32) * .5f);
    const float max_y = std::max(0.f, image->height * scale - view_height());
    pan_x = std::clamp(pan_x, -max_x, max_x); pan_y = std::clamp(pan_y, 0.f, max_y);
  }
  void zoom_by(float factor) { zoom = std::clamp(zoom * factor, .25f, 8.f); clamp_pan(); dirty = true; }
  void fullscreen() { SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)); dirty = true; }
  bool inside(float x, float y, const SDL_FRect& r) const { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }
  int columns() const { return std::max(1, static_cast<int>((safe_w - 40) / 184)); }
  float card_width() const { return (safe_w - 40) / columns(); }
  float shelf_max() const {
    const auto rows = (library.size() + columns() - 1) / columns();
    return std::max(0.f, static_cast<float>(rows) * 260 - view_height() + 110);
  }
  void select_book(long long index) {
    if (library.empty()) return;
    shelf_selection = static_cast<std::size_t>(std::clamp(index, 0ll, static_cast<long long>(library.size() - 1)));
    const float y = 90 + static_cast<float>(shelf_selection / columns()) * 260;
    if (y < scroll_target) scroll_target = y;
    if (y + 246 > scroll_target + view_height()) scroll_target = y + 246 - view_height();
    scroll_target = std::clamp(scroll_target, 0.f, shelf_max()); dirty = true;
  }
  void click(float x, float y) {
    if (help) { help = false; dirty = true; return; }
    if (y >= safe_y + 14 && y < safe_y + 56) {
      if (x >= safe_x + safe_w - 88) dialog();
#ifndef SDL_PLATFORM_ANDROID
      else if (safe_w > 520 && x >= safe_x + safe_w - 184) dialog(true);
#endif
      else if (x < safe_x + 140) { shelf = true; dirty = true; }
      else if (!shelf && x >= safe_x + 145 && x < safe_x + 225) { fit_width = !fit_width; zoom = 1; pan_x = pan_y = 0; dirty = true; }
      else if (!shelf && safe_w > 430 && x >= safe_x + 230 && x < safe_x + 280) fullscreen();
      return;
    }
    if (shelf) {
      if (library.empty()) { dialog(); return; }
      const float origin = top() + 90 - scroll;
      for (std::size_t i = 0; i < library.size(); ++i) {
        SDL_FRect rect{safe_x + 20 + static_cast<float>(i % columns()) * card_width(), origin + static_cast<float>(i / columns()) * 260, card_width() - 12, 246};
        if (inside(x, y, rect) && y > top() && y < bottom()) { open(library[i]); break; }
      }
    } else if (y >= top() && y < bottom()) {
      if (x < safe_x + safe_w * .25f) navigate(static_cast<long long>(page) - 1);
      else if (x > safe_x + safe_w * .75f) navigate(static_cast<long long>(page) + 1);
    }
  }
  void keydown(const SDL_KeyboardEvent& event) {
    if ((event.mod & SDL_KMOD_CTRL) && event.key == SDLK_O) { if (!event.repeat) dialog(); return; }
    if (event.key == SDLK_ESCAPE || event.key == SDLK_AC_BACK) {
      if (help) help = false; else shelf = true;
      dirty = true; return;
    }
    if (event.key == SDLK_F1) { help = !help; dirty = true; return; }
    if (event.key == SDLK_F11) { if (!event.repeat) fullscreen(); return; }
    if (shelf) {
      if (event.key == SDLK_O && !event.repeat) dialog();
      if (event.key == SDLK_RETURN && !event.repeat) {
        if (library.empty()) dialog(); else open(library[std::min(shelf_selection, library.size() - 1)]);
      }
      if (event.key == SDLK_DOWN) select_book(static_cast<long long>(shelf_selection) + columns());
      if (event.key == SDLK_UP) select_book(static_cast<long long>(shelf_selection) - columns());
      if (event.key == SDLK_RIGHT) select_book(static_cast<long long>(shelf_selection) + 1);
      if (event.key == SDLK_LEFT) select_book(static_cast<long long>(shelf_selection) - 1);
      if (event.key == SDLK_HOME) select_book(0);
      if (event.key == SDLK_END) select_book(static_cast<long long>(library.size()) - 1);
      if (event.key == SDLK_PAGEDOWN) scroll_target = std::min(shelf_max(), scroll_target + view_height());
      if (event.key == SDLK_PAGEUP) scroll_target = std::max(0.f, scroll_target - view_height());
      dirty = true; return;
    }
    switch (event.key) {
      case SDLK_RIGHT: case SDLK_PAGEDOWN: case SDLK_SPACE: navigate(static_cast<long long>(page) + 1); break;
      case SDLK_LEFT: case SDLK_PAGEUP: navigate(static_cast<long long>(page) - 1); break;
      case SDLK_HOME: navigate(0); break;
      case SDLK_END: if (book) navigate(static_cast<long long>(book->pages.size()) - 1); break;
      case SDLK_UP: pan_y -= 70; clamp_pan(); break;
      case SDLK_DOWN: pan_y += 70; clamp_pan(); break;
      case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS: zoom_by(1.2f); break;
      case SDLK_MINUS: case SDLK_KP_MINUS: zoom_by(1 / 1.2f); break;
      case SDLK_0: zoom = 1; pan_x = pan_y = 0; break;
      case SDLK_W: fit_width = !fit_width; zoom = 1; pan_x = pan_y = 0; break;
      default: break;
    }
    dirty = true;
  }
  SDL_AppResult event(SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      event.button.x /= ui_scale; event.button.y /= ui_scale;
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
      event.motion.x /= ui_scale; event.motion.y /= ui_scale;
      event.motion.xrel /= ui_scale; event.motion.yrel /= ui_scale;
    }
    switch (event.type) {
      case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
      case SDL_EVENT_KEY_DOWN: keydown(event.key); break;
      case SDL_EVENT_DROP_FILE: if (event.drop.data) open(event.drop.data); break;
      case SDL_EVENT_WINDOW_RESIZED: case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED: resize(); break;
      case SDL_EVENT_WINDOW_EXPOSED: dirty = true; break;
      case SDL_EVENT_WINDOW_FOCUS_LOST: dragging = has_first = has_second = pinching = false; break;
      case SDL_EVENT_RENDER_DEVICE_RESET: texture.reset(); thumbnails.clear(); text = std::make_unique<Text>(renderer); upload(); dirty = true; break;
      case SDL_EVENT_WILL_ENTER_BACKGROUND: background = true; break;
      case SDL_EVENT_DID_ENTER_FOREGROUND: background = false; resize(); break;
      case SDL_EVENT_LOW_MEMORY: thumbnails.clear(); reader->trim_cache(); covers->trim_cache(); dirty = true; break;
      case SDL_EVENT_MOUSE_WHEEL:
        if (SDL_GetModState() & SDL_KMOD_CTRL) zoom_by(std::pow(1.12f, event.wheel.y));
        else if (shelf) scroll_target = std::clamp(scroll_target - event.wheel.y * 80, 0.f, shelf_max());
        else { pan_y -= event.wheel.y * 70; clamp_pan(); }
        dirty = true; break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_LEFT) { dragging = true; touch_start_x = touch_x = event.button.x; touch_start_y = touch_y = event.button.y; }
        break;
      case SDL_EVENT_MOUSE_MOTION:
        if (dragging) {
          if (shelf) scroll_target = std::clamp(scroll_target - event.motion.yrel, 0.f, shelf_max());
          else { pan_x += event.motion.xrel; pan_y -= event.motion.yrel; clamp_pan(); }
          touch_x = event.motion.x; touch_y = event.motion.y; dirty = true;
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (dragging && event.button.button == SDL_BUTTON_LEFT) {
          dragging = false;
          if (std::hypot(event.button.x - touch_start_x, event.button.y - touch_start_y) < 8) click(event.button.x, event.button.y);
        }
        break;
      case SDL_EVENT_FINGER_DOWN:
        if (!has_first) { has_first = true; first_finger = event.tfinger.fingerID; touch_start_x = touch_x = event.tfinger.x * width; touch_start_y = touch_y = event.tfinger.y * height; pinching = false; }
        else if (!has_second) { has_second = true; second_finger = event.tfinger.fingerID; second_x = event.tfinger.x * width; second_y = event.tfinger.y * height; pinch_distance = std::hypot(second_x - touch_x, second_y - touch_y); pinching = true; }
        break;
      case SDL_EVENT_FINGER_MOTION: {
        const float x = event.tfinger.x * width, y = event.tfinger.y * height;
        if (has_second) {
          if (event.tfinger.fingerID == first_finger) { touch_x = x; touch_y = y; }
          else if (event.tfinger.fingerID == second_finger) { second_x = x; second_y = y; }
          const float distance = std::hypot(second_x - touch_x, second_y - touch_y);
          if (!shelf && pinch_distance > 1) zoom_by(distance / pinch_distance);
          pinch_distance = distance;
        } else if (event.tfinger.fingerID == first_finger) {
          if (shelf) scroll_target = std::clamp(scroll_target - (y - touch_y), 0.f, shelf_max());
          else { pan_x += x - touch_x; pan_y -= y - touch_y; clamp_pan(); }
          touch_x = x; touch_y = y;
        }
        dirty = true; break;
      }
      case SDL_EVENT_FINGER_UP:
        if (event.tfinger.fingerID == second_finger && has_second) has_second = false;
        else if (event.tfinger.fingerID == first_finger && has_first) {
          has_first = false;
          const float dx = event.tfinger.x * width - touch_start_x, dy = event.tfinger.y * height - touch_start_y;
          if (!pinching) {
            if (std::hypot(dx, dy) < 12) click(touch_start_x, touch_start_y);
            else if (!shelf && zoom <= 1.01f && std::abs(dx) > 70 && std::abs(dx) > std::abs(dy) * 1.5f)
              navigate(static_cast<long long>(page) + (dx < 0 ? 1 : -1));
          }
          has_second = false;
        }
        break;
      default: break;
    }
    return SDL_APP_CONTINUE;
  }
  void upload() {
    texture.reset();
    if (!image) return;
    texture.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image->width, image->height));
    if (!texture || !SDL_UpdateTexture(texture.get(), nullptr, image->pixels.data(), image->width * 4)) {
      error = SDL_GetError(); texture.reset(); return;
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
        chosen = std::move(inbox->selected); inbox->selected.reset(); folder = inbox->folder;
        if (!inbox->error.empty()) { error = std::move(inbox->error); inbox->error.clear(); dirty = true; }
        if (inbox->library) { library = std::move(*inbox->library); inbox->library.reset(); scroll = scroll_target = 0; shelf_selection = 0; thumbnails.clear(); cover_source.clear(); cover_generation = 0; dirty = true; }
      }
      if (chosen) {
        if (folder) {
          start_scan(*chosen);
        } else open(*chosen);
      }
    }
    if (auto result = reader->poll(); result && result->generation == generation) {
      busy = false; book = result->book; page = result->page; error = result->error;
      image = result->image; upload(); pan_x = pan_y = 0; clamp_pan();
      if (image) {
        if (!titles.contains(source) || titles[source] != book->title) {
          titles[source] = book->title; settings->save_title(key, book->title);
        }
        if (!settings->save_position(key, page)) notice = "Reading position could not be saved.";
        if (!settings->remember(source)) notice = "Recent books could not be saved.";
        if (std::find(library.begin(), library.end(), source) == library.end()) library.insert(library.begin(), source);
        const auto window_title = title(source) + "  |  " + std::to_string(page + 1) + " / " + std::to_string(book->pages.size()) + "  |  oneam";
        SDL_SetWindowTitle(window, window_title.c_str());
        SDL_Log("Page %zu: %.2f ms%s", page + 1, result->milliseconds, result->cached ? " (cache)" : "");
      }
      dirty = true;
    }
    if (auto result = covers->poll(); result && result->generation == cover_generation) {
      if (result->book) titles[cover_source] = result->book->title;
      Texture thumbnail;
      if (result->image) {
        const auto& im = *result->image;
        const float scale = std::min(200.f / im.width, 250.f / im.height);
        const int w = std::max(1, static_cast<int>(im.width * scale)), h = std::max(1, static_cast<int>(im.height * scale));
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4);
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
          const auto src = (static_cast<std::size_t>(y * im.height / h) * im.width + x * im.width / w) * 4;
          const auto dst = (static_cast<std::size_t>(y) * w + x) * 4;
          std::copy_n(im.pixels.data() + src, 4, pixels.data() + dst);
        }
        thumbnail.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h));
        if (thumbnail) { SDL_UpdateTexture(thumbnail.get(), nullptr, pixels.data(), w * 4); SDL_SetTextureScaleMode(thumbnail.get(), SDL_SCALEMODE_LINEAR); }
      }
      if (thumbnails.size() >= 48) thumbnails.clear();
      thumbnails.insert_or_assign(cover_source, std::move(thumbnail)); cover_source.clear(); dirty = true;
    }
    const auto now = SDL_GetTicks();
    const auto dt = std::min(.05f, (now - last_tick) / 1000.f); last_tick = now;
    if (std::abs(scroll_target - scroll) > .1f) { scroll += (scroll_target - scroll) * std::min(1.f, dt * 18); dirty = true; }
    else scroll = scroll_target;
  }
  void fill(SDL_FRect r, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a); SDL_RenderFillRect(renderer, &r);
  }
  void button(std::string_view label, SDL_FRect r, bool highlighted = false) {
    fill(r, highlighted ? SDL_Color{47, 74, 68, 255} : SDL_Color{39, 44, 54, 255});
    text->draw(label, r.x + 12, r.y + 10, 19, highlighted ? accent : ink, r.w - 16);
  }
  void render() {
    SDL_SetRenderDrawColor(renderer, 19, 22, 28, 255); SDL_RenderClear(renderer);
    fill({safe_x, safe_y, safe_w, 70}, {25, 29, 37, 255});
    text->draw("oneam", safe_x + 22, safe_y + 19, 30, ink);
    if (!shelf) {
      button(fit_width ? "Width" : "Fit", {safe_x + 145, safe_y + 14, 74, 42}, fit_width);
      if (safe_w > 430) button("[ ]", {safe_x + 230, safe_y + 14, 48, 42});
    }
#ifndef SDL_PLATFORM_ANDROID
    if (safe_w > 520) button("Folder", {safe_x + safe_w - 184, safe_y + 14, 88, 42});
#endif
    button("Open", {safe_x + safe_w - 88, safe_y + 14, 68, 42}, true);
    SDL_Rect clip{static_cast<int>(safe_x), static_cast<int>(top()), static_cast<int>(safe_w), static_cast<int>(view_height())};
    SDL_SetRenderClipRect(renderer, &clip);
    if (shelf) render_shelf();
    else if (texture && image) {
      const float scale = base_scale() * zoom, w = image->width * scale, h = image->height * scale;
      SDL_FRect destination{safe_x + (safe_w - w) * .5f + pan_x, top() + std::max(0.f, (view_height() - h) * .5f) - pan_y, w, h};
      SDL_RenderTexture(renderer, texture.get(), nullptr, &destination);
    }
    if (!shelf && (busy || !error.empty())) {
      fill({safe_x + 20, top() + 22, safe_w - 40, 78}, {35, 40, 50, 245});
      text->draw(busy ? "Opening your page..." : "This book needs attention", safe_x + 36, top() + 32, 23, ink, safe_w - 72);
      text->draw(busy ? "You can keep navigating while it loads." : error, safe_x + 36, top() + 63, 18, muted, safe_w - 72);
    }
    SDL_SetRenderClipRect(renderer, nullptr);
    fill({safe_x, bottom(), safe_w, 44}, {25, 29, 37, 255});
    std::string status = shelf ? std::to_string(library.size()) + " books  /  Open a book to begin" :
      book ? std::to_string(page + 1) + " / " + std::to_string(book->pages.size()) + "   " + title(source) : title(source);
    if (!notice.empty()) status = notice;
    text->draw(status, safe_x + 22, bottom() + 13, 17, muted, safe_w - 44);
    if (help) {
      fill({safe_x + 20, top() + 20, safe_w - 40, 310}, {35, 40, 50, 255});
      const std::array lines{"Reader controls", "Open: Ctrl+O or Open   /   drag a comic onto the window", "Pages: Left / Right, Page Up / Down, Space", "First / last: Home / End   /   Library: Escape", "Scroll: wheel or drag   /   Zoom: Ctrl+wheel or +/-", "Fit width: W   /   Reset zoom: 0   /   Fullscreen: F11", "Touch: swipe pages, pinch to zoom, drag to scroll", "Tap anywhere to close this help."};
      float y = top() + 40;
      for (const char* line : lines) { text->draw(line, safe_x + 40, y, 20, ink, safe_w - 80); y += 32; }
    }
    if (smoke_frames == 1 && !smoke_output.empty()) {
      SDL_Surface* snapshot = SDL_RenderReadPixels(renderer, nullptr);
      if (snapshot) { SDL_SaveBMP(snapshot, smoke_output.c_str()); SDL_DestroySurface(snapshot); }
    }
    SDL_RenderPresent(renderer); dirty = false;
  }
  void render_shelf() {
    const float origin = top() + 90 - scroll;
    text->draw("Your reading shelf", safe_x + 22, top() + 20 - scroll, 30, ink);
#ifdef SDL_PLATFORM_ANDROID
    text->draw("Tap a cover to continue reading.", safe_x + 22, top() + 57 - scroll, 18, muted, safe_w - 44);
#else
    text->draw(inbox->scanning ? "Looking for books..." : "Pick up where you left off.  F1 for controls.", safe_x + 22, top() + 57 - scroll, 18, muted, safe_w - 44);
#endif
    if (library.empty()) {
      text->draw("Your next good read.", safe_x + 28, top() + 150, 28, ink, safe_w - 56);
      text->draw("Choose Open to pick a comic.", safe_x + 28, top() + 198, 19, muted, safe_w - 56);
      if (!error.empty()) text->draw(error, safe_x + 28, top() + 244, 18, accent, safe_w - 56);
    }
    for (std::size_t i = 0; i < library.size(); ++i) {
      const float x = safe_x + 20 + static_cast<float>(i % columns()) * card_width();
      const float y = origin + static_cast<float>(i / columns()) * 260;
      if (y + 246 < top() || y > bottom()) continue;
      const float w = card_width() - 12;
      fill({x, y, w, 246}, i == shelf_selection ? SDL_Color{47, 66, 64, 255} : SDL_Color{30, 35, 44, 255});
      if (auto found = thumbnails.find(library[i]); found != thumbnails.end() && found->second) {
        float tw = 0, th = 0; SDL_GetTextureSize(found->second.get(), &tw, &th);
        const float scale = std::min((w - 20) / tw, 192.f / th);
        SDL_FRect destination{x + (w - tw * scale) * .5f, y + 10 + (192 - th * scale) * .5f, tw * scale, th * scale};
        SDL_RenderTexture(renderer, found->second.get(), nullptr, &destination);
      } else {
        fill({x + 12, y + 12, w - 24, 188}, {43, 53, 65, 255});
        text->draw("oneam", x + 26, y + 90, 27, muted, w - 40);
        if (cover_source.empty() && !thumbnails.contains(library[i]) && !busy) {
          cover_source = library[i]; cover_generation = covers->request(cover_source, 0);
        }
      }
      text->draw(title(library[i]), x + 10, y + 212, 17, ink, w - 20);
    }
  }
  SDL_AppResult iterate() {
    tick();
    if (background) { SDL_Delay(40); return SDL_APP_CONTINUE; }
    if (smoke_frames == 1) dirty = true;
    if (dirty) render();
    else SDL_Delay(12);
    if (smoke_frames > 0 && --smoke_frames == 0) {
      if (!startup_source.empty() && (!image || busy || !error.empty())) return SDL_APP_FAILURE;
      return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
  }
};
}
SDL_AppResult SDL_AppInit(void** state, int argc, char** argv) {
  try {
    auto app = std::make_unique<App>();
    if (!app->init(argc, argv)) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "oneam", SDL_GetError(), nullptr); return SDL_APP_FAILURE; }
    app->resize(); *state = app.release(); return SDL_APP_CONTINUE;
  } catch (const std::exception& ex) { SDL_Log("Startup failed: %s", ex.what()); return SDL_APP_FAILURE; }
}
SDL_AppResult SDL_AppEvent(void* state, SDL_Event* event) {
  try { return static_cast<App*>(state)->event(*event); }
  catch (const std::exception& ex) { SDL_Log("Event failed: %s", ex.what()); return SDL_APP_FAILURE; }
}
SDL_AppResult SDL_AppIterate(void* state) {
  try { return static_cast<App*>(state)->iterate(); }
  catch (const std::exception& ex) { SDL_Log("Reader failed: %s", ex.what()); return SDL_APP_FAILURE; }
}
void SDL_AppQuit(void* state, SDL_AppResult) { delete static_cast<App*>(state); SDL_Quit(); }
