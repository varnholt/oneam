#include "reader.hpp"
#ifdef _WIN32
#include <objbase.h>
#include <propidl.h>
#endif
// GUID definitions must be enabled before the archive interface headers.
// clang-format off
#include <Common/MyInitGuid.h>
#include <Common/MyCom.h>
#include <7zip/Archive/IArchive.h>
// clang-format on
#include <algorithm>
#include <array>
#include <limits>

extern "C" HRESULT WINAPI CreateObject(const GUID* clsid, const GUID* iid, void** object);

namespace oneam
{
namespace
{
struct Property : PROPVARIANT
{
   Property() : PROPVARIANT{}
   {
      vt = VT_EMPTY;
   }
   ~Property()
   {
      if (vt == VT_BSTR)
         SysFreeString(bstrVal);
   }
};
bool canceled(const Cancel& cancel)
{
   return cancel && cancel();
}
std::string utf8(BSTR text)
{
   std::string result;
   if (!text)
      return result;
   const auto size = SysStringLen(text);
   for (unsigned i = 0; i < size; ++i)
   {
      std::uint32_t c = static_cast<std::uint32_t>(text[i]);
      if (c >= 0xd800 && c <= 0xdbff && i + 1 < size)
      {
         const auto next = static_cast<std::uint32_t>(text[i + 1]);
         if (next >= 0xdc00 && next <= 0xdfff)
         {
            c = 0x10000 + ((c - 0xd800) << 10) + next - 0xdc00;
            ++i;
         }
      }
      if (c < 0x80)
         result += static_cast<char>(c);
      else if (c < 0x800)
      {
         result += static_cast<char>(0xc0 | (c >> 6));
         result += static_cast<char>(0x80 | (c & 63));
      }
      else if (c < 0x10000)
      {
         result += static_cast<char>(0xe0 | (c >> 12));
         result += static_cast<char>(0x80 | ((c >> 6) & 63));
         result += static_cast<char>(0x80 | (c & 63));
      }
      else
      {
         result += static_cast<char>(0xf0 | (c >> 18));
         result += static_cast<char>(0x80 | ((c >> 12) & 63));
         result += static_cast<char>(0x80 | ((c >> 6) & 63));
         result += static_cast<char>(0x80 | (c & 63));
      }
   }
   return result;
}
class Input final : public IInStream, public CMyUnknownImp
{
   Z7_COM_UNKNOWN_IMP_2(IInStream, ISequentialInStream)
   Z7_IFACE_COM7_IMP(ISequentialInStream)
   Z7_IFACE_COM7_IMP(IInStream)
public:
   SDL_IOStream* stream = nullptr;
   Cancel cancel;
   explicit Input(SDL_IOStream* value) : stream(value)
   {
   }
   ~Input()
   {
      if (stream)
         SDL_CloseIO(stream);
   }
};
Z7_COM7F_IMF(Input::Read(void* data, UInt32 size, UInt32* processed))
{
   if (processed)
      *processed = 0;
   if (canceled(cancel))
      return E_ABORT;
   const auto count = SDL_ReadIO(stream, data, size);
   if (processed)
      *processed = static_cast<UInt32>(count);
   return !count && size && SDL_GetIOStatus(stream) == SDL_IO_STATUS_ERROR ? E_FAIL : S_OK;
}
Z7_COM7F_IMF(Input::Seek(Int64 offset, UInt32 origin, UInt64* position))
{
   if (canceled(cancel))
      return E_ABORT;
   if (origin > 2)
      return E_INVALIDARG;
   const auto result = SDL_SeekIO(stream, offset, static_cast<SDL_IOWhence>(origin));
   if (result < 0)
      return E_FAIL;
   if (position)
      *position = static_cast<UInt64>(result);
   return S_OK;
}
class Output final : public ISequentialOutStream, public CMyUnknownImp
{
   Z7_COM_UNKNOWN_IMP_1(ISequentialOutStream)
   Z7_IFACE_COM7_IMP(ISequentialOutStream)
public:
   std::vector<std::uint8_t> bytes;
   Cancel cancel;
   bool oversized = false;
};
Z7_COM7F_IMF(Output::Write(const void* data, UInt32 size, UInt32* processed))
{
   if (processed)
      *processed = 0;
   if (canceled(cancel))
      return E_ABORT;
   if (size > max_encoded_bytes - bytes.size())
   {
      oversized = true;
      return E_ABORT;
   }
   try
   {
      auto* begin = static_cast<const std::uint8_t*>(data);
      bytes.insert(bytes.end(), begin, begin + size);
   }
   catch (...)
   {
      return E_OUTOFMEMORY;
   }
   if (processed)
      *processed = size;
   return S_OK;
}
class Extract final : public IArchiveExtractCallback, public CMyUnknownImp
{
   Z7_COM_UNKNOWN_IMP_2(IArchiveExtractCallback, IProgress)
   Z7_IFACE_COM7_IMP(IArchiveExtractCallback)
   Z7_IFACE_COM7_IMP(IProgress)
public:
   CMyComPtr<ISequentialOutStream> output;
   Cancel cancel;
   UInt32 target = 0;
   Int32 result = NArchive::NExtract::NOperationResult::kOK;
};
Z7_COM7F_IMF(Extract::SetTotal(UInt64))
{
   return canceled(cancel) ? E_ABORT : S_OK;
}
Z7_COM7F_IMF(Extract::SetCompleted(const UInt64*))
{
   return canceled(cancel) ? E_ABORT : S_OK;
}
Z7_COM7F_IMF(Extract::GetStream(UInt32 index, ISequentialOutStream** stream, Int32 mode))
{
   *stream = nullptr;
   if (canceled(cancel))
      return E_ABORT;
   if (index == target && mode == NArchive::NExtract::NAskMode::kExtract)
   {
      *stream = output;
      (*stream)->AddRef();
   }
   return S_OK;
}
Z7_COM7F_IMF(Extract::PrepareOperation(Int32))
{
   return canceled(cancel) ? E_ABORT : S_OK;
}
Z7_COM7F_IMF(Extract::SetOperationResult(Int32 value))
{
   if (value != NArchive::NExtract::NOperationResult::kOK)
      result = value;
   return canceled(cancel) ? E_ABORT : S_OK;
}
}  // namespace
struct Archive::Impl
{
   CMyComPtr<IInArchive> archive;
   CMyComPtr<IInStream> stream;
   Input* input = nullptr;
   std::string source, format;
   void close()
   {
      if (archive)
         archive->Close();
      archive.Release();
      stream.Release();
      input = nullptr;
      source.clear();
   }
   ~Impl()
   {
      close();
   }
   Result<void> open(const std::string& path, const Cancel& cancel)
   {
      close();
      auto* io = SDL_IOFromFile(path.c_str(), "rb");
      if (!io)
         return std::unexpected(std::string(SDL_GetError()));
      input = new Input(io);
      input->cancel = cancel;
      stream = input;
      std::array<unsigned char, 8> signature{};
      SDL_ReadIO(io, signature.data(), signature.size());
      if (SDL_SeekIO(io, 0, SDL_IO_SEEK_SET) < 0)
         return std::unexpected("This document provider does not support seeking. Download the book locally first.");
      Byte id = 0xee;
      format = "tar";
      if (signature[0] == 'P' && signature[1] == 'K')
      {
         id = 1;
         format = "ZIP";
      }
      else if (signature[0] == 'R' && signature[1] == 'a' && signature[2] == 'r' && signature[3] == '!')
      {
         id = signature[6] == 1 ? 0xcc : 3;
         format = id == 3 ? "RAR4" : "RAR5";
      }
      else if (signature[0] == '7' && signature[1] == 'z')
      {
         id = 7;
         format = "7z";
      }
      GUID clsid{0x23170f69, 0x40c1, 0x278a, {0x10, 0, 0, 1, 0x10, id, 0, 0}};
      if (CreateObject(&clsid, &IID_IInArchive, reinterpret_cast<void**>(&archive)) != S_OK)
         return std::unexpected("Archive decoder is unavailable.");
      const UInt64 check = 0;
      const auto status = archive->Open(stream, &check, nullptr);
      if (status != S_OK)
      {
         close();
         return std::unexpected(canceled(cancel) ? "Canceled" : "Cannot open this archive. It may be damaged, encrypted, or unsupported.");
      }
      source = path;
      return {};
   }
};
Archive::Archive() : impl_(std::make_unique<Impl>())
{
}
Archive::~Archive() = default;
void Archive::reset()
{
   impl_->close();
}
Result<Book> Archive::index(const std::string& source, const Cancel& cancel)
{
   auto opened = impl_->open(source, cancel);
   if (!opened)
      return std::unexpected(opened.error());
   UInt32 count = 0;
   if (impl_->archive->GetNumberOfItems(&count) != S_OK || count > 100000)
      return std::unexpected("Archive has too many entries or invalid metadata.");
   Book book{source, {}, display_name(source), impl_->format};
   std::optional<UInt32> metadata_entry;
   for (UInt32 i = 0; i < count; ++i)
   {
      if (canceled(cancel))
         return std::unexpected("Canceled");
      Property directory, name, encrypted;
      if (impl_->archive->GetProperty(i, kpidIsDir, &directory) != S_OK || impl_->archive->GetProperty(i, kpidPath, &name) != S_OK)
         return std::unexpected("Cannot read archive metadata.");
      if ((directory.vt == VT_BOOL && directory.boolVal) || name.vt != VT_BSTR)
         continue;
      auto filename = utf8(name.bstrVal);
      std::replace(filename.begin(), filename.end(), '\\', '/');
      const auto leaf = filename.substr(filename.find_last_of('/') == filename.npos ? 0 : filename.find_last_of('/') + 1);
      if (!natural_less(leaf, "ComicInfo.xml") && !natural_less("ComicInfo.xml", leaf))
         metadata_entry = i;
      else
      {
         auto folded = leaf;
         for (auto& c : folded)
            if (c >= 'A' && c <= 'Z')
               c += 'a' - 'A';
         if (folded == "comicinfo.xml")
            metadata_entry = i;
      }
      if (!is_image(filename) || filename.find("__MACOSX/") != filename.npos)
         continue;
      if (impl_->archive->GetProperty(i, kpidEncrypted, &encrypted) == S_OK && encrypted.vt == VT_BOOL && encrypted.boolVal)
         return std::unexpected("Encrypted comics are not supported.");
      book.pages.push_back({std::move(filename), i});
   }
   if (book.pages.empty())
      return std::unexpected("No supported images in this archive (JPEG, PNG, BMP or GIF).");
   std::stable_sort(book.pages.begin(), book.pages.end(), [](const Page& a, const Page& b) { return natural_less(a.name, b.name); });
   if (metadata_entry)
   {
      Property size;
      if (impl_->archive->GetProperty(*metadata_entry, kpidSize, &size) == S_OK &&
          ((size.vt == VT_UI8 && size.uhVal.QuadPart <= 1024 * 1024) || (size.vt == VT_UI4 && size.ulVal <= 1024 * 1024)))
      {
         Book entry{source, {{"ComicInfo.xml", *metadata_entry}}, {}, {}};
         if (auto bytes = read(entry, 0, cancel); bytes && bytes->size() <= 1024 * 1024)
         {
            book.metadata = parse_comic_info({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
            if (auto title = metadata_title(book.metadata); !title.empty())
               book.title = std::move(title);
         }
      }
   }
   return book;
}
Result<std::vector<std::uint8_t>> Archive::read(const Book& book, std::size_t page, const Cancel& cancel)
{
   if (page >= book.pages.size())
      return std::unexpected("Page is out of range.");
   if (impl_->source != book.source)
   {
      auto opened = impl_->open(book.source, cancel);
      if (!opened)
         return std::unexpected(opened.error());
   }
   impl_->input->cancel = cancel;
   const auto index = static_cast<UInt32>(book.pages[page].entry);
   Property size;
   if (impl_->archive->GetProperty(index, kpidSize, &size) != S_OK)
      return std::unexpected("Cannot read page metadata.");
   const UInt64 length = size.vt == VT_UI8 ? size.uhVal.QuadPart : size.vt == VT_UI4 ? size.ulVal : 0;
   if (length > max_encoded_bytes)
      return std::unexpected("Page exceeds the 64 MiB encoded-image limit.");
   auto* output = new Output;
   output->cancel = cancel;
   CMyComPtr<ISequentialOutStream> owner = output;
   output->bytes.reserve(static_cast<std::size_t>(length));
   auto* callback = new Extract;
   callback->cancel = cancel;
   callback->target = index;
   callback->output = owner;
   CMyComPtr<IArchiveExtractCallback> callback_owner = callback;
   const auto status = impl_->archive->Extract(&index, 1, 0, callback_owner);
   if (canceled(cancel))
   {
      impl_->close();
      return std::unexpected("Canceled");
   }
   if (output->oversized)
      return std::unexpected("Page exceeds the 64 MiB encoded-image limit.");
   if (status != S_OK || callback->result != NArchive::NExtract::NOperationResult::kOK)
      return std::unexpected("Cannot extract this page. The archive may be damaged or use an unsupported compression method.");
   return std::move(output->bytes);
}
}  // namespace oneam
