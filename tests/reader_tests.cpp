#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include "reader.hpp"

using namespace oneam;
namespace
{
void require(bool condition, const char* message)
{
   if (!condition)
      throw std::runtime_error(message);
}
std::string utf8(const std::filesystem::path& p)
{
   const auto s = p.u8string();
   return {reinterpret_cast<const char*>(s.data()), s.size()};
}
std::vector<std::uint8_t> bmp(int w = 2, int h = 3, std::uint8_t red = 200)
{
   const auto row = (w * 3 + 3) & ~3;
   std::vector<std::uint8_t> b(static_cast<std::size_t>(row) * h + 54, 0);
   auto put = [&](std::size_t offset, std::uint32_t value)
   {
      for (unsigned i = 0; i < 4; ++i)
         b[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
   };
   b[0] = 'B';
   b[1] = 'M';
   put(2, static_cast<std::uint32_t>(b.size()));
   put(10, 54);
   put(14, 40);
   put(18, w);
   put(22, h);
   b[26] = 1;
   b[28] = 24;
   for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x)
         b[54 + y * row + x * 3 + 2] = red;
   return b;
}
enum
{
   ARCHIVE_FORMAT_ZIP,
   ARCHIVE_FORMAT_7ZIP,
   ARCHIVE_FORMAT_TAR_PAX_RESTRICTED
};
void make_archive(
   const std::filesystem::path& path,
   int format,
   const std::vector<std::pair<std::string, std::vector<std::uint8_t>>>& files
)
{
   if (format != ARCHIVE_FORMAT_ZIP)
   {
      std::filesystem::copy_file(
         std::filesystem::path(ONEAM_ARCHIVE_FIXTURES) / (format == ARCHIVE_FORMAT_7ZIP ? "pages.7z" : "pages.tar"),
         path,
         std::filesystem::copy_options::overwrite_existing
      );
      return;
   }
   std::ofstream out(path, std::ios::binary);
   auto number = [&](std::uint32_t value, int bytes)
   {
      for (int n = 0; n < bytes; ++n)
         out.put(static_cast<char>(value >> (n * 8)));
   };
   auto crc = [](const std::vector<std::uint8_t>& data)
   {
      std::uint32_t value = 0xffffffffu;
      for (auto byte : data)
      {
         value ^= byte;
         for (int n = 0; n < 8; ++n)
            value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
      }
      return value ^ 0xffffffffu;
   };
   std::vector<std::uint32_t> offsets;
   for (const auto& [name, bytes] : files)
   {
      offsets.push_back(static_cast<std::uint32_t>(out.tellp()));
      number(0x04034b50, 4);
      number(20, 2);
      number(0, 2);
      number(0, 2);
      number(0, 4);
      number(crc(bytes), 4);
      number(static_cast<std::uint32_t>(bytes.size()), 4);
      number(static_cast<std::uint32_t>(bytes.size()), 4);
      number(static_cast<std::uint32_t>(name.size()), 2);
      number(0, 2);
      out << name;
      out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
   }
   const auto central = static_cast<std::uint32_t>(out.tellp());
   for (std::size_t i = 0; i < files.size(); ++i)
   {
      const auto& [name, bytes] = files[i];
      number(0x02014b50, 4);
      number(20, 2);
      number(20, 2);
      number(0, 2);
      number(0, 2);
      number(0, 4);
      number(crc(bytes), 4);
      number(static_cast<std::uint32_t>(bytes.size()), 4);
      number(static_cast<std::uint32_t>(bytes.size()), 4);
      number(static_cast<std::uint32_t>(name.size()), 2);
      number(0, 2);
      number(0, 2);
      number(0, 2);
      number(0, 2);
      number(0, 4);
      number(offsets[i], 4);
      out << name;
   }
   const auto central_size = static_cast<std::uint32_t>(out.tellp()) - central;
   number(0x06054b50, 4);
   number(0, 2);
   number(0, 2);
   number(static_cast<std::uint32_t>(files.size()), 2);
   number(static_cast<std::uint32_t>(files.size()), 2);
   number(central_size, 4);
   number(central, 4);
   number(0, 2);
   require(!!out, "write test ZIP");
}
void natural_order()
{
   std::vector<std::string> names{"page10.JPG", "page2.JPG", "page1.JPG", "page02.JPG", "page99999999999999999999999999.png"};
   std::sort(names.begin(), names.end(), natural_less);
   require(names[0] == "page1.JPG" && names[1] == "page2.JPG" && names[2] == "page02.JPG" && names[3] == "page10.JPG", "natural order");
   for (const auto& a : names)
      for (const auto& b : names)
      {
         require(!(natural_less(a, b) && natural_less(b, a)), "strict comparator");
         for (const auto& c : names)
            if (natural_less(a, b) && natural_less(b, c))
               require(natural_less(a, c), "transitive comparator");
      }
   require(is_image("PAGE.JPEG") && is_image("a.PNG") && !is_image("a.txt"), "image extensions");
   require(is_book("Book.CB7") && is_book("a.CBT"), "book extensions");
}
void archive_roundtrip(const std::filesystem::path& root)
{
   for (const auto format : {ARCHIVE_FORMAT_ZIP, ARCHIVE_FORMAT_7ZIP, ARCHIVE_FORMAT_TAR_PAX_RESTRICTED})
   {
      const auto path = root / (std::to_string(format) + ".cbz");
      make_archive(
         path,
         format,
         {{"pages/page10.BMP", bmp(2, 3, 10)}, {"note.txt", {1, 2}}, {"pages/page2.BMP", bmp(2, 3, 20)}, {"__MACOSX/junk.bmp", {1}}}
      );
      Archive archive;
      auto book = archive.index(utf8(path));
      require(book.has_value(), "index archive");
      require(book->pages.size() == 2 && book->pages[0].name == "pages/page2.BMP", "filter and sort");
      for (const auto page : {0u, 1u, 0u, 0u})
      {
         auto bytes = archive.read(*book, page);
         require(bytes.has_value(), "read forwards/backwards");
         auto image = decode_image(*bytes);
         require(image.has_value(), "decode page");
         require(image->width == 2 && image->height == 3 && image->pixels[0] == (page ? 10 : 20), "right page pixels");
      }
      require(!archive.read(*book, 99), "out of bounds rejected");
      require(!archive.index(utf8(path), [] { return true; }), "index cancellation");
      book = archive.index(utf8(path));
      require(book.has_value(), "index after cancellation");
      require(!archive.read(*book, 0, [] { return true; }), "read cancellation");
      require(archive.read(*book, 0).has_value(), "read recovers after cancellation");
   }
   Archive archive;
   make_archive(root / "empty.cbz", ARCHIVE_FORMAT_ZIP, {{"text.txt", {1}}});
   require(!archive.index(utf8(root / "empty.cbz")), "empty book rejected");
   {
      std::ofstream out(root / "broken.cbz");
      out << "broken";
   }
   require(!archive.index(utf8(root / "broken.cbz")), "corrupt archive rejected");
   require(!archive.index(utf8(root / "missing.cbz")), "missing file rejected");
   require(!decode_image(std::vector<std::uint8_t>{1, 2, 3}), "bad image rejected");
   auto oversized = bmp();
   oversized[18] = 0;
   oversized[19] = 64;
   oversized[22] = 0;
   oversized[23] = 64;
   require(!decode_image(oversized), "huge image rejected before allocation");
}
void cache_test()
{
   ImageCache cache(24);
   auto image = std::make_shared<Image>();
   image->pixels.resize(12);
   cache.put(0, image);
   cache.put(1, image);
   require(cache.bytes() == 24, "cache byte accounting");
   require(!!cache.get(0), "cache get");
   cache.put(2, image);
   require(!cache.get(1) && !!cache.get(0) && !!cache.get(2), "LRU evicts least recently used");
   cache.put(0, image);
   require(cache.bytes() == 24, "replacement accounting");
   cache.clear();
   require(cache.bytes() == 0 && !cache.get(0), "cache clear");
   auto huge = std::make_shared<Image>();
   huge->pixels.resize(25);
   cache.put(9, huge);
   require(!cache.get(9), "oversized item not retained");
}
void rar_test(const std::filesystem::path& root)
{
   for (const auto& [name, expected] :
        std::vector<std::pair<std::string, std::string>>{{"rar4", "test text document\r\n"}, {"rar5", "hello libarchive test suite!\n"}})
   {
      std::ifstream input(std::filesystem::path(ONEAM_ARCHIVE_FIXTURES) / (name + ".uu"));
      require(!!input, "RAR reference fixture available");
      std::vector<std::uint8_t> bytes;
      for (std::string line; std::getline(input, line);)
      {
         if (line.starts_with("begin ") || line == "end" || line.empty())
            continue;
         const auto count = (static_cast<unsigned char>(line[0]) - 32) & 63;
         auto value = [&](std::size_t p) { return (static_cast<unsigned char>(line.at(p)) - 32) & 63; };
         for (int i = 0; i < count; i += 3)
         {
            const std::size_t offset = 1 + static_cast<std::size_t>(i / 3) * 4;
            const auto packed = (value(offset) << 18) | (value(offset + 1) << 12) | (value(offset + 2) << 6) | value(offset + 3);
            for (int n = 0; n < std::min(3, count - i); ++n)
               bytes.push_back(static_cast<std::uint8_t>(packed >> (16 - n * 8)));
         }
      }
      const auto destination = root / name;
      {
         std::ofstream out(destination, std::ios::binary);
         out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
      }
      Archive archive;
      // These upstream interoperability fixtures contain text rather than comic
      // images. Exercise the identical entry reader without the image-name filter.
      Book book{utf8(destination), {{"reference", 0}}};
      auto data = archive.read(book, 0);
      require(data.has_value() && std::string(data->begin(), data->end()) == expected, "RAR4/RAR5 extraction");
   }
}
void settings_test(const std::filesystem::path& root)
{
   Settings settings(utf8(root / "prefs"));
   const auto first = book_key(utf8(root / "a" / "book.cbz"));
   const auto second = book_key(utf8(root / "b" / "book.cbz"));
   require(first != second, "same basename does not collide");
   require(settings.position(first) == 0, "default position");
   require(settings.save_position(first, 7) && settings.position(first) == 7, "position roundtrip");
   require(settings.save_position(first, 9) && settings.position(first) == 9, "replace saved position");
   require(!settings.save_position("../../escape", 2), "settings key validation");
   require(settings.save_folder(utf8(root)) && settings.folder() == utf8(root), "library folder survives restart");
   require(settings.save_title(first, "Book title") && settings.title(first) == "Book title", "document title survives restart");
   require(settings.remember("first.cbz") && settings.remember("second.cbz") && settings.remember("first.cbz"), "recent save");
   auto recent = settings.recent();
   require(recent.size() == 2 && recent[0] == "first.cbz", "recent deduplication");
   {
      std::ofstream out(root / "prefs" / (first + ".pos"));
      out << "-1";
   }
   require(settings.position(first) == 0, "corrupt position rejected");
   const auto file = root / "change.cbz";
   {
      std::ofstream out(file);
      out << "one";
   }
   const auto before = book_key(utf8(file));
   {
      std::ofstream out(file);
      out << "longer";
   }
   require(before != book_key(utf8(file)), "modified book invalidates old state");
}
Update wait_for(Reader& reader, std::uint64_t generation)
{
   const auto start = SDL_GetTicks();
   while (SDL_GetTicks() - start < 10000)
   {
      if (auto update = reader.poll(); update && update->generation == generation)
         return std::move(*update);
      SDL_Delay(2);
   }
   throw std::runtime_error("worker timed out");
}
void worker_test(const std::filesystem::path& root)
{
   make_archive(root / "a.cbz", ARCHIVE_FORMAT_ZIP, {{"page1.bmp", bmp(2, 3, 33)}, {"page2.bmp", bmp(2, 3, 77)}});
   make_archive(root / "b.cbz", ARCHIVE_FORMAT_ZIP, {{"page1.bmp", bmp(2, 3, 99)}});
   Reader reader;
   auto generation = reader.request(utf8(root / "a.cbz"), 0);
   auto result = wait_for(reader, generation);
   require(result.image && result.image->pixels[0] == 33, "worker opens book");
   for (int i = 0; i < 100; ++i)
      reader.request(utf8(root / "a.cbz"), i % 2);
   generation = reader.request(utf8(root / "b.cbz"), 999);
   result = wait_for(reader, generation);
   require(result.image && result.image->pixels[0] == 99 && result.page == 0, "latest book wins and position clamps");
   generation = reader.request(utf8(root / "b.cbz"), 0);
   result = wait_for(reader, generation);
   require(result.cached, "repeat page cached");
   reader.trim_cache();
   generation = reader.request(utf8(root / "b.cbz"), 0);
   result = wait_for(reader, generation);
   require(result.image && !result.cached, "memory pressure releases cache and reopens decoder");
   generation = reader.request(utf8(root / "missing.cbz"), 0);
   result = wait_for(reader, generation);
   require(!result.image && !result.error.empty(), "load failure clears stale page");
}
}  // namespace
int main(int argc, char** argv)
{
   auto root = std::filesystem::temp_directory_path() / ("oneam-tests-" + std::to_string(std::random_device{}()));
   try
   {
      require(SDL_Init(0), "SDL init");
      std::filesystem::create_directories(root);
      natural_order();
      archive_roundtrip(root);
      rar_test(root);
      cache_test();
      settings_test(root);
      worker_test(root);
      if (argc > 1)
      {
         const std::string arg = argv[1];
         make_archive(
            std::filesystem::path(std::u8string(arg.begin(), arg.end())),
            ARCHIVE_FORMAT_ZIP,
            {{"page2.bmp", bmp(640, 960, 170)}, {"page10.bmp", bmp(800, 600, 80)}}
         );
      }
      std::filesystem::remove_all(root);
      SDL_Quit();
      std::cout << "All reader regressions passed.\n";
      return 0;
   }
   catch (const std::exception& ex)
   {
      std::cerr << "FAIL: " << ex.what() << "\n";
      std::filesystem::remove_all(root);
      SDL_Quit();
      return 1;
   }
}
