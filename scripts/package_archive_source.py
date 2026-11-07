"""Ship the exact shared archive-library source and its standalone build recipe."""
import pathlib
import sys
import zipfile

source = pathlib.Path(sys.argv[1]).resolve()
destination = pathlib.Path(sys.argv[2]).resolve()
root = pathlib.Path(__file__).resolve().parents[1]
if not (source / "CPP/7zip/Archive/IArchive.h").is_file():
    raise SystemExit("Expected the pinned 7-Zip source directory")
destination.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(source.rglob("*")):
        if file.is_file() and ".git" not in file.parts:
            archive.write(file, "7zip/" + file.relative_to(source).as_posix())
    archive.write(root / "cmake/sevenzip.cmake", "cmake/sevenzip.cmake")
    archive.write(root / "cmake/sevenzip.def", "cmake/sevenzip.def")
    archive.write(root / "assets/licenses/vcpkg.txt", "cmake/LICENSE.txt")
    archive.writestr("CMakeLists.txt", '''cmake_minimum_required(VERSION 3.22)
project(oneam_archive LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(sevenzip_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/7zip)
set(USE_NO_ASM ON CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
  add_compile_options(-pthread -fexceptions)
endif()
include(cmake/sevenzip.cmake)
set_target_properties(7zip PROPERTIES OUTPUT_NAME oneam_archive)
if(EMSCRIPTEN)
  set_target_properties(7zip PROPERTIES PREFIX "" SUFFIX ".wasm")
  target_link_options(7zip PRIVATE -pthread -fexceptions -sSIDE_MODULE=1)
endif()
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(7zip PRIVATE Threads::Threads)
endif()
''')
    archive.writestr("BUILDING.txt", '''7-Zip archive backend source for oneam

Source revision: 9128b80e3a471108678e2b3ec3c985861c8d7b0f (7-Zip 26.04).
Build recipe adapted from Microsoft vcpkg revision
6fdd23fa0a9479a9f9a1c48178696f60cf39255d, ports/7zip/CMakeLists.txt.
No changes are made to the 7-Zip sources.

To rebuild the shared library:
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --config Release --target 7zip

The library is named oneam_archive.dll, liboneam_archive.so, or
liboneam_archive.dylib. For Android, configure with the NDK toolchain,
ANDROID_ABI=arm64-v8a and ANDROID_PLATFORM=android-26.

See 7zip/DOC/License.txt, copying.txt and unRarLicense.txt for the licenses.
The application dynamically links this library; a compatible rebuilt library
can replace the bundled one. Android APKs must be repackaged and signed after
replacement. No user-installed 7-Zip executable or plugin is required.

For the browser build, use Emscripten 6.0.1:
  emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
  cmake --build build-web --target 7zip
Replace oneam_archive.wasm beside oneam.js with the rebuilt module. The module
uses the browser reader's shared memory and exported runtime interfaces.
''')
print(destination)
