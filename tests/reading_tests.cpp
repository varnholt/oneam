#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include "reading.hpp"
#include "scaler.hpp"

using namespace oneam;
void require(bool value, const char* message)
{
   if (!value)
      throw std::runtime_error(message);
}
int main()
{
   auto root = std::filesystem::temp_directory_path() / ("oneam-reading-" + std::to_string(std::random_device{}()));
   try
   {
      Image image{100, 120, std::vector<std::uint8_t>(100 * 120 * 4, 255)};
      for (int y = 15; y < 105; ++y)
         for (int x = 10; x < 90; ++x)
         {
            auto i = (y * 100 + x) * 4;
            image.pixels[i] = 40;
            image.pixels[i + 1] = 80;
            image.pixels[i + 2] = 120;
         }
      image.content = detect_content(image);
      require(
         image.content.x == 8 && image.content.y == 13 && image.content.w == 84 && image.content.h == 94,
         "margin detection preserves safety border"
      );
      require(
         page_size(image, true, 1).x == 94 && page_size(image, true, 1).y == 84,
         "rotation swaps layout dimensions without changing proportions"
      );
      Image blank{100, 120, std::vector<std::uint8_t>(100 * 120 * 4, 255)};
      require(detect_content(blank).w == 100, "blank pages are not cropped");
      auto color = filter_image(image, 40, 45, {10, 15, 80, 90}, {.1f, 1, 1});
      require(
         bool(color) && color->pixels[0] >= 65 && color->pixels[0] <= 66 && color->pixels[3] == 255,
         "cropped image tone adjustment preserves alpha"
      );
      require(!filter_image(image, 20, 20, {99, 0, 2, 2}, {}), "invalid crop rejected");
      const auto lut = tone_table({});
      for (int i = 0; i < 256; ++i)
         require(lut[i] == i, "neutral image adjustments are lossless");
      for (bool cover : {false, true})
         for (std::size_t count = 1; count < 15; ++count)
            for (std::size_t page = 0; page < count; ++page)
            {
               const auto start = spread_start(page, cover);
               require(start <= page && page - start < 2, "spread anchor contains requested page");
               require(spread_next(start, count, cover, 1) < count, "next spread stays in bounds");
               if (start)
                  require(spread_next(start, count, cover, -1) < start, "previous spread moves backward");
            }
      require(spread_start(0, true) == 0 && spread_start(2, true) == 1 && spread_start(2, false) == 2, "cover offset is respected");
      auto metadata = parse_comic_info(
         "<?xml version=\"1.0\"?><ComicInfo><Series>Night &amp; Day</Series><Number>02</Number><Title><![CDATA[One < "
         "Two]]></Title><Writer>Ren&#233;</Writer><Manga>YesAndRightToLeft</Manga><Pages><Page Image=\"0\"/></Pages></ComicInfo>"
      );
      require(
         metadata.series == "Night & Day" && metadata.title == "One < Two" && metadata.writer == "Ren\xc3\xa9" && metadata.manga_rtl,
         "ComicInfo scalar fields and UTF-8 entities parsed"
      );
      require(metadata_title(metadata) == "Night & Day #02 - One < Two", "metadata display title");
      require(
         parse_comic_info("<!DOCTYPE ComicInfo [<!ENTITY x SYSTEM 'file:///secret'>]><ComicInfo><Title>&x;</Title></ComicInfo>")
            .title.empty(),
         "external entities rejected"
      );
      require(parse_comic_info("<ComicInfo><Title>broken</ComicInfo>").title.empty(), "malformed metadata ignored");
      require(text_matches("Lucky Luke", "LUKE") && !text_matches("Lucky Luke", "Asterix"), "library search ignores ASCII case");
      Settings settings(root.string());
      const std::string key = "0123456789abcdef";
      require(
         settings.save_bookmarks(key, {8, 2, 8}) && settings.bookmarks(key) == std::vector<std::size_t>{2, 8},
         "bookmarks persist sorted without duplicates"
      );
      require(settings.save_progress(key, {8, 12, false}) && settings.progress(key).page == 8, "reading progress persists");
      require(settings.save_progress(key, {11, 12, true}) && settings.progress(key).finished, "finished state persists");
      require(settings.save_progress(key, {}) && settings.progress(key).total == 0, "mark unread persists across restarts");
      require(!settings.save_progress(key, {12, 12, false}) && !settings.save_bookmarks("../escape", {1}), "saved-state bounds enforced");
      require(settings.save_preference("reader", "2 1 1") && settings.preference("reader") == "2 1 1", "reader preferences persist");
      require(!settings.save_preference("../escape", "value"), "preference paths constrained");
      std::filesystem::remove_all(root);
      std::cout << "Reading layouts, image adjustments, metadata and saved state passed.\n";
      return 0;
   }
   catch (const std::exception& error)
   {
      std::filesystem::remove_all(root);
      std::cerr << error.what() << '\n';
      return 1;
   }
}
