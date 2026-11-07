#include <algorithm>
#include <charconv>
#include "reader.hpp"

namespace oneam
{
namespace
{
std::string decoded(std::string_view text)
{
   std::string out;
   for (std::size_t i = 0; i < text.size() && out.size() < 4096; ++i)
   {
      if (text[i] != '&')
      {
         out += text[i];
         continue;
      }
      const auto end = text.find(';', i);
      if (end == text.npos || end - i > 12)
         return {};
      auto entity = text.substr(i + 1, end - i - 1);
      if (entity == "amp")
         out += '&';
      else if (entity == "lt")
         out += '<';
      else if (entity == "gt")
         out += '>';
      else if (entity == "quot")
         out += '"';
      else if (entity == "apos")
         out += '\'';
      else if (entity.starts_with('#'))
      {
         entity.remove_prefix(1);
         int base = 10;
         if (entity.starts_with('x'))
         {
            entity.remove_prefix(1);
            base = 16;
         }
         unsigned c = 0;
         const auto parsed = std::from_chars(entity.data(), entity.data() + entity.size(), c, base);
         if (parsed.ec != std::errc{} || parsed.ptr != entity.data() + entity.size() || c == 0 || c > 0x10ffff ||
             (c >= 0xd800 && c <= 0xdfff))
            return {};
         if (c < 128)
            out += char(c);
         else
         {
            if (c < 0x800)
               out += char(0xc0 | (c >> 6));
            else
            {
               if (c >= 0x10000)
               {
                  out += char(0xf0 | (c >> 18));
                  out += char(0x80 | ((c >> 12) & 63));
               }
               else
                  out += char(0xe0 | (c >> 12));
               out += char(0x80 | ((c >> 6) & 63));
            }
            out += char(0x80 | (c & 63));
         }
      }
      else
         return {};
      i = end;
   }
   for (auto& c : out)
      if (c == '\r' || c == '\n' || c == '\t')
         c = ' ';
   const auto start = out.find_first_not_of(' '), end = out.find_last_not_of(' ');
   return start == out.npos ? "" : out.substr(start, end - start + 1);
}
}  // namespace
Metadata parse_comic_info(std::string_view xml)
{
   Metadata result;
   if (xml.size() > 1024 * 1024 || xml.find("<!DOCTYPE") != xml.npos || xml.find("<!ENTITY") != xml.npos)
      return result;
   // Read only scalar children of ComicInfo. No external resources or entity expansion.
   std::vector<std::string> stack;
   std::size_t pos = 0;
   while (pos < xml.size())
   {
      const auto open = xml.find('<', pos);
      if (open == xml.npos)
         break;
      if (stack.size() == 2 && open > pos)
      {
         const auto value = decoded(xml.substr(pos, open - pos));
         const auto& tag = stack.back();
         if (tag == "Title")
            result.title += value;
         else if (tag == "Series")
            result.series += value;
         else if (tag == "Number")
            result.number += value;
         else if (tag == "Writer")
            result.writer += value;
         else if (tag == "Summary")
            result.summary += value;
         else if (tag == "Manga")
            result.manga_rtl = value == "YesAndRightToLeft";
      }
      if (xml.substr(open).starts_with("<!--"))
      {
         auto end = xml.find("-->", open + 4);
         if (end == xml.npos)
            return {};
         pos = end + 3;
         continue;
      }
      if (xml.substr(open).starts_with("<![CDATA["))
      {
         // CDATA is literal text and never expands entities.
         auto end = xml.find("]]>", open + 9);
         if (end == xml.npos)
            return {};
         if (stack.size() == 2)
         {
            std::string value(xml.substr(open + 9, end - open - 9));
            for (char& c : value)
               if (c == '\r' || c == '\n' || c == '\t')
                  c = ' ';
            value.resize(std::min<std::size_t>(value.size(), 4096));
            if (stack.back() == "Title")
               result.title += value;
            else if (stack.back() == "Series")
               result.series += value;
            else if (stack.back() == "Summary")
               result.summary += value;
            else if (stack.back() == "Writer")
               result.writer += value;
            else if (stack.back() == "Number")
               result.number += value;
            else if (stack.back() == "Manga")
               result.manga_rtl = value == "YesAndRightToLeft";
         }
         pos = end + 3;
         continue;
      }
      const auto close = xml.find('>', open + 1);
      if (close == xml.npos)
         return {};
      auto tag = xml.substr(open + 1, close - open - 1);
      pos = close + 1;
      if (tag.starts_with('?'))
         continue;
      if (tag.starts_with('!'))
         return {};
      const bool closing = tag.starts_with('/'), empty = tag.ends_with('/');
      if (closing)
         tag.remove_prefix(1);
      tag = tag.substr(0, tag.find_first_of(" /\t\r\n"));
      if (tag.empty())
         return {};
      if (closing)
      {
         if (stack.empty() || stack.back() != tag)
            return {};
         stack.pop_back();
      }
      else if (!empty)
      {
         if (stack.empty() && tag != "ComicInfo")
            return {};
         stack.emplace_back(tag);
         if (stack.size() > 16)
            return {};
      }
   }
   if (!stack.empty())
      return {};
   return result;
}
std::string metadata_title(const Metadata& m)
{
   auto title = m.series;
   if (!m.number.empty())
      title += (title.empty() ? "#" : " #") + m.number;
   if (!m.title.empty())
      title += (title.empty() ? "" : " - ") + m.title;
   return title;
}
}  // namespace oneam
