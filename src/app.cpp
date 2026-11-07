#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <thread>
#include <unordered_map>
#include "controls.hpp"
#include "icons.hpp"
#include "page_texture.hpp"
#include "reader.hpp"
#include "reading.hpp"
#include "scaler.hpp"
#include "text.hpp"
#include "web_bridge.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace
{
using namespace oneam;
constexpr SDL_Color ink{233, 234, 237, 255}, muted{157, 164, 179, 255}, accent{137, 194, 176, 255};
#ifdef __EMSCRIPTEN__
struct App;
std::atomic<App*> browser_app{nullptr};
std::atomic<int> browser_page{0}, browser_rotation{0};
std::atomic<double> browser_zoom{1};
#endif
struct TextureDeleter
{
   void operator()(SDL_Texture* texture) const
   {
      SDL_DestroyTexture(texture);
   }
};
using Texture = std::unique_ptr<SDL_Texture, TextureDeleter>;
struct Inbox
{
   std::mutex mutex;
   std::optional<std::string> selected;
   std::optional<std::vector<std::string>> library;
   std::optional<std::pair<int, int>> window_size;
   std::string error;
   std::atomic<bool> alive{true}, scanning{false};
   bool dialog = false, folder = false;
};
struct Dialog
{
   std::shared_ptr<Inbox> inbox;
   bool folder;
};
void SDLCALL selected(void* data, const char* const* paths, int)
{
   std::unique_ptr<Dialog> context(static_cast<Dialog*>(data));
   auto& inbox = *context->inbox;
   std::lock_guard lock(inbox.mutex);
   inbox.dialog = false;
   if (!inbox.alive)
      return;
   if (!paths)
      inbox.error = SDL_GetError();
   else if (paths[0])
   {
      inbox.selected = paths[0];
      inbox.folder = context->folder;
   }
}
std::string basename(const std::string& source)
{
   auto end = source.find_last_of("/\\");
   return source.substr(end == source.npos ? 0 : end + 1);
}
std::string utf8(const std::filesystem::path& path)
{
   const auto value = path.u8string();
   return {reinterpret_cast<const char*>(value.data()), value.size()};
}
struct Scan
{
   std::shared_ptr<Inbox> inbox;
   std::string path;
};
int scan_folder(void* pointer)
{
   std::unique_ptr<Scan> job(static_cast<Scan*>(pointer));
   std::vector<std::string> books;
   std::error_code ec;
   for (std::filesystem::directory_iterator it(std::filesystem::path(std::u8string(job->path.begin(), job->path.end())), ec), end;
        !ec && it != end && job->inbox->alive;
        it.increment(ec))
   {
      if (it->is_regular_file(ec) && is_book(utf8(it->path())))
         books.push_back(utf8(it->path()));
      if (books.size() >= 10000)
         break;
   }
   std::sort(books.begin(), books.end(), [](const auto& a, const auto& b) { return natural_less(a, b); });
   std::lock_guard lock(job->inbox->mutex);
   job->inbox->library = std::move(books);
   if (ec)
      job->inbox->error = ec.message();
   job->inbox->scanning = false;
   return 0;
}
struct App
{
   SDL_Window* window = nullptr;
   SDL_Renderer* renderer = nullptr;
   SDL_Cursor* hand_cursor = nullptr;
   std::unique_ptr<Text> text;
   std::unique_ptr<Settings> settings;
   std::unique_ptr<Reader> reader, covers;
   std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
   SDL_Thread* scanner = nullptr;
   std::shared_ptr<const Book> book;
   std::shared_ptr<const Image> image;
   PageTexture primary, following, preceding;
   std::unique_ptr<Reader> ahead, behind, page_previews;
   std::shared_ptr<const Image> next_image, previous_image;
   std::uint64_t ahead_generation = 0, behind_generation = 0, preview_generation = 0;
   std::optional<std::size_t> preview_pending;
   std::unordered_map<std::size_t, Texture> page_thumbnails;
   bool controls_hidden = false;
   ReadingMode layout = ReadingMode::Single;
   bool manga = false, preferred_manga = false, cover_alone = true, trim = false, auto_next = false;
   int rotation = 0;
   Tone tone;
   ViewSettings default_view;
   bool restored_view = false, view_dirty = false;
   std::uint64_t view_save_due = 0;
   std::vector<std::size_t> bookmarks;
   float continuous_carry = 0;
   float pending_scroll = 0;
   bool continuous_transition = false;
   enum class Panel
   {
      None,
      Pages,
      Bookmarks,
      Adjust,
      Search,
      Info
   };
   Panel panel = Panel::None;
   float panel_scroll = 0;
   std::string entry, search_query;
   bool editing = false, replace_entry = false;
   int library_sort = 0, library_filter = 0;
   std::vector<std::string> collection, book_order;
   std::unordered_map<std::string, Settings::Progress> progress;
   std::unordered_map<std::string, std::string> fingerprints;
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
   float render_scale = 1, input_x = 1, input_y = 1;
   std::uint64_t last_tick = 0;
   std::uint64_t redraw_until = 0, touch_started = 0;
   int smoke_frames = 0;
   std::vector<std::string> smoke_actions;
   std::size_t smoke_action_index = 0;
   std::uint64_t smoke_action_due = 0;
   std::optional<std::size_t> smoke_expect_page;
   std::optional<std::size_t> smoke_page;
   std::string smoke_output;
   std::string startup_source;
   std::string smoke_library;
   std::string prefs;

   ~App()
   {
      persist_view();
      inbox->alive = false;
      if (scanner)
         SDL_WaitThread(scanner, nullptr);
      covers.reset();
      reader.reset();
      ahead.reset();
      behind.reset();
      page_previews.reset();
      primary.clear();
      following.clear();
      preceding.clear();
      page_thumbnails.clear();
      thumbnails.clear();
      text.reset();
      SDL_DestroyCursor(hand_cursor);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
   }
   bool init(int argc, char** argv)
   {
      SDL_SetAppMetadata("oneam", "2.0.0", "com.varnholt.oneam");
      SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
      SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
      if (!SDL_Init(SDL_INIT_VIDEO))
         return false;
      if (!SDL_CreateWindowAndRenderer("oneam", 1100, 800, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer))
         return false;
      SDL_SetRenderVSync(renderer, 1);
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      hand_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
      SDL_SetWindowMinimumSize(window, 320, 280);
      SDL_SetRenderLogicalPresentation(renderer, 1100, 800, SDL_LOGICAL_PRESENTATION_DISABLED);
      char* preference_path = SDL_GetPrefPath("varnholt", "oneam");
      if (preference_path)
      {
         prefs = preference_path;
         SDL_free(preference_path);
      }
      for (int i = 1; i < argc; ++i)
         if (std::string_view(argv[i]) == "--smoke")
         {
            prefs += "smoke/" + (i + 1 < argc ? book_key(argv[i + 1]) : "default") + "/";
            break;
         }
      if (prefs.empty())
         return SDL_SetError("Cannot locate writable application settings.");
      settings = std::make_unique<Settings>(prefs);
      reader = std::make_unique<Reader>();
      covers = std::make_unique<Reader>(false, 0);
      ahead = std::make_unique<Reader>(false, 32 * 1024 * 1024);
      behind = std::make_unique<Reader>(false, 32 * 1024 * 1024);
      page_previews = std::make_unique<Reader>(false, 8 * 1024 * 1024);
      text = std::make_unique<Text>(renderer);
      if (!text->valid())
         return SDL_SetError("Missing assets/NotoSans.ttf. Rebuild or reinstall oneam.");
      library = settings->recent();
#ifdef __EMSCRIPTEN__
      std::erase_if(
         library,
         [](const auto& file)
         {
            std::error_code ec;
            return !std::filesystem::exists(file, ec);
         }
      );
#endif
      collection = library;
      order_books();
      load_preferences();
      for (const auto& path : library)
      {
         auto title = settings->title(book_key(path));
         if (!title.empty())
            titles[path] = std::move(title);
      }
#if !defined(SDL_PLATFORM_ANDROID) && !defined(__EMSCRIPTEN__)
      if (const auto folder = settings->folder(); !folder.empty())
         start_scan(folder);
#endif
      for (int i = 1; i < argc; ++i)
      {
         const std::string arg = argv[i];
         if (arg == "--smoke" && i + 1 < argc)
         {
            smoke_output = argv[++i];
            smoke_frames = 180;
         }
         else if (arg == "--smoke-menu")
            smoke_menu = true;
         else if (arg == "--smoke-reading")
            controls_hidden = true;
         else if (arg == "--smoke-page" && i + 1 < argc)
            smoke_page = static_cast<std::size_t>(std::max(1, std::stoi(argv[++i])) - 1);
         else if (arg == "--smoke-help")
            smoke_help = true;
         else if (arg == "--smoke-spread")
            layout = ReadingMode::Spread;
         else if (arg == "--smoke-continuous")
            layout = ReadingMode::Continuous;
         else if (arg == "--smoke-trim")
            trim = true;
         else if (arg == "--smoke-manga")
            manga = preferred_manga = true;
         else if (arg == "--smoke-pages")
            panel = Panel::Pages;
         else if (arg == "--smoke-actions" && i + 1 < argc)
         {
            std::istringstream actions(argv[++i]);
            for (std::string action; std::getline(actions, action, ',');)
               smoke_actions.push_back(action);
         }
         else if (arg == "--smoke-library" && i + 1 < argc)
            smoke_library = argv[++i];
         else if (arg == "--smoke-expect-page" && i + 1 < argc)
            smoke_expect_page = std::size_t(std::max(1, std::stoi(argv[++i])) - 1);
         else if (arg == "--size" && i + 2 < argc)
         {
            const int w = std::stoi(argv[++i]);
            const int h = std::stoi(argv[++i]);
            SDL_SetWindowSize(window, w, h);
         }
         else
            startup_source = arg;
      }
      if (smoke_frames && !smoke_library.empty())
         start_scan(smoke_library);
      smoke_frames = smoke_frames ? std::max(smoke_frames, int(smoke_actions.size()) * 30) : 0;
      if (smoke_frames)
         default_view = current_view();
      if (!startup_source.empty())
         open(startup_source);
      for (int i = 1; i < argc; ++i)
         if (std::string_view(argv[i]) == "--smoke-pages")
            open_panel(Panel::Pages);
      menu_open = smoke_menu;
      help = smoke_help;
      last_tick = SDL_GetTicks();
      return true;
   }
   void dialog(bool folder = false)
   {
#ifdef __EMSCRIPTEN__
      if (!folder)
         web_choose_book();
      return;
#endif
      {
         std::lock_guard lock(inbox->mutex);
         if (inbox->dialog)
            return;
         inbox->dialog = true;
      }
      static const SDL_DialogFileFilter filters[] = {{"Comic books", "cbz;cbr;cb7;cbt;zip;rar;7z;tar"}, {"All files", "*"}};
      auto* context = new Dialog{inbox, folder};
      if (folder)
         SDL_ShowOpenFolderDialog(selected, context, window, nullptr, false);
      else
         SDL_ShowOpenFileDialog(selected, context, window, filters, 2, nullptr, false);
   }
   void open(std::string path)
   {
      if (path.empty())
         return;
      persist_view();
      menu_open = help = false;
      close_panel();
      source = path;
      manga = preferred_manga;
      key = book_key(source);
      const auto saved = settings->view(key);
      restored_view = saved.has_value();
      apply_view(saved.value_or(default_view));
      page = smoke_frames && smoke_page ? *smoke_page : settings->position(key);
      if (layout == ReadingMode::Spread)
         page = spread_start(page, cover_alone);
      book.reset();
      image.reset();
      primary.clear();
      following.clear();
      preceding.clear();
      next_image.reset();
      previous_image.reset();
      ahead_generation = behind_generation = 0;
      page_thumbnails.clear();
      preview_pending.reset();
      bookmarks = settings->bookmarks(key);
      continuous_transition = false;
      pending_scroll = 0;
      shelf = false;
      error.clear();
      notice.clear();
      pan_x = pan_y = 0;
      busy = true;
      generation = reader->request(source, page, true);
      dirty = true;
   }
   void start_scan(const std::string& path)
   {
      if (inbox->scanning)
      {
         notice = "A folder is already being scanned.";
         dirty = true;
         return;
      }
      if (scanner)
         SDL_WaitThread(scanner, nullptr);
      inbox->scanning = true;
      shelf = true;
      auto* scan = new Scan{inbox, path};
      scanner = SDL_CreateThread(scan_folder, "oneam-library", scan);
      if (!scanner)
      {
         delete scan;
         inbox->scanning = false;
         error = SDL_GetError();
      }
      else if (!settings->save_folder(path))
         notice = "Library folder could not be saved.";
      dirty = true;
   }
   std::string title(const std::string& path) const
   {
      auto found = titles.find(path);
      return found == titles.end() ? basename(path) : found->second;
   }
   void navigate(long long target)
   {
      if (!book || book->pages.empty())
         return;
      const auto next = static_cast<std::size_t>(std::clamp(target, 0ll, static_cast<long long>(book->pages.size() - 1)));
      if (next == page)
         return;
      if (!continuous_transition)
         pending_scroll = 0;
      page = next;
      if (layout == ReadingMode::Spread)
         page = spread_start(page, cover_alone);
      busy = true;
      error.clear();
      generation = reader->request(source, page);
      request_neighbors();
      dirty = true;
   }
   std::string fingerprint(const std::string& path)
   {
      auto [it, fresh] = fingerprints.try_emplace(path);
      if (fresh)
         it->second = book_key(path);
      return it->second;
   }
   Settings::Progress progress_for(const std::string& path)
   {
      auto [it, fresh] = progress.try_emplace(path);
      if (fresh)
         it->second = settings->progress(fingerprint(path));
      return it->second;
   }
   void order_books()
   {
      book_order = collection;
      std::sort(book_order.begin(), book_order.end(), [](const auto& a, const auto& b) { return natural_less(basename(a), basename(b)); });
   }
   void rebuild_library()
   {
      const auto selected = shelf_selection < library.size() ? library[shelf_selection] : std::string{};
      library.clear();
      for (const auto& path : collection)
      {
         const auto p = progress_for(path);
         if (!search_query.empty() && !text_matches(title(path) + " " + basename(path), search_query))
            continue;
         if ((library_filter == 1 && (p.finished || p.total)) || (library_filter == 2 && (!p.total || p.finished)) ||
             (library_filter == 3 && !p.finished))
            continue;
         library.push_back(path);
      }
      if (library_sort == 1)
         std::stable_sort(library.begin(), library.end(), [&](const auto& a, const auto& b) { return natural_less(title(a), title(b)); });
      else if (library_sort == 2)
         std::stable_sort(
            library.begin(),
            library.end(),
            [&](const auto& a, const auto& b)
            {
               auto pa = progress_for(a), pb = progress_for(b);
               const double aa = pa.finished ? 1. : pa.total ? double(pa.page + 1) / pa.total : 0.;
               const double bb = pb.finished ? 1. : pb.total ? double(pb.page + 1) / pb.total : 0.;
               return aa > bb;
            }
         );
      else
      {
         const auto recent = settings->recent();
         std::stable_sort(
            library.begin(),
            library.end(),
            [&](const auto& a, const auto& b)
            { return std::find(recent.begin(), recent.end(), a) < std::find(recent.begin(), recent.end(), b); }
         );
      }
      const auto found = std::find(library.begin(), library.end(), selected);
      shelf_selection = found == library.end() ? 0 : std::size_t(found - library.begin());
      scroll = scroll_target = 0;
      dirty = true;
   }
   void load_preferences()
   {
      std::istringstream input(settings->preference("reader"));
      int mode = 0;
      if (input >> mode >> manga >> cover_alone >> trim >> auto_next >> tone.brightness >> tone.contrast >> tone.gamma)
      {
         layout = static_cast<ReadingMode>(std::clamp(mode, 0, 2));
         if (!std::isfinite(tone.brightness) || !std::isfinite(tone.contrast) || !std::isfinite(tone.gamma))
            tone = {};
         tone.brightness = std::clamp(tone.brightness, -.5f, .5f);
         tone.contrast = std::clamp(tone.contrast, .5f, 2.f);
         tone.gamma = std::clamp(tone.gamma, .5f, 2.f);
      }
      std::istringstream lib(settings->preference("library"));
      preferred_manga = manga;
      default_view = current_view();
      lib >> library_sort >> library_filter;
      library_sort = std::clamp(library_sort, 0, 2);
      library_filter = std::clamp(library_filter, 0, 3);
      rebuild_library();
   }
   void save_preferences()
   {
      std::ostringstream out;
      out << int(default_view.layout) << ' ' << default_view.manga << ' ' << default_view.cover_alone << ' ' << default_view.trim << ' '
          << auto_next << ' ' << default_view.tone.brightness << ' ' << default_view.tone.contrast << ' ' << default_view.tone.gamma;
      settings->save_preference("reader", out.str());
      settings->save_preference("library", std::to_string(library_sort) + " " + std::to_string(library_filter));
   }
   ViewSettings current_view() const
   {
      return {layout, manga, cover_alone, trim, fit_width, controls_hidden, rotation, zoom, tone};
   }
   void apply_view(const ViewSettings& view)
   {
      layout = view.layout;
      manga = view.manga;
      cover_alone = view.cover_alone;
      trim = view.trim;
      fit_width = view.fit_width;
      controls_hidden = view.controls_hidden;
      rotation = view.rotation;
      zoom = view.zoom;
      tone = view.tone;
   }
   void schedule_view_save()
   {
      if (key.empty() || !book || !image)
         return;
      view_dirty = true;
      view_save_due = SDL_GetTicks() + 300;
   }
   void persist_view()
   {
      if (view_dirty && settings && !key.empty() && book && image)
      {
         if (!settings->save_view(key, current_view()))
            notice = "Reading preferences could not be saved.";
         view_dirty = false;
      }
   }
   std::optional<std::string> next_book() const
   {
      auto found = std::find(book_order.begin(), book_order.end(), source);
      if (found != book_order.end() && ++found != book_order.end())
         return *found;
      return {};
   }
   bool pair_visible() const
   {
      return layout == ReadingMode::Spread && !(cover_alone && page == 0) && book && page + 1 < book->pages.size();
   }
   SDL_FPoint current_size() const
   {
      return image ? page_size(*image, trim, rotation) : SDL_FPoint{1, 1};
   }
   SDL_FPoint neighbor_size(const std::shared_ptr<const Image>& im) const
   {
      return im ? page_size(*im, trim, rotation) : current_size();
   }
   float page_height(const std::shared_ptr<const Image>& im) const
   {
      const auto s = neighbor_size(im);
      return (safe_w - 32) * zoom * s.y / s.x;
   }
   void request_neighbors()
   {
      next_image.reset();
      previous_image.reset();
      following.clear();
      preceding.clear();
      ahead_generation = behind_generation = 0;
      if (!book || layout == ReadingMode::Single)
         return;
      if (page + 1 < book->pages.size())
         ahead_generation = ahead->request(source, page + 1);
      if (layout == ReadingMode::Continuous && page > 0)
         behind_generation = behind->request(source, page - 1);
   }
   void turn(int direction)
   {
      if (!book)
         return;
      const auto shown_end = page + (pair_visible() ? 1 : 0);
      if (direction > 0 && shown_end + 1 >= book->pages.size())
      {
         if (auto_next)
         {
            if (auto next = next_book())
               open(*next);
         }
         return;
      }
      auto target =
         layout == ReadingMode::Spread
            ? spread_next(page, book->pages.size(), cover_alone, direction)
            : std::size_t(std::clamp(static_cast<long long>(page) + direction, 0ll, static_cast<long long>(book->pages.size() - 1)));
      navigate(static_cast<long long>(target));
   }
   void advance_continuous()
   {
      if (shelf || busy || !image || layout != ReadingMode::Continuous || panel != Panel::None || menu_open || help)
         return;
      const auto step = page_height(image) + 12;
      if (next_image && pan_y >= step)
      {
         continuous_carry = pan_y - step;
         continuous_transition = true;
         navigate(static_cast<long long>(page) + 1);
      }
      else if (previous_image && pan_y < 0)
      {
         continuous_carry = page_height(previous_image) + 12 + pan_y;
         continuous_transition = true;
         navigate(static_cast<long long>(page) - 1);
      }
   }
   void resize()
   {
      int w = 0, h = 0;
      SDL_GetWindowSize(window, &w, &h);
      ui_scale = std::max(1.f, SDL_GetWindowDisplayScale(window) / std::max(.1f, SDL_GetWindowPixelDensity(window)));
      int pixels_w = 0, pixels_h = 0;
      SDL_GetRenderOutputSize(renderer, &pixels_w, &pixels_h);
      // One scalar on both axes preserves page proportions even at fractional DPI.
      render_scale = std::max(.1f, SDL_GetWindowPixelDensity(window)) * ui_scale;
      width = std::max(pixels_w, 1) / render_scale;
      height = std::max(pixels_h, 1) / render_scale;
      input_x = width / std::max(w, 1);
      input_y = height / std::max(h, 1);
      SDL_Rect area{};
      if (!SDL_GetWindowSafeArea(window, &area))
         area = {0, 0, w, h};
      safe_x = area.x * input_x;
      safe_y = area.y * input_y;
      safe_w = area.w * input_x;
      safe_h = area.h * input_y;
      SDL_SetRenderScale(renderer, render_scale, render_scale);
      if (panel == Panel::Pages)
      {
         const auto columns = std::max(1, int(feature_viewport().w / 126));
         panel_scroll = std::min(panel_max(), float(page / columns) * 176);
      }
      else
         panel_scroll = std::clamp(panel_scroll, 0.f, panel_max());
      scroll_target = std::clamp(scroll_target, 0.f, shelf_max());
      scroll = std::clamp(scroll, 0.f, shelf_max());
      clamp_pan();
      dirty = true;
      redraw_until = SDL_GetTicks() + 400;
   }
   bool reading_mode() const
   {
      return !shelf && controls_hidden;
   }
   float top() const
   {
      return safe_y + (reading_mode() ? 0 : 72);
   }
   float bottom() const
   {
      return safe_y + safe_h - (reading_mode() ? 0 : 72);
   }
   float view_height() const
   {
      return std::max(1.f, bottom() - top());
   }
   float base_scale() const
   {
      if (!image)
         return 1;
      auto size = current_size();
      if (pair_visible())
      {
         const auto next = neighbor_size(next_image);
         size.x += next.x;
         size.y = std::max(size.y, next.y);
      }
      const float sx = std::max(1.f, safe_w - 32) / size.x;
      return fit_width || layout == ReadingMode::Continuous ? sx : std::min(sx, view_height() / size.y);
   }
   void clamp_pan()
   {
      if (!image)
         return;
      const auto size = current_size();
      const float scale = base_scale() * zoom;
      const auto next = neighbor_size(next_image);
      const float w = (size.x + (pair_visible() ? next.x : 0)) * scale;
      const float h = std::max(size.y, pair_visible() ? next.y : 0.f) * scale;
      pan_x = std::clamp(pan_x, -std::max(0.f, (w - safe_w + 32) * .5f), std::max(0.f, (w - safe_w + 32) * .5f));
      const float low = layout == ReadingMode::Continuous && previous_image ? -page_height(previous_image) - 12 : 0;
      const float high = layout == ReadingMode::Continuous && next_image
                            ? std::max(h + 12, h + 12 + page_height(next_image) - view_height())
                            : std::max(0.f, h - view_height());
      if (layout == ReadingMode::Continuous && !busy && book)
      {
         if (!next_image && page + 1 < book->pages.size() && pan_y > high)
            pending_scroll += pan_y - high;
         if (!previous_image && page > 0 && pan_y < low)
            pending_scroll += pan_y - low;
      }
      pan_y = std::clamp(pan_y, low, high);
   }
   void zoom_by(float factor)
   {
      zoom = std::clamp(zoom * factor, .25f, 8.f);
      clamp_pan();
      schedule_view_save();
      dirty = true;
   }
   void fullscreen()
   {
      SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
      dirty = true;
   }
   bool inside(float x, float y, const SDL_FRect& r) const
   {
      return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
   }
   int columns() const
   {
      return std::max(1, static_cast<int>((safe_w - 40) / 184));
   }
   float card_width() const
   {
      return (safe_w - 40) / columns();
   }
   float shelf_heading_height() const
   {
      return view_height() < 330 ? 104.f : 132.f;
   }
   float card_height() const
   {
      return std::clamp(view_height() - shelf_heading_height() - 12, 120.f, 246.f);
   }
   float row_height() const
   {
      return card_height() + 14;
   }
   float shelf_max() const
   {
      const auto rows = (library.size() + columns() - 1) / columns();
      return std::max(0.f, static_cast<float>(rows) * row_height() - view_height() + shelf_heading_height() + 20);
   }
   void select_book(long long index)
   {
      if (library.empty())
         return;
      shelf_selection = static_cast<std::size_t>(std::clamp(index, 0ll, static_cast<long long>(library.size() - 1)));
      const float y = shelf_heading_height() + static_cast<float>(shelf_selection / columns()) * row_height();
      if (y < scroll_target)
         scroll_target = y;
      if (y + card_height() > scroll_target + view_height())
         scroll_target = y + card_height() - view_height();
      scroll_target = std::clamp(scroll_target, 0.f, shelf_max());
      dirty = true;
   }
   SDL_FRect safe() const
   {
      return {safe_x, safe_y, safe_w, safe_h};
   }
   ui::Control search_control() const
   {
      return {
         ui::Action::Search,
         {safe_x + 22, top() + 48 - scroll, safe_w - 44, 48},
         search_query.empty() ? std::string_view("Search library") : std::string_view(search_query),
         ui::Icon::Search
      };
   }
   ui::Context context() const
   {
      return {
         !shelf,
         shelf ? !library.empty() : bool(book),
         bool(image),
#if defined(SDL_PLATFORM_ANDROID) || defined(__EMSCRIPTEN__)
         false,
#else
         true,
#endif
         book && page > 0,
         book && (page + (pair_visible() ? 2 : 1) < book->pages.size() || (auto_next && next_book())),
         bool(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN),
         fit_width,
         reading_mode(),
         layout == ReadingMode::Spread,
         layout == ReadingMode::Continuous,
         manga,
         cover_alone,
         trim,
         std::find(bookmarks.begin(), bookmarks.end(), page) != bookmarks.end(),
         auto_next,
         next_book().has_value()
      };
   }
   SDL_FRect feature_panel() const
   {
      const float w = std::min(safe_w - 24, 720.f);
      return {safe_x + (safe_w - w) * .5f, safe_y + 12, w, std::max(100.f, safe_h - 24)};
   }
   SDL_FRect feature_viewport() const
   {
      auto r = feature_panel();
      const float header = panel == Panel::Pages || panel == Panel::Search ? 132.f : 76.f;
      return {r.x + 16, r.y + header, r.w - 32, std::max(1.f, r.h - header - 12)};
   }
   std::vector<std::size_t> browser_pages() const
   {
      if (!book)
         return {};
      if (panel == Panel::Bookmarks)
      {
         auto result = bookmarks;
         std::erase_if(result, [&](auto i) { return i >= book->pages.size(); });
         return result;
      }
      std::vector<std::size_t> result(book->pages.size());
      for (std::size_t i = 0; i < result.size(); ++i)
         result[i] = i;
      return result;
   }
   std::vector<std::string> information() const
   {
      if (!book)
         return {};
      const auto& m = book->metadata;
      return {
         book->title,
         "Series: " + m.series,
         "Issue: " + m.number,
         "Writer: " + m.writer,
         "Archive: " + book->format,
         "Pages: " + std::to_string(book->pages.size()),
         "Summary: " + m.summary
      };
   }
   float panel_max() const
   {
      auto r = feature_viewport();
      float content = 0;
      if (panel == Panel::Pages || panel == Panel::Bookmarks)
      {
         const auto cols = std::max(1, int(r.w / 126));
         content = float((browser_pages().size() + cols - 1) / cols) * 176;
      }
      else if (panel == Panel::Adjust)
         content = 7 * 56 + 80;
      else if (panel == Panel::Info)
         content = 7 * 88.f;
      return std::max(0.f, content - r.h);
   }
   std::vector<ui::Control> panel_controls() const
   {
      auto r = feature_panel(), v = feature_viewport();
      std::vector<ui::Control> result{{ui::Action::Close, {r.x + r.w - 60, r.y + 10, 48, 48}, {}, ui::Icon::Close}};
      if (panel == Panel::Pages || panel == Panel::Search)
         result.push_back(
            {ui::Action::Confirm, {r.x + r.w - 116, r.y + 72, 100, 48}, panel == Panel::Pages ? "Go" : "Done", ui::Icon::Right}
         );
      if (panel == Panel::Adjust)
      {
         const std::array actions{
            ui::Action::BrightnessDown,
            ui::Action::BrightnessUp,
            ui::Action::ContrastDown,
            ui::Action::ContrastUp,
            ui::Action::GammaDown,
            ui::Action::GammaUp,
            ui::Action::ResetImage
         };
         for (std::size_t i = 0; i < actions.size(); ++i)
         {
            auto action = actions[i];
            result.push_back(
               {action, {v.x, v.y + 80 + float(i) * 56 - panel_scroll, v.w, 48}, ui::label(action, context()), ui::icon(action, context())}
            );
         }
      }
      return result;
   }
   void edit_entry()
   {
      editing = true;
      replace_entry = true;
      const auto props = SDL_CreateProperties();
      SDL_SetNumberProperty(
         props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, panel == Panel::Pages ? SDL_TEXTINPUT_TYPE_NUMBER : SDL_TEXTINPUT_TYPE_TEXT
      );
      SDL_StartTextInputWithProperties(window, props);
      SDL_DestroyProperties(props);
      SDL_Rect area{
         int(feature_panel().x * ui_scale),
         int((feature_panel().y + 72) * ui_scale),
         int((feature_panel().w - 140) * ui_scale),
         int(48 * ui_scale)
      };
      SDL_SetTextInputArea(window, &area, 0);
   }
   void close_panel()
   {
      panel = Panel::None;
      editing = false;
      SDL_StopTextInput(window);
   }
   void open_panel(Panel value)
   {
      menu_open = help = false;
      panel = value;
      panel_scroll = 0;
      entry = value == Panel::Pages ? std::to_string(page + 1) : value == Panel::Search ? search_query : "";
      if (value == Panel::Pages)
      {
         const auto cols = std::max(1, int(feature_viewport().w / 126));
         panel_scroll = std::min(panel_max(), float(page / cols) * 176);
      }
      editing = false;
      SDL_StopTextInput(window);
      dirty = true;
   }
   void confirm_entry()
   {
      if (panel == Panel::Pages)
      {
         std::size_t value = 0;
         const auto parsed = std::from_chars(entry.data(), entry.data() + entry.size(), value);
         if (parsed.ec != std::errc{} || parsed.ptr != entry.data() + entry.size() || !book || value == 0 || value > book->pages.size())
         {
            notice = "Enter a page between 1 and " + std::to_string(book ? book->pages.size() : 0);
            dirty = true;
            return;
         }
         close_panel();
         navigate(static_cast<long long>(value - 1));
      }
      else if (panel == Panel::Search)
      {
         search_query = entry;
         rebuild_library();
         close_panel();
      }
      dirty = true;
   }
   void feature_click(float x, float y)
   {
      const auto p = feature_panel(), v = feature_viewport();
      auto controls = panel_controls();
      for (std::size_t i = 0; i < controls.size(); ++i)
         if ((i < (panel == Panel::Pages || panel == Panel::Search ? 2u : 1u) || ui::contains(v, x, y)) &&
             ui::contains(controls[i].rect, x, y))
         {
            perform(controls[i].action);
            return;
         }
      if ((panel == Panel::Pages || panel == Panel::Search) && ui::contains({p.x + 16, p.y + 72, p.w - 140, 48}, x, y))
      {
         edit_entry();
         return;
      }
      if (!ui::contains(p, x, y))
      {
         close_panel();
         dirty = true;
         return;
      }
      if ((panel == Panel::Pages || panel == Panel::Bookmarks) && ui::contains(v, x, y))
      {
         const auto cols = std::max(1, int(v.w / 126));
         const auto index = std::size_t((y - v.y + panel_scroll) / 176) * cols + std::size_t((x - v.x) / (v.w / cols));
         const auto pages = browser_pages();
         if (index < pages.size())
         {
            close_panel();
            navigate(static_cast<long long>(pages[index]));
            dirty = true;
         }
      }
   }
   void render_feature_panel()
   {
      fill(safe(), {8, 11, 16, 215});
      const auto p = feature_panel(), v = feature_viewport();
      ui::rounded_rect(renderer, p, 14, {28, 34, 44, 255});
      const auto title = panel == Panel::Pages       ? "Pages"
                         : panel == Panel::Bookmarks ? "Bookmarks"
                         : panel == Panel::Adjust    ? "Image adjustments"
                         : panel == Panel::Search    ? "Search library"
                                                     : "Book information";
      text->draw(title, p.x + 20, p.y + 23, 24, ink, p.w - 100);
      const auto controls = panel_controls();
      control(controls.front());
      if (panel == Panel::Pages || panel == Panel::Search)
      {
         ui::rounded_rect(
            renderer, {p.x + 16, p.y + 72, p.w - 140, 48}, 8, editing ? SDL_Color{47, 67, 70, 255} : SDL_Color{40, 48, 61, 255}
         );
         text->draw(
            entry.empty() ? (panel == Panel::Pages ? "Page number" : "Type a title or filename") : entry,
            p.x + 28,
            p.y + 85,
            18,
            ink,
            p.w - 166
         );
         control(controls[1]);
      }
      SDL_Rect clip{int(v.x), int(v.y), int(v.w), int(v.h)};
      SDL_SetRenderClipRect(renderer, &clip);
      if (panel == Panel::Pages || panel == Panel::Bookmarks)
      {
         auto pages = browser_pages();
         const auto cols = std::max(1, int(v.w / 126));
         const float cell = v.w / cols;
         if (pages.empty())
            text->draw("No bookmarks yet. Choose Bookmark this page in Menu.", v.x, v.y + 10, 18, muted, v.w);
         for (std::size_t i = 0; i < pages.size(); ++i)
         {
            const float x = v.x + float(i % cols) * cell, y = v.y + float(i / cols) * 176 - panel_scroll;
            if (y + 170 < v.y || y >= v.y + v.h)
               continue;
            const auto n = pages[i];
            ui::rounded_rect(
               renderer, {x + 3, y + 3, cell - 6, 166}, 8, n == page ? SDL_Color{48, 76, 67, 255} : SDL_Color{36, 44, 57, 255}
            );
            if (auto found = page_thumbnails.find(n); found != page_thumbnails.end() && found->second)
            {
               float tw = 0, th = 0;
               SDL_GetTextureSize(found->second.get(), &tw, &th);
               const float scale = std::min((cell - 20) / tw, 128.f / th);
               SDL_FRect dest{x + (cell - tw * scale) * .5f, y + 10 + (128 - th * scale) * .5f, tw * scale, th * scale};
               SDL_RenderTexture(renderer, found->second.get(), nullptr, &dest);
            }
            else
            {
               ui::draw_icon(renderer, ui::Icon::Book, {x + (cell - 32) * .5f, y + 50, 32, 32}, muted);
               if (!preview_pending && !page_thumbnails.contains(n))
               {
                  preview_pending = n;
                  preview_generation = page_previews->request(source, n, false, 192);
               }
            }
            auto caption = std::to_string(n + 1);
            text->draw(caption, x + (cell - text->width(caption, 18)) * .5f, y + 143, 18, ink, cell - 12);
            if (std::find(bookmarks.begin(), bookmarks.end(), n) != bookmarks.end())
               ui::draw_icon(renderer, ui::Icon::Bookmark, {x + cell - 25, y + 8, 16, 16}, accent);
         }
      }
      else if (panel == Panel::Adjust)
      {
         const auto values =
            "Brightness " + std::to_string(int(tone.brightness * 100)) + "%   Contrast " + std::to_string(int(tone.contrast * 100)) + "%";
         text->draw(values, v.x, v.y - panel_scroll, 17, muted, v.w);
         text->draw("Gamma " + std::to_string(int(tone.gamma * 100)) + "%", v.x, v.y + 30 - panel_scroll, 17, muted, v.w);
         for (std::size_t i = 1; i < controls.size(); ++i)
            control(controls[i]);
      }
      else if (panel == Panel::Search)
      {
         text->draw("Search matches book titles and filenames.", v.x, v.y + 12, 18, muted, v.w);
         text->draw("Clear the field and choose Done to show all books.", v.x, v.y + 46, 18, muted, v.w);
      }
      else
      {
         float y = v.y - panel_scroll;
         for (const auto& line : information())
         {
            // Wrap metadata using the same measured font as the reader.
            std::istringstream words(line);
            std::string word, row;
            int lines = 0;
            while (words >> word && lines < 3)
            {
               auto candidate = row.empty() ? word : row + " " + word;
               if (!row.empty() && text->width(candidate, 18) > v.w)
               {
                  text->draw(row, v.x, y, 18, ink, v.w);
                  y += 24;
                  ++lines;
                  row = word;
               }
               else
                  row = std::move(candidate);
            }
            if (!row.empty() && lines < 3)
               text->draw(row, v.x, y, 18, ink, v.w);
            y += 88 - lines * 24;
         }
      }
      SDL_SetRenderClipRect(renderer, nullptr);
      scroll_indicator(v, panel_scroll, panel_max(), v.h + panel_max());
   }
   SDL_FRect help_panel() const
   {
      const float w = std::min(safe_w - 24, 560.f), h = std::min(safe_h - 92, 440.f);
      return {safe_x + (safe_w - w) * .5f, safe_y + 80, w, h};
   }
   SDL_FRect help_viewport() const
   {
      auto r = help_panel();
      return {r.x + 20, r.y + 72, r.w - 40, std::max(1.f, r.h - 88)};
   }
   ui::Control help_close() const
   {
      auto r = help_panel();
      return {ui::Action::Close, {r.x + r.w - 60, r.y + 10, 48, 48}, {}, ui::Icon::Close};
   }
   std::vector<std::string> help_lines()
   {
#ifdef SDL_PLATFORM_ANDROID
      const std::array paragraphs{
         "Choose Open to pick a comic.",
         "Use the bottom arrows to turn pages, or swipe across the page.",
         "Pinch to zoom. Drag to pan or scroll.",
         "Tap the page center to hide the bars. Controls brings them back.",
         "Menu contains zoom, first and last page, fit modes and fullscreen.",
         "Choose Library or Android Back to return to your books.",
         "Scroll this panel to read more."
      };
#else
      const std::array paragraphs{
         "Open a book with Open or Ctrl+O. Folder opens your library directory.",
         "Use the bottom arrows or Left / Right to turn pages. Home / End jumps to the first / last page.",
         "Use the mouse wheel or drag to scroll. Ctrl+wheel zooms.",
         "Read or Tab hides the bars. Click the page center or Controls to bring them back.",
         "Menu contains every reader command, including zoom, reset, fit modes and fullscreen.",
         "F11 toggles fullscreen. F1 opens help. Escape closes a panel or returns to the library.",
         "Keyboard shortcuts are optional. Every command is available with the mouse."
      };
#endif
      std::vector<std::string> result;
      const float available = help_viewport().w;
      for (const char* paragraph : paragraphs)
      {
         std::istringstream words(paragraph);
         std::string word, line;
         while (words >> word)
         {
            auto candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && text->width(candidate, 18) > available)
            {
               result.push_back(line);
               line = word;
            }
            else
               line = std::move(candidate);
         }
         if (!line.empty())
            result.push_back(line);
         result.emplace_back();
      }
      return result;
   }
   void scroll_view(float amount)
   {
      if (panel != Panel::None)
         panel_scroll = std::clamp(panel_scroll + amount, 0.f, panel_max());
      else if (help)
         help_scroll = std::clamp(help_scroll + amount, 0.f, std::max(0.f, help_lines().size() * 26.f - help_viewport().h));
      else if (menu_open)
      {
         auto menu_layout = ui::menu(safe(), context(), menu_scroll);
         menu_scroll = std::clamp(menu_scroll + amount, 0.f, menu_layout.max_scroll);
      }
      else if (shelf)
         scroll_target = std::clamp(scroll_target + amount, 0.f, shelf_max());
      else
      {
         pan_y += amount;
         clamp_pan();
      }
      dirty = true;
   }
   void perform(ui::Action action)
   {
      using A = ui::Action;
      const auto view_before = current_view();
      if (!ui::enabled(action, context()))
         return;
      if (action == A::Menu)
      {
         menu_open = !menu_open;
         help = false;
         menu_scroll = 0;
         dirty = true;
         return;
      }
      if (action == A::Help)
      {
         help = !help;
         menu_open = false;
         help_scroll = 0;
         dirty = true;
         return;
      }
      if (action == A::Close)
      {
         close_panel();
         help = menu_open = false;
         dirty = true;
         return;
      }
      if (action == A::Back)
      {
         if (panel != Panel::None)
            close_panel();
         else if (help)
            help = false;
         else if (menu_open)
            menu_open = false;
         else if (reading_mode())
         {
            controls_hidden = false;
            clamp_pan();
            schedule_view_save();
         }
         else
         {
            shelf = true;
            rebuild_library();
         }
         dirty = true;
         return;
      }
      menu_open = false;
      switch (action)
      {
         case A::Confirm:
            confirm_entry();
            break;
         case A::Pages:
            open_panel(Panel::Pages);
            break;
         case A::Bookmarks:
            open_panel(Panel::Bookmarks);
            break;
         case A::Adjust:
            open_panel(Panel::Adjust);
            break;
         case A::Info:
            open_panel(Panel::Info);
            break;
         case A::Search:
            open_panel(Panel::Search);
            edit_entry();
            break;
         case A::Single:
         case A::Spread:
         case A::Continuous:
            layout = action == A::Single ? ReadingMode::Single : action == A::Spread ? ReadingMode::Spread : ReadingMode::Continuous;
            zoom = 1;
            pending_scroll = 0;
            pan_x = pan_y = 0;
            if (layout == ReadingMode::Spread && page != spread_start(page, cover_alone))
               navigate(static_cast<long long>(spread_start(page, cover_alone)));
            else
               request_neighbors();
            break;
         case A::Manga:
            manga = !manga;
            preferred_manga = manga;
            break;
         case A::CoverAlone:
            cover_alone = !cover_alone;
            if (layout == ReadingMode::Spread)
            {
               const auto start = spread_start(page, cover_alone);
               if (start != page)
                  navigate(static_cast<long long>(start));
               else
                  request_neighbors();
            }
            break;
         case A::Trim:
            trim = !trim;
            pan_x = pan_y = 0;
            break;
         case A::Rotate:
            rotation = (rotation + 1) % 4;
            pan_x = pan_y = 0;
            break;
         case A::Bookmark:
            if (auto found = std::find(bookmarks.begin(), bookmarks.end(), page); found != bookmarks.end())
               bookmarks.erase(found);
            else
               bookmarks.push_back(page);
            if (!settings->save_bookmarks(key, bookmarks))
               notice = "Could not save bookmarks.";
            break;
         case A::NextBook:
            if (auto next = next_book())
               open(*next);
            break;
         case A::AutoNext:
            auto_next = !auto_next;
            break;
         case A::BrightnessDown:
            tone.brightness = std::max(-.5f, tone.brightness - .05f);
            break;
         case A::BrightnessUp:
            tone.brightness = std::min(.5f, tone.brightness + .05f);
            break;
         case A::ContrastDown:
            tone.contrast = std::max(.5f, tone.contrast - .1f);
            break;
         case A::ContrastUp:
            tone.contrast = std::min(2.f, tone.contrast + .1f);
            break;
         case A::GammaDown:
            tone.gamma = std::max(.5f, tone.gamma - .1f);
            break;
         case A::GammaUp:
            tone.gamma = std::min(2.f, tone.gamma + .1f);
            break;
         case A::ResetImage:
            tone = {};
            rotation = 0;
            break;
         case A::SortTitle:
         case A::SortRecent:
         case A::SortProgress:
            library_sort = action == A::SortTitle ? 1 : action == A::SortRecent ? 0 : 2;
            rebuild_library();
            break;
         case A::FilterAll:
         case A::FilterUnread:
         case A::FilterReading:
         case A::FilterRead:
            library_filter = action == A::FilterAll ? 0 : action == A::FilterUnread ? 1 : action == A::FilterReading ? 2 : 3;
            rebuild_library();
            break;
         case A::MarkRead:
         case A::MarkUnread:
            if (!library.empty())
            {
               const auto path = library[std::min(shelf_selection, library.size() - 1)];
               auto p = progress_for(path);
               if (action == A::MarkRead)
               {
                  p.total = std::max<std::size_t>(1, p.total);
                  p.finished = true;
               }
               else
                  p = {};
               progress[path] = p;
               settings->save_progress(fingerprint(path), p);
               if (action == A::MarkUnread)
                  settings->save_position(fingerprint(path), 0);
               rebuild_library();
            }
            break;
         case A::ReadingMode:
            controls_hidden = !controls_hidden;
            clamp_pan();
            break;
         case A::Open:
            dialog();
            break;
         case A::Folder:
            dialog(true);
            break;
         case A::Library:
            shelf = true;
            close_panel();
            rebuild_library();
            help = false;
            break;
         case A::Previous:
            turn(-1);
            break;
         case A::Next:
            turn(1);
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
            else
            {
               pan_y -= 70;
               clamp_pan();
            }
            break;
         case A::ScrollDown:
            if (shelf)
               scroll_target = std::min(shelf_max(), scroll_target + 70);
            else
            {
               pan_y += 70;
               clamp_pan();
            }
            break;
         default:
            break;
      }
      clamp_pan();
      if (current_view() != view_before)
         schedule_view_save();
      if ((action >= A::Single && action <= A::Trim) || action == A::AutoNext || (action >= A::SortTitle && action <= A::FilterRead) ||
          (action >= A::BrightnessDown && action <= A::ResetImage))
         save_preferences();
      dirty = true;
   }
   std::optional<ui::Action> control_at(float x, float y) const
   {
      if (panel != Panel::None)
      {
         auto controls = panel_controls();
         if (ui::contains(controls.front().rect, x, y))
            return controls.front().action;
         const auto fixed = panel == Panel::Pages || panel == Panel::Search ? 2u : 1u;
         if (fixed == 2 && ui::contains(controls[1].rect, x, y))
            return controls[1].action;
         if (ui::contains(feature_viewport(), x, y))
            return ui::hit(std::vector<ui::Control>(controls.begin() + fixed, controls.end()), x, y);
         return {};
      }
      if (help)
      {
         if (ui::contains(help_close().rect, x, y))
            return ui::Action::Close;
         return {};
      }
      if (menu_open)
         return ui::hit(ui::menu(safe(), context(), menu_scroll), x, y);
      if (shelf && y >= top() && y < bottom())
      {
         if (auto action = ui::hit(std::vector<ui::Control>{search_control()}, x, y))
            return action;
      }
      if (auto action = ui::hit(ui::toolbar(safe(), context()), x, y))
         return action;
      return ui::hit(ui::navigation(safe(), context()), x, y);
   }
   bool select_at(float x, float y)
   {
      if (y < top() + shelf_heading_height() - scroll || y >= bottom())
         return false;
      const auto col = int((x - safe_x - 20) / card_width());
      const auto row = int((y - top() - shelf_heading_height() + scroll) / row_height());
      if (col < 0 || col >= columns() || row < 0 || x < safe_x + 20)
         return false;
      const auto index = std::size_t(row * columns() + col);
      if (index >= library.size())
         return false;
      shelf_selection = index;
      dirty = true;
      return true;
   }
   void click(float x, float y)
   {
      if (panel != Panel::None)
      {
         feature_click(x, y);
         return;
      }
      if (auto action = control_at(x, y))
      {
         perform(*action);
         return;
      }
      if (help)
      {
         if (!ui::contains(help_panel(), x, y))
            perform(ui::Action::Close);
         return;
      }
      if (menu_open)
      {
         if (!ui::contains(ui::menu(safe(), context(), menu_scroll).panel, x, y))
            perform(ui::Action::Close);
         return;
      }
      if (y < top() || y >= bottom())
         return;
      if (shelf)
      {
         if (library.empty())
         {
            dialog();
            return;
         }
         const float origin = top() + shelf_heading_height() - scroll;
         for (std::size_t i = 0; i < library.size(); ++i)
         {
            SDL_FRect r{
               safe_x + 20 + static_cast<float>(i % columns()) * card_width(),
               origin + static_cast<float>(i / columns()) * row_height(),
               card_width() - 12,
               card_height()
            };
            if (inside(x, y, r))
            {
               shelf_selection = i;
               open(library[i]);
               break;
            }
         }
      }
      else
      {
         if (x < safe_x + safe_w * .25f)
            perform(manga ? ui::Action::Next : ui::Action::Previous);
         else if (x > safe_x + safe_w * .75f)
            perform(manga ? ui::Action::Previous : ui::Action::Next);
         else
            perform(ui::Action::ReadingMode);
      }
   }
   void keydown(const SDL_KeyboardEvent& event)
   {
      if (panel != Panel::None)
      {
         if (event.key == SDLK_ESCAPE || event.key == SDLK_AC_BACK)
         {
            close_panel();
            dirty = true;
         }
         else if (event.key == SDLK_RETURN || event.key == SDLK_KP_ENTER)
            confirm_entry();
         else if (editing && event.key == SDLK_BACKSPACE && !entry.empty())
         {
            if (replace_entry)
            {
               entry.clear();
               replace_entry = false;
               dirty = true;
               return;
            }
            auto n = entry.size() - 1;
            while (n && (static_cast<unsigned char>(entry[n]) & 0xc0) == 0x80)
               --n;
            entry.resize(n);
            dirty = true;
         }
         else if (editing && (event.mod & SDL_KMOD_CTRL) && event.key == SDLK_A)
         {
            entry.clear();
            dirty = true;
         }
         else if (event.key == SDLK_DOWN || event.key == SDLK_PAGEDOWN)
            scroll_view(176);
         else if (event.key == SDLK_UP || event.key == SDLK_PAGEUP)
            scroll_view(-176);
         return;
      }
      if ((help || menu_open) && (event.key == SDLK_UP || event.key == SDLK_DOWN || event.key == SDLK_PAGEUP || event.key == SDLK_PAGEDOWN))
      {
         scroll_view((event.key == SDLK_UP || event.key == SDLK_PAGEUP) ? -70.f : 70.f);
         return;
      }
      if (shelf && (event.mod & SDL_KMOD_CTRL) && event.key == SDLK_F)
      {
         perform(ui::Action::Search);
         return;
      }
      if (auto action = ui::keyboard(event.key, event.mod, !shelf))
      {
         if (manga && (event.key == SDLK_LEFT || event.key == SDLK_RIGHT))
            action = *action == ui::Action::Next ? ui::Action::Previous : ui::Action::Next;
         if (event.repeat && (*action == ui::Action::Open || *action == ui::Action::Help || *action == ui::Action::Fullscreen ||
                              *action == ui::Action::ReadingMode || *action == ui::Action::Bookmark || *action == ui::Action::Rotate ||
                              *action == ui::Action::Back))
            return;
         perform(*action);
         return;
      }
      if (!shelf)
         return;
      if (event.key == SDLK_O && !event.repeat)
         perform(ui::Action::Open);
      if (event.key == SDLK_RETURN && !event.repeat)
      {
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
   SDL_AppResult event(SDL_Event& event)
   {
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)
      {
         event.button.x *= input_x;
         event.button.y *= input_y;
      }
      else if (event.type == SDL_EVENT_MOUSE_MOTION)
      {
         event.motion.x *= input_x;
         event.motion.y *= input_y;
         event.motion.xrel *= input_x;
         event.motion.yrel *= input_y;
      }
      switch (event.type)
      {
         case SDL_EVENT_TEXT_INPUT:
            if (editing && (panel == Panel::Pages || panel == Panel::Search))
            {
               const std::string_view input(event.text.text);
               if (replace_entry)
               {
                  entry.clear();
                  replace_entry = false;
               }
               if (entry.size() + input.size() <= 256 && (panel != Panel::Pages || input.find_first_not_of("0123456789") == input.npos))
                  entry += input;
               dirty = true;
            }
            break;
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
            primary.clear();
            following.clear();
            preceding.clear();
            thumbnails.clear();
            text = std::make_unique<Text>(renderer);
            upload();
            resize();
            break;
         case SDL_EVENT_WILL_ENTER_BACKGROUND:
            persist_view();
            background = true;
            break;
         case SDL_EVENT_DID_ENTER_FOREGROUND:
            background = false;
            resize();
            break;
         case SDL_EVENT_LOW_MEMORY:
            primary.clear();
            following.clear();
            preceding.clear();
            ahead->trim_cache();
            behind->trim_cache();
            page_previews->trim_cache();
            page_thumbnails.clear();
            thumbnails.clear();
            reader->trim_cache();
            covers->trim_cache();
            dirty = true;
            break;
         case SDL_EVENT_MOUSE_WHEEL:
            if (panel == Panel::None && !help && !menu_open && (SDL_GetModState() & SDL_KMOD_CTRL))
               zoom_by(std::pow(1.12f, event.wheel.y));
            else
               scroll_view(-event.wheel.y * 70);
            dirty = true;
            break;
         case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_RIGHT)
            {
               if (shelf)
                  select_at(event.button.x, event.button.y);
               perform(ui::Action::Menu);
               break;
            }
            if (event.button.button == SDL_BUTTON_LEFT)
            {
               dragging = true;
               touch_start_x = touch_x = event.button.x;
               touch_start_y = touch_y = event.button.y;
            }
            break;
         case SDL_EVENT_MOUSE_MOTION:
         {
            const auto previous_hover = control_at(pointer_x, pointer_y);
            pointer_x = event.motion.x;
            pointer_y = event.motion.y;
            if (previous_hover != control_at(pointer_x, pointer_y))
               dirty = true;
            if (hand_cursor)
               SDL_SetCursor(control_at(pointer_x, pointer_y) ? hand_cursor : SDL_GetDefaultCursor());
            if (dragging)
            {
               if (help || menu_open || shelf || panel != Panel::None)
                  scroll_view(-event.motion.yrel);
               else
               {
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
            if (dragging && event.button.button == SDL_BUTTON_LEFT)
            {
               dragging = false;
               if (std::hypot(event.button.x - touch_start_x, event.button.y - touch_start_y) < 8)
                  click(event.button.x, event.button.y);
            }
            break;
         case SDL_EVENT_FINGER_DOWN:
            if (!has_first)
            {
               has_first = true;
               first_finger = event.tfinger.fingerID;
               touch_started = SDL_GetTicks();
               touch_start_x = touch_x = event.tfinger.x * width;
               touch_start_y = touch_y = event.tfinger.y * height;
               pinching = false;
            }
            else if (!has_second)
            {
               has_second = true;
               second_finger = event.tfinger.fingerID;
               second_x = event.tfinger.x * width;
               second_y = event.tfinger.y * height;
               pinch_distance = std::hypot(second_x - touch_x, second_y - touch_y);
               pinching = true;
            }
            break;
         case SDL_EVENT_FINGER_MOTION:
         {
            const float x = event.tfinger.x * width, y = event.tfinger.y * height;
            if (has_second)
            {
               if (event.tfinger.fingerID == first_finger)
               {
                  touch_x = x;
                  touch_y = y;
               }
               else if (event.tfinger.fingerID == second_finger)
               {
                  second_x = x;
                  second_y = y;
               }
               const float distance = std::hypot(second_x - touch_x, second_y - touch_y);
               if (!shelf && !help && !menu_open && pinch_distance > 1)
                  zoom_by(distance / pinch_distance);
               pinch_distance = distance;
            }
            else if (event.tfinger.fingerID == first_finger)
            {
               if (help || menu_open || shelf || panel != Panel::None)
                  scroll_view(-(y - touch_y));
               else
               {
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
            else if (event.tfinger.fingerID == first_finger && has_first)
            {
               has_first = false;
               const float dx = event.tfinger.x * width - touch_start_x, dy = event.tfinger.y * height - touch_start_y;
               if (!pinching)
               {
                  if (std::hypot(dx, dy) < 12)
                  {
                     if (shelf && SDL_GetTicks() - touch_started > 600 && select_at(touch_start_x, touch_start_y))
                        perform(ui::Action::Menu);
                     else
                        click(touch_start_x, touch_start_y);
                  }
                  else if (!shelf && !menu_open && !help && panel == Panel::None && layout != ReadingMode::Continuous &&
                           touch_start_y >= top() && touch_start_y < bottom() && zoom <= 1.01f && std::abs(dx) > 70 &&
                           std::abs(dx) > std::abs(dy) * 1.5f)
                     turn((dx < 0 ? 1 : -1) * (manga ? -1 : 1));
               }
               has_second = false;
            }
            break;
         default:
            break;
      }
      return SDL_APP_CONTINUE;
   }
   void upload()
   {
      primary.clear();
   }
   void filter_page()
   {
      if (shelf || !image)
         return;
      const auto factor = base_scale() * zoom * render_scale;
      dirty |= primary.prepare(renderer, image, trim, tone, factor);
      if (pair_visible() && next_image)
         dirty |= following.prepare(renderer, next_image, trim, tone, factor);
      if (layout == ReadingMode::Continuous)
      {
         if (next_image)
            dirty |= following.prepare(renderer, next_image, trim, tone, (safe_w - 32) * zoom / neighbor_size(next_image).x * render_scale);
         if (previous_image)
            dirty |= preceding.prepare(
               renderer, previous_image, trim, tone, (safe_w - 32) * zoom / neighbor_size(previous_image).x * render_scale
            );
      }
   }
   void render_pages()
   {
      if (!image)
         return;
      const float scale = base_scale() * zoom;
      const auto size = current_size();
      float w = size.x * scale, h = size.y * scale;
      if (layout == ReadingMode::Continuous)
      {
         const float x = safe_x + (safe_w - w) * .5f + pan_x, y = top() - pan_y;
         primary.draw(renderer, {x, y, w, h}, rotation);
         if (next_image)
            following.draw(renderer, {x, y + h + 12, w, page_height(next_image)}, rotation);
         if (previous_image)
         {
            const float ph = page_height(previous_image);
            preceding.draw(renderer, {x, y - ph - 12, w, ph}, rotation);
         }
      }
      else if (pair_visible())
      {
         const auto second = neighbor_size(next_image);
         const float nw = second.x * scale, nh = second.y * scale;
         const float x = safe_x + (safe_w - w - nw) * .5f + pan_x;
         const float y = top() + std::max(0.f, (view_height() - std::max(h, nh)) * .5f) - pan_y;
         primary.draw(renderer, {x + (manga ? nw : 0), y, w, h}, rotation);
         if (next_image)
            following.draw(renderer, {x + (manga ? 0 : w), y, nw, nh}, rotation);
      }
      else
      {
         primary.draw(
            renderer, {safe_x + (safe_w - w) * .5f + pan_x, top() + std::max(0.f, (view_height() - h) * .5f) - pan_y, w, h}, rotation
         );
      }
   }
   void tick()
   {
      {
         std::optional<std::string> chosen;
         std::optional<std::pair<int, int>> window_size;
         bool folder = false;
         {
            std::lock_guard lock(inbox->mutex);
            chosen = std::move(inbox->selected);
            window_size = inbox->window_size;
            inbox->window_size.reset();
            inbox->selected.reset();
            folder = inbox->folder;
            if (!inbox->error.empty())
            {
               error = std::move(inbox->error);
               inbox->error.clear();
               dirty = true;
            }
            if (inbox->library)
            {
               collection = std::move(*inbox->library);
               order_books();
               fingerprints.clear();
               progress.clear();
               rebuild_library();
               inbox->library.reset();
               scroll = scroll_target = 0;
               shelf_selection = 0;
               thumbnails.clear();
               cover_source.clear();
               cover_generation = 0;
               dirty = true;
            }
         }
         if (window_size)
         {
            SDL_SetWindowSize(window, window_size->first, window_size->second);
            resize();
         }
         if (chosen)
         {
            if (folder)
            {
               start_scan(*chosen);
            }
            else
               open(*chosen);
         }
      }
      if (auto result = reader->poll(); result && result->generation == generation)
      {
         const bool first = !book;
         busy = false;
         book = result->book;
         page = result->page;
         error = result->error;
         image = result->image;
         upload();
         pan_x = pan_y = 0;
         if (first && !restored_view && book && book->metadata.manga_rtl)
            manga = true;
         request_neighbors();
         if (continuous_transition)
         {
            pan_y = continuous_carry;
            continuous_transition = false;
         }
         if (image)
         {
            if (first)
               schedule_view_save();
            if (!titles.contains(source) || titles[source] != book->title)
            {
               titles[source] = book->title;
               settings->save_title(key, book->title);
            }
            if (!settings->save_position(key, page))
               notice = "Reading position could not be saved.";
            if (!settings->remember(source))
               notice = "Recent books could not be saved.";
            if (std::find(collection.begin(), collection.end(), source) == collection.end())
               collection.insert(collection.begin(), source);
            order_books();
            auto p = progress_for(source);
            p.page = page;
            p.total = book->pages.size();
            p.finished = p.finished || page + (pair_visible() ? 2 : 1) >= p.total;
            progress[source] = p;
            if (!settings->save_progress(key, p))
               notice = "Reading progress could not be saved.";
            const auto window_title =
               title(source) + "  |  " + std::to_string(page + 1) + " / " + std::to_string(book->pages.size()) + "  |  oneam";
            SDL_SetWindowTitle(window, window_title.c_str());
            SDL_Log("Page %zu: %.2f ms%s", page + 1, result->milliseconds, result->cached ? " (cache)" : "");
            if (smoke_frames)
               SDL_Log("Book: %s", book->title.c_str());
         }
         dirty = true;
      }
      if (auto result = ahead->poll(); result && result->generation == ahead_generation && result->book && result->book->source == source)
      {
         next_image = result->image;
         if (next_image && pending_scroll > 0)
         {
            pan_y += pending_scroll;
            pending_scroll = 0;
         }
         if (!result->error.empty())
            notice = "Following page: " + result->error;
         clamp_pan();
         dirty = true;
      }
      if (auto result = behind->poll(); result && result->generation == behind_generation && result->book && result->book->source == source)
      {
         previous_image = result->image;
         if (previous_image && pending_scroll < 0)
         {
            pan_y += pending_scroll;
            pending_scroll = 0;
         }
         clamp_pan();
         dirty = true;
      }
      if (auto result = page_previews->poll(); result && result->generation == preview_generation && preview_pending)
      {
         Texture thumbnail;
         if (result->image)
         {
            const auto& im = *result->image;
            thumbnail.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, im.width, im.height));
            if (thumbnail)
            {
               SDL_UpdateTexture(thumbnail.get(), nullptr, im.pixels.data(), im.width * 4);
               SDL_SetTextureScaleMode(thumbnail.get(), SDL_SCALEMODE_LINEAR);
            }
         }
         if (page_thumbnails.size() >= 64)
            page_thumbnails.erase(page_thumbnails.begin());
         page_thumbnails.insert_or_assign(*preview_pending, std::move(thumbnail));
         preview_pending.reset();
         dirty = true;
      }
      if (auto result = covers->poll(); result && result->generation == cover_generation)
      {
         if (result->book)
         {
            titles[cover_source] = result->book->title;
            settings->save_title(fingerprint(cover_source), result->book->title);
         }
         Texture thumbnail;
         if (result->image)
         {
            const auto& im = *result->image;
            thumbnail.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, im.width, im.height));
            if (thumbnail)
            {
               SDL_UpdateTexture(thumbnail.get(), nullptr, im.pixels.data(), im.width * 4);
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
      if (view_dirty && now >= view_save_due)
         persist_view();
      advance_continuous();
      filter_page();
      const auto dt = std::min(.05f, (now - last_tick) / 1000.f);
      last_tick = now;
      if (std::abs(scroll_target - scroll) > .1f)
      {
         scroll += (scroll_target - scroll) * std::min(1.f, dt * 18);
         dirty = true;
      }
      else
         scroll = scroll_target;
#ifdef __EMSCRIPTEN__
      browser_page.store(!busy && image ? int(page + 1) : 0);
      browser_rotation.store(rotation);
      browser_zoom.store(zoom);
#endif
   }
   void fill(SDL_FRect r, SDL_Color color)
   {
      SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
      SDL_RenderFillRect(renderer, &r);
   }
   void control(const ui::Control& item, bool menu_item = false)
   {
      const bool hover = item.enabled && ui::contains(item.rect, pointer_x, pointer_y);
      const SDL_Color surface = item.active ? SDL_Color{43, 72, 64, 255} : hover ? SDL_Color{51, 59, 71, 255} : SDL_Color{32, 38, 48, 255};
      ui::rounded_rect(renderer, item.rect, 10, hover ? SDL_Color{80, 103, 111, 255} : surface);
      SDL_FRect inner{item.rect.x + 1, item.rect.y + 1, item.rect.w - 2, item.rect.h - 2};
      ui::rounded_rect(renderer, inner, 9, surface);
      const auto foreground = !item.enabled ? SDL_Color{93, 103, 119, 255} : item.active ? accent : ink;
      const float icon_x = item.label.empty() ? item.rect.x + (item.rect.w - 24) * .5f : item.rect.x + 13;
      ui::draw_icon(renderer, item.icon, {icon_x, item.rect.y + 12, 24, 24}, foreground);
      float reserved = 0;
#ifndef SDL_PLATFORM_ANDROID
      const auto hint = menu_item ? ui::shortcut(item.action) : std::string_view{};
      if (!hint.empty() && item.rect.w >= 230)
      {
         reserved = text->width(hint, 14) + 16;
         text->draw(hint, item.rect.x + item.rect.w - reserved, item.rect.y + 17, 14, muted);
      }
#else
      (void)menu_item;
#endif
      if (!item.label.empty())
         text->draw(item.label, item.rect.x + 47, item.rect.y + 15, 18, foreground, item.rect.w - 59 - reserved);
   }
   void scroll_indicator(SDL_FRect viewport, float offset, float maximum, float content)
   {
      if (maximum <= 0)
         return;
      const float h = std::min(viewport.h, std::max(24.f, viewport.h * viewport.h / content));
      ui::rounded_rect(renderer, {viewport.x + viewport.w + 5, viewport.y, 3, viewport.h}, 1.5f, {51, 59, 70, 255});
      ui::rounded_rect(
         renderer, {viewport.x + viewport.w + 5, viewport.y + (viewport.h - h) * offset / maximum, 3, h}, 1.5f, {132, 156, 155, 255}
      );
   }
   void render_menu()
   {
      const auto menu_layout = ui::menu(safe(), context(), menu_scroll);
      menu_scroll = menu_layout.scroll;
      fill(safe(), {0, 0, 0, 120});
      ui::rounded_rect(
         renderer, {menu_layout.panel.x + 2, menu_layout.panel.y + 4, menu_layout.panel.w, menu_layout.panel.h}, 14, {0, 0, 0, 100}
      );
      ui::rounded_rect(renderer, menu_layout.panel, 14, {27, 33, 43, 255});
      text->draw(
         menu_layout.panel.w >= 280 ? "Controls" : "Menu",
         menu_layout.panel.x + 20,
         menu_layout.panel.y + 22,
         23,
         ink,
         menu_layout.panel.w - 140
      );
      for (const auto& item : menu_layout.fixed)
         control(item);
      SDL_Rect clip{
         static_cast<int>(menu_layout.viewport.x),
         static_cast<int>(menu_layout.viewport.y),
         static_cast<int>(menu_layout.viewport.w),
         static_cast<int>(menu_layout.viewport.h)
      };
      SDL_SetRenderClipRect(renderer, &clip);
      for (const auto& item : menu_layout.items)
         if (item.rect.y + item.rect.h > menu_layout.viewport.y && item.rect.y < menu_layout.viewport.y + menu_layout.viewport.h)
            control(item, true);
      SDL_SetRenderClipRect(renderer, nullptr);
      scroll_indicator(menu_layout.viewport, menu_layout.scroll, menu_layout.max_scroll, menu_layout.content_height);
   }
   void render_help()
   {
      const auto help_bounds = help_panel(), viewport = help_viewport();
      auto lines = help_lines();
      const float content = static_cast<float>(lines.size()) * 26;
      const float maximum = std::max(0.f, content - viewport.h);
      help_scroll = std::clamp(help_scroll, 0.f, maximum);
      fill(safe(), {0, 0, 0, 120});
      ui::rounded_rect(renderer, help_bounds, 14, {27, 33, 43, 255});
      ui::draw_icon(renderer, ui::Icon::Help, {help_bounds.x + 20, help_bounds.y + 20, 26, 26}, accent);
      text->draw(help_bounds.w >= 300 ? "Reader help" : "Help", help_bounds.x + 58, help_bounds.y + 22, 23, ink, help_bounds.w - 130);
      SDL_Rect clip{static_cast<int>(viewport.x), static_cast<int>(viewport.y), static_cast<int>(viewport.w), static_cast<int>(viewport.h)};
      SDL_SetRenderClipRect(renderer, &clip);
      float y = viewport.y - help_scroll;
      for (const auto& line : lines)
      {
         if (y + 26 > viewport.y && y < viewport.y + viewport.h)
            text->draw(line, viewport.x, y, 18, ink, viewport.w);
         y += 26;
      }
      SDL_SetRenderClipRect(renderer, nullptr);
      scroll_indicator(viewport, help_scroll, maximum, content);
      control(help_close());
   }
   void render()
   {
      SDL_SetRenderDrawColor(renderer, 19, 22, 28, 255);
      SDL_RenderClear(renderer);
      if (!reading_mode())
         fill({safe_x, safe_y, safe_w, 72}, {25, 30, 39, 255});
      if (shelf)
      {
         ui::draw_icon(renderer, ui::Icon::Book, {safe_x + 20, safe_y + 22, 28, 28}, accent);
         if (safe_w >= 320)
            text->draw("oneam", safe_x + 60, safe_y + 22, 27, ink);
      }
      SDL_Rect clip{static_cast<int>(safe_x), static_cast<int>(top()), static_cast<int>(safe_w), static_cast<int>(view_height())};
      SDL_SetRenderClipRect(renderer, &clip);
      if (shelf)
         render_shelf();
      else if (image)
         render_pages();
      if (!shelf && (busy || !error.empty()))
      {
         fill({safe_x + 20, top() + 22, safe_w - 40, 78}, {35, 40, 50, 245});
         text->draw(busy ? "Opening your page..." : "This book needs attention", safe_x + 36, top() + 32, 23, ink, safe_w - 72);
         text->draw(busy ? "You can keep navigating while it loads." : error, safe_x + 36, top() + 63, 18, muted, safe_w - 72);
      }
      SDL_SetRenderClipRect(renderer, nullptr);
      for (const auto& item : ui::toolbar(safe(), context()))
         control(item);
      if (!reading_mode())
         fill({safe_x, bottom(), safe_w, 72}, {25, 30, 39, 255});
      if (shelf)
      {
         ui::draw_icon(renderer, ui::Icon::Book, {safe_x + 20, bottom() + 24, 24, 24}, muted);
         const auto count = std::to_string(library.size()) + (library.size() == 1 ? " book" : " books");
         text->draw(notice.empty() ? count : notice, safe_x + 56, bottom() + 27, 18, muted, safe_w - 76);
      }
      else if (!reading_mode())
      {
         auto buttons = ui::navigation(safe(), context());
         std::sort(buttons.begin(), buttons.end(), [](const auto& a, const auto& b) { return a.rect.x < b.rect.x; });
         for (const auto& item : buttons)
            control(item);
         const float left = buttons.front().rect.x + buttons.front().rect.w + 12;
         const float available = std::max(1.f, buttons.back().rect.x - left - 12);
         const auto range = std::to_string(page + 1) + (pair_visible() ? "-" + std::to_string(page + 2) : "");
         const auto position = book ? range + " / " + std::to_string(book->pages.size()) : "Opening";
         text->draw(position, left + std::max(0.f, (available - text->width(position, 21)) * .5f), bottom() + 14, 21, ink, available);
         const auto caption = notice.empty() ? (busy ? "Loading page" : title(source)) : notice;
         text->draw(caption, left, bottom() + 43, 14, muted, available);
      }
      if (menu_open)
         render_menu();
      if (help)
         render_help();
      if (panel != Panel::None)
         render_feature_panel();
      if (smoke_frames == 1 && !smoke_output.empty())
      {
         SDL_Surface* snapshot = SDL_RenderReadPixels(renderer, nullptr);
         if (snapshot)
         {
            SDL_SaveBMP(snapshot, smoke_output.c_str());
            SDL_DestroySurface(snapshot);
         }
      }
      SDL_RenderPresent(renderer);
      dirty = false;
   }
   void render_shelf()
   {
      const float origin = top() + shelf_heading_height() - scroll;
      text->draw("Your reading shelf", safe_x + 22, top() + 16 - scroll, shelf_heading_height() < 90 ? 26.f : 30.f, ink);
      control(search_control());
      if (shelf_heading_height() >= 132)
      {
         const std::array<const char*, 4> filters{"All books", "Unread", "In progress", "Finished"};
         const std::array<const char*, 3> sorts{"Recent", "Title", "Progress"};
         text->draw(
            std::string(filters[library_filter]) + "  /  " + sorts[library_sort] + "  /  Menu to change",
            safe_x + 22,
            top() + 108 - scroll,
            14,
            muted,
            safe_w - 44
         );
      }
      if (library.empty())
      {
         text->draw("Your next good read.", safe_x + 28, top() + 150, 28, ink, safe_w - 56);
         text->draw("Choose Open to pick a comic.", safe_x + 28, top() + 198, 19, muted, safe_w - 56);
         if (!error.empty())
            text->draw(error, safe_x + 28, top() + 244, 18, accent, safe_w - 56);
      }
      for (std::size_t i = 0; i < library.size(); ++i)
      {
         const float x = safe_x + 20 + static_cast<float>(i % columns()) * card_width();
         const float y = origin + static_cast<float>(i / columns()) * row_height();
         if (y + card_height() < top() || y > bottom())
            continue;
         const float w = card_width() - 12;
         ui::rounded_rect(
            renderer, {x, y, w, card_height()}, 12, i == shelf_selection ? SDL_Color{43, 64, 60, 255} : SDL_Color{30, 35, 44, 255}
         );
         if (auto found = thumbnails.find(library[i]); found != thumbnails.end() && found->second)
         {
            float tw = 0, th = 0;
            SDL_GetTextureSize(found->second.get(), &tw, &th);
            const float cover_height = card_height() - 50;
            const float scale = std::min((w - 20) / tw, cover_height / th);
            SDL_FRect destination{x + (w - tw * scale) * .5f, y + 10 + (cover_height - th * scale) * .5f, tw * scale, th * scale};
            SDL_RenderTexture(renderer, found->second.get(), nullptr, &destination);
         }
         else
         {
            fill({x + 12, y + 12, w - 24, card_height() - 58}, {43, 53, 65, 255});
            ui::draw_icon(renderer, ui::Icon::Book, {x + (w - 44) * .5f, y + (card_height() - 50 - 44) * .5f + 10, 44, 44}, muted);
            if (cover_source.empty() && !thumbnails.contains(library[i]) && !busy)
            {
               cover_source = library[i];
               cover_generation = covers->request(cover_source, 0, false, 512);
            }
         }
         text->draw(title(library[i]), x + 10, y + card_height() - 40, 16, ink, w - 20);
         const auto state = progress_for(library[i]);
         const auto label = state.finished ? std::string("Finished")
                            : state.total  ? std::to_string(int(100. * (state.page + 1) / state.total)) + "% read"
                                           : std::string("Unread");
         text->draw(label, x + 10, y + card_height() - 19, 13, state.finished ? accent : muted, w - 20);
      }
   }
   SDL_AppResult iterate()
   {
      tick();
      if (smoke_frames && image && !busy && smoke_action_index < smoke_actions.size() && SDL_GetTicks() >= smoke_action_due)
      {
         const auto& command = smoke_actions[smoke_action_index++];
         if (command.starts_with("scroll:"))
            scroll_view(std::stof(command.substr(7)));
         else if (command.starts_with("goto:"))
         {
            open_panel(Panel::Pages);
            entry = command.substr(5);
            confirm_entry();
         }
         else if (command == "reopen")
            open(source);
         else if (command.starts_with("book:"))
         {
            const auto index = std::size_t(std::stoul(command.substr(5)));
            if (index == 0 || index > book_order.size())
               return SDL_APP_FAILURE;
            open(book_order[index - 1]);
         }
         else if (command.starts_with("search:"))
         {
            perform(ui::Action::Library);
            open_panel(Panel::Search);
            entry = command.substr(7);
            confirm_entry();
         }
         else
         {
            const std::unordered_map<std::string, ui::Action> commands{
               {"single", ui::Action::Single},
               {"spread", ui::Action::Spread},
               {"continuous", ui::Action::Continuous},
               {"next", ui::Action::Next},
               {"previous", ui::Action::Previous},
               {"manga", ui::Action::Manga},
               {"trim", ui::Action::Trim},
               {"rotate", ui::Action::Rotate},
               {"bookmark", ui::Action::Bookmark},
               {"bookmarks", ui::Action::Bookmarks},
               {"pages", ui::Action::Pages},
               {"adjust", ui::Action::Adjust},
               {"brightness", ui::Action::BrightnessUp},
               {"contrast", ui::Action::ContrastUp},
               {"gamma", ui::Action::GammaUp},
               {"reset-image", ui::Action::ResetImage},
               {"zoom-in", ui::Action::ZoomIn},
               {"info", ui::Action::Info},
               {"close", ui::Action::Close},
               {"library", ui::Action::Library},
               {"sort-title", ui::Action::SortTitle},
               {"filter-read", ui::Action::FilterRead},
               {"mark-read", ui::Action::MarkRead},
               {"mark-unread", ui::Action::MarkUnread},
               {"next-book", ui::Action::NextBook},
               {"auto-next", ui::Action::AutoNext},
               {"last", ui::Action::Last},
               {"first", ui::Action::First}
            };
            const auto found = commands.find(command);
            if (found == commands.end())
            {
               SDL_Log("Unknown smoke action: %s", command.c_str());
               return SDL_APP_FAILURE;
            }
            perform(found->second);
         }
         SDL_Log("Smoke action: %s; page=%zu; layout=%d", command.c_str(), page + 1, int(layout));
         SDL_Log("View: layout=%d rotation=%d zoom=%.2f trim=%d", int(layout), rotation, zoom, int(trim));
         smoke_action_due = SDL_GetTicks() + 100;
      }
      if (background)
      {
#ifndef __EMSCRIPTEN__
         SDL_Delay(40);
#endif
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
#ifndef __EMSCRIPTEN__
         SDL_Delay(12);
#else
         ;
#endif
      if (smoke_frames > 0 && --smoke_frames == 0)
      {
         if (smoke_action_index != smoke_actions.size() || (smoke_expect_page && page != *smoke_expect_page))
            return SDL_APP_FAILURE;
         if (!startup_source.empty() && (!image || busy || !error.empty()))
            return SDL_APP_FAILURE;
         return SDL_APP_SUCCESS;
      }
      return SDL_APP_CONTINUE;
   }
};
}  // namespace
#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void oneam_web_open(const char* path)
{
   if (auto* app = browser_app.load(); app && path)
   {
      std::lock_guard lock(app->inbox->mutex);
      app->inbox->selected = path;
      app->inbox->folder = false;
   }
}
extern "C" EMSCRIPTEN_KEEPALIVE void oneam_web_resize(int width, int height)
{
   if (auto* app = browser_app.load(); app && width > 0 && height > 0)
   {
      std::lock_guard lock(app->inbox->mutex);
      app->inbox->window_size = std::pair{width, height};
   }
}
extern "C" EMSCRIPTEN_KEEPALIVE int oneam_web_page()
{
   return browser_page.load();
}
extern "C" EMSCRIPTEN_KEEPALIVE int oneam_web_rotation()
{
   return browser_rotation.load();
}
extern "C" EMSCRIPTEN_KEEPALIVE double oneam_web_zoom()
{
   return browser_zoom.load();
}
#endif
SDL_AppResult SDL_AppInit(void** state, int argc, char** argv)
{
   try
   {
      auto app = std::make_unique<App>();
      if (!app->init(argc, argv))
      {
         SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "oneam", SDL_GetError(), nullptr);
         return SDL_APP_FAILURE;
      }
      app->resize();
#ifdef __EMSCRIPTEN__
      browser_app = app.get();
      oneam::web_ready();
#endif
      *state = app.release();
      return SDL_APP_CONTINUE;
   }
   catch (const std::exception& ex)
   {
      SDL_Log("Startup failed: %s", ex.what());
      return SDL_APP_FAILURE;
   }
}
SDL_AppResult SDL_AppEvent(void* state, SDL_Event* event)
{
   try
   {
      return static_cast<App*>(state)->event(*event);
   }
   catch (const std::exception& ex)
   {
      SDL_Log("Event failed: %s", ex.what());
      return SDL_APP_FAILURE;
   }
}
SDL_AppResult SDL_AppIterate(void* state)
{
   try
   {
      return static_cast<App*>(state)->iterate();
   }
   catch (const std::exception& ex)
   {
      SDL_Log("Reader failed: %s", ex.what());
      return SDL_APP_FAILURE;
   }
}
void SDL_AppQuit(void* state, SDL_AppResult)
{
#ifdef __EMSCRIPTEN__
   browser_app.store(nullptr);
#endif
   delete static_cast<App*>(state);
   SDL_Quit();
}
