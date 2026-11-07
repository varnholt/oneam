# oneam

A small comic reader for Windows, Linux, macOS and Android. The reader uses C++23,
SDL3 and one 7-Zip archive backend, both built from pinned source. Qt and bit7z
are removed; no user-installed archive utility or plugin is required.

Open a book, turn the page, and return to the same place next time. Reading and
image decoding run in the background. Adjacent pages are prefetched into a bounded
cache, and resizing reuses the current image instead of extracting it again.
Reduced pages use background Mitchell filtering in linear light to prevent jagged
text and line art. The original pixels remain available for zooming, and both
rendering axes always use the same scale to preserve the page's aspect ratio.

## Reading

- Open **CBZ/ZIP, CBR/RAR, CB7/7z or CBT/tar** through **Open**, or drop a book onto
  the desktop window. Archive contents are read in memory, never unpacked onto disk.
- JPEG, PNG, BMP and GIF pages are supported. Animated GIFs display their first frame.
- Pages sort naturally: `page2` precedes `page10`, including uppercase extensions.
- The shelf shows recently opened books. Desktop **Folder** adds books from a
  directory and remembers that folder; the scan and visible cover loading happen
  in the background.
- Reading position is saved after a page loads. Settings live in the operating
  system's per-user application data directory, independently of the working directory.

| Action | Desktop | Touch |
|---|---|---|
| Open a book | Open / Ctrl+O | Open |
| Next / previous page | Bottom buttons; Right / Left, Page Down / Up, Space | Bottom buttons, swipe or tap a page edge |
| First / last page | Menu; Home / End | Menu |
| Scroll / pan | Wheel, Up / Down, drag | Drag |
| Zoom | Menu; Ctrl+wheel, + / - | Menu or pinch |
| Reset zoom | Menu; 0 | Menu |
| Fit page / fit width | Menu; W | Menu |
| Hide / show bars | Read, Menu or Tab; click the page center | Menu or tap the page center |
| Shelf | Library or Escape | Library or Android Back |
| Select a shelf book | Arrow keys, Enter | Tap a cover |
| Fullscreen | Menu; F11 | Menu |
| Help | Menu's help button; F1 | Menu's help button |

All keyboard commands have visible mouse and touch equivalents. The shared
controls menu scrolls on small screens, and its help and close buttons stay fixed.
A floating **Controls** button stays available when the bars are hidden. Escape
or Android Back restores the bars before returning to the shelf.
Buttons use vector icons, clear disabled/active/hover states and touch targets
of at least 48 logical pixels. No icon fonts or image libraries are required.

## Desktop build

Requires CMake 3.22+, a C/C++ toolchain with C++23 `std::expected`, and internet
access on the first configure. MSVC 2022, GCC 14, or Clang with a recent libc++ or
libstdc++ are suitable. Dependency source revisions are pinned in `CMakeLists.txt`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix artifacts/oneam
```

Run `artifacts/oneam/oneam` (Windows: `oneam.exe`) and keep the packaged files together,
including `assets/` and the `oneam_archive` shared library.
On Linux, install the development packages for X11 or Wayland and OpenGL first;
the CI workflow lists the Ubuntu packages.

## Android APK

The same C++ reader is built as an SDL Android application. It uses the system
document picker and `content://` streams; broad storage access is not required.
Read permissions are persisted when the document provider supports it. A provider
that revokes permission requires choosing the book again.

Requires JDK 21, SDK platform 36, NDK `27.1.12297006`, and Android CMake `3.22.1`.
Set `ANDROID_HOME` to the SDK directory, then:

```sh
python scripts/prepare_android.py
cd android
./gradlew assembleDebug
```

On Windows use `gradlew.bat`. The checksum-verified wrapper downloads Gradle 8.13.
The installable APK is `android/app/build/outputs/apk/debug/app-debug.apk`.
The default architecture is ARM64; add `-PoneamAbis=arm64-v8a,x86_64` for an APK
that also runs on an x86_64 emulator. Minimum Android version: 8.0 (API 26).

The GitHub Actions workflow builds desktop packages and uploads an
`oneam-android-arm64-debug` APK artifact on pushes, pull requests and manual runs.
Debug APKs use optimized native decoding and are suitable for testing and sideloading. Release signing is deliberately
left to the owner; no release credentials belong in the repository.

## Limits and verification

The decoded page cache is capped at 128 MiB, with separate bounded cover textures.
Individual encoded images are limited to 64 MiB and decoded RGBA images to 96 MiB
and 16384 pixels per dimension. Current displayed images and temporary decoder
allocations are additional to the cache budget. Oversized or damaged pages report
an error instead of attempting an unbounded allocation.

Password-protected books, multipart RAR archives, WebP/AVIF/TIFF pages and PDF are
outside the supported format set. Solid RAR/7z archives can require substantial
decompression for a distant page; this remains background work. Providers that
cannot seek may not support archive formats requiring random access.

The regression suite covers ZIP/7z/tar reads, RAR4/RAR5 interoperability,
ordering, corrupt input, cache eviction, canceled requests, fast book switching,
and persistence. See [the audit](docs/AUDIT.md) for the original defects and fixes.

## Dependencies

There are two compiled third-party libraries: SDL3 for windows, rendering, input
and native file dialogs, and 7-Zip for all archive formats and compression. There
are no separate zlib, liblzma, libarchive or UnRAR dependencies. Image and font
decoding use the `stb_image` and `stb_truetype` headers. Noto Sans is bundled under
the SIL Open Font License. The font was
taken from google/fonts revision `86aced91de05cd4f96a06838124cca45be337b2b`.
Third-party license notices ship in `assets/licenses/` and `assets/OFL.txt`.
The 7-Zip backend uses its normal library interface and a small stream adapter;
it is linked normally, without runtime library-path guessing or shell commands.
Its CMake recipe is adapted from Microsoft vcpkg. Matching decoder source and
a standalone rebuild recipe accompany CI artifacts as `oneam-archive-source.zip`.
