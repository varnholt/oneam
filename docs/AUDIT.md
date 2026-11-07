# Reader audit and rewrite

The former Qt/qmake implementation is retained in Git history. The replacement
keeps the comic-reader workflow while removing the Qt and Windows-only bit7z build.

| Finding in the original implementation | Effect | Replacement behavior |
|---|---|---|
| `PageWidget::load` started its worker before connecting the completion signal | Fast loads could complete without updating the page | A synchronized result slot is polled by the UI |
| Page completions carried no navigation generation | A slow older load could replace the newest page | Only the current generation can publish or display results; pending requests coalesce |
| `resizeEvent` called `load` | Repeated extraction and image decoding while resizing | Resize only updates geometry and redraws the existing GPU texture |
| Image decoding and smooth rescaling occurred in the UI update slot | Large pages could block input | Decode on a worker, upload once, scale with SDL's renderer |
| `QPixmap` covers were loaded and scaled in a thread-pool worker | GUI-resource work occurred outside the GUI thread | Workers handle pixel buffers; SDL textures remain on the main thread |
| `PreviewWidget::addItem` triggered another scan when all items were present | Repeated scans and duplicate scene items | One bounded scan per folder request; thumbnails load only for visible books |
| Cache files were named using only the archive basename | Same-named comics in different folders shared metadata and covers | Persistent position keys include canonical path, file size and modification time; decoded caches are scoped to one book |
| Cached metadata was trusted and indexed with `at(1)` after splitting | Malformed or truncated metadata could throw | No on-disk archive-index cache; saved positions are validated and clamped |
| File extension checks accepted only lowercase `jpg` and `png` | Valid pages could disappear | Case-insensitive JPEG/PNG/BMP/GIF filtering |
| Page names used plain lexical ordering | `10` preceded `2` | Natural ordering without converting digit runs to fixed-width integers |
| Scene height assumed an A4 aspect ratio | Landscape and unusually tall pages had incorrect scroll bounds | Geometry uses decoded image dimensions |
| Manual archive libraries and a runtime `7z.dll` were required | Windows-specific setup and deployment failures | Pinned source dependencies and packaged assets |
| The optional `pdf2cbr.sh` helper ran `rm "$1"` after changing directories | An absolute input path could delete the original PDF | Removed the unused conversion helper and its external `rar` requirement; the reader does not modify comic archives |

## Concurrency and performance

One persistent reader worker owns the archive handle and decoded-page LRU cache.
It publishes the requested page before prefetching adjacent pages. New requests
invalidate old generations and interrupt archive reads between chunks. Decoding
has explicit size limits and runs off the UI thread; an in-progress decoder call
finishes before the worker services its next request.

The cover worker has no decoded-page cache or adjacent-page prefetch. The UI keeps
at most 48 small cover textures, and displays text placeholders while they load.
The directory scan is separate from page loading. The renderer sleeps between
unchanged frames and respects Android background lifecycle events.

The 128 MiB cache limit is not a whole-process memory limit. The displayed page,
an active decode, renderer textures, and font glyphs consume additional memory.

## Regression coverage

- ZIP, 7z and tar indexing and forward/backward/repeated page reads.
- RAR4 and RAR5 reference fixtures from libarchive, read through the 7-Zip stream adapter.
- Uppercase extensions, hidden macOS metadata, nested paths, and natural ordering.
- Missing files, invalid archives, empty books, invalid images, oversized dimensions,
  out-of-range requests, cancellation, and recovery after cancellation.
- LRU byte accounting, eviction order, replacing an entry, and oversized cache items.
- Same-name book identities, changed files, malformed saved positions, path-key validation,
  atomic position replacement, and recent-book deduplication.
- Rapid book/page requests, latest-request results, clamped resume positions, cache hits,
  and clearing the displayed result after load failures.

Generated test books and upstream archive reference fixtures supplement testing;
they do not establish compatibility with every real-world CBR variant or cloud
document provider. Physical-device testing remains useful for touch feel, memory
pressure, thermal behavior and provider-specific URI handling.

## Real collection verification, 7 November 2026

The supplied collection contains 88 books (87 CBR and one CBZ), with 4,331 indexed
pages. All 88 opened, and all 264 sampled first/middle/last pages extracted and
decoded successfully. Additionally, every page was decoded in three representative
books: a 46-page solid-RAR comic, a 50-page CBZ, and the largest archive, a 95-page
CBR of approximately 314 MB. All 191 full-read pages passed.

On the local Windows Release build, the 264-page sampling run measured median
archive indexing at 0.64 ms, median uncached extraction plus decoding at 76 ms,
95th percentile at 324 ms, and maximum at 519 ms. These are local observations,
not portable performance guarantees. Prefetched pages bypass extraction and decoding.

Windows rendering and Android API 36.1 emulator checks include a real solid-RAR
comic, document-picker URI access, touch page navigation, persisted book names,
and resuming after app restart. The emulator is not a substitute for physical-device
testing. Test comics and their images are not included in the repository or packages.

## Dependency choice

The final build uses SDL3 and one 7-Zip backend, plus two stb headers and a font.
An initial libarchive implementation failed on 30 solid-RAR books in the real
collection. Replacing that backend also removes separate zlib/liblzma build
dependencies; no fallback decoder or external extraction command is used.

The shared archive library is built from unmodified, pinned official sources
using a CMake recipe adapted from Microsoft vcpkg. Its source and standalone build
recipe are distributed alongside binaries. This keeps deployment reproducible
while allowing the library to be rebuilt and replaced independently.

## Mouse, touch and keyboard parity

Keyboard shortcuts and pointer controls share the same command dispatcher.
The persistent bottom bar provides previous/next page buttons, while the controls
menu exposes first/last page, zoom in/out/reset, both fit modes, scrolling,
library, opening files/folders, fullscreen and help. Help and close stay visible
while menu items scroll. Android does not expose the unsupported folder picker.

Icons are native vector geometry with consistent strokes and rounded caps;
they do not use ASCII characters, font glyphs or another runtime dependency.
Buttons have at least 48 logical pixels of hit area, with hover, disabled and
active states. Rendering and hit testing use the same responsive geometry.

The control regression suite verifies safe-area bounds, non-overlapping toolbar
buttons, scrolling access to every command, clipped hit testing, and keyboard
equivalents across narrow, wide and landscape layouts. Emulator interaction checks
cover last-page navigation, zoom/reset, menu scrolling, fullscreen and help.

Rotation testing exposed late Android surface invalidation after the first resize
frame. A short redraw interval after resize/focus changes keeps that transition
painted, then the renderer returns to its idle behavior. Smoke-test sessions use
separate settings so visual checks do not alter the user's reading position.
