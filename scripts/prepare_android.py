"""Fetch the pinned SDL Java bridge used by both Gradle and native CMake."""
import io
import pathlib
import tarfile
import urllib.request

root = pathlib.Path(__file__).resolve().parents[1]
for name, repository, revision, marker in [
    ("sdl3", "libsdl-org/SDL", "7f3ae3d57459e59943a4ecfefc8f6277ec6bf540", "android-project/app/src/main/java/org/libsdl/app/SDLActivity.java"),
    ("sevenzip", "ip7z/7zip", "9128b80e3a471108678e2b3ec3c985861c8d7b0f", "CPP/7zip/Archive/IArchive.h"),
]:
    destination = root / ".deps" / name
    if not (destination / marker).exists():
        url = f"https://codeload.github.com/{repository}/tar.gz/{revision}"
        with urllib.request.urlopen(url, timeout=90) as response:
            data = response.read()
        destination.mkdir(parents=True, exist_ok=True)
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            for member in archive.getmembers():
                components = pathlib.PurePosixPath(member.name).parts[1:]
                if not components:
                    continue
                member.name = "/".join(components)
                archive.extract(member, destination, filter="data")
    print(destination)
