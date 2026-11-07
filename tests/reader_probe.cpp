#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include "reader.hpp"

using namespace oneam;
namespace
{
std::string utf8(const std::filesystem::path& path)
{
   auto bytes = path.u8string();
   return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
std::string json(std::string_view value)
{
   std::string out = "\"";
   for (const unsigned char c : value)
   {
      if (c == '\\' || c == '"')
      {
         out += '\\';
         out += static_cast<char>(c);
      }
      else if (c == '\n')
         out += "\\n";
      else if (c == '\r')
         out += "\\r";
      else if (c < 32)
         out += '?';
      else
         out += static_cast<char>(c);
   }
   return out + '"';
}
double milliseconds(std::chrono::steady_clock::time_point start)
{
   return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
}  // namespace
int main(int argc, char** argv)
{
   if (argc < 3)
   {
      std::cerr << "reader_probe <file-or-directory> <report.json> [--full]\n";
      return 2;
   }
   if (!SDL_Init(0))
      return 2;
   std::vector<std::filesystem::path> files;
   const std::string argument = argv[1];
   const auto path = std::filesystem::path(std::u8string(argument.begin(), argument.end()));
   if (std::filesystem::is_directory(path))
   {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
         if (entry.is_regular_file() && is_book(utf8(entry.path())))
            files.push_back(entry.path());
   }
   else
      files.push_back(path);
   std::sort(files.begin(), files.end());
   std::ofstream report(argv[2]);
   report << "[\n";
   int failures = 0;
   std::size_t tested_pages = 0;
   const bool full = argc > 3 && std::string_view(argv[3]) == "--full";
   const auto total_start = std::chrono::steady_clock::now();
   for (std::size_t i = 0; i < files.size(); ++i)
   {
      Archive archive;
      const auto start = std::chrono::steady_clock::now();
      auto book = archive.index(utf8(files[i]));
      const double index_ms = milliseconds(start);
      report << (i ? ",\n" : "") << "  {\"source\":" << json(utf8(files[i])) << ",\"index_ms\":" << index_ms;
      if (!book)
      {
         ++failures;
         report << ",\"error\":" << json(book.error()) << "}";
         continue;
      }
      report << ",\"format\":" << json(book->format) << ",\"pages\":" << book->pages.size() << ",\"samples\":[";
      std::set<std::size_t> samples{0, book->pages.size() / 2, book->pages.size() - 1};
      if (full)
         for (std::size_t page = 0; page < book->pages.size(); ++page)
            samples.insert(page);
      std::size_t count = 0;
      for (const auto page : samples)
      {
         const auto page_start = std::chrono::steady_clock::now();
         auto bytes = archive.read(*book, page);
         auto image = bytes ? decode_image(*bytes) : Result<Image>(std::unexpected(bytes.error()));
         report << (count++ ? "," : "") << "{\"page\":" << page + 1 << ",\"ms\":" << milliseconds(page_start);
         if (image)
         {
            ++tested_pages;
            report << ",\"width\":" << image->width << ",\"height\":" << image->height;
         }
         else
         {
            ++failures;
            report << ",\"error\":" << json(image.error());
         }
         report << "}";
      }
      report << "]}";
      report.flush();
      std::cout << i + 1 << "/" << files.size() << " books, " << tested_pages << " pages decoded, " << failures << " failures\n";
   }
   report << "\n]\n";
   std::cout << "Finished in " << milliseconds(total_start) << " ms\n";
   SDL_Quit();
   return failures ? 1 : 0;
}
