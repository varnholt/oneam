"""Turn verified Actions artifacts into versioned, checksummed release downloads."""
import argparse
import hashlib
import pathlib
import re
import stat
import zipfile

DESKTOP = ('Windows-x64', 'Windows-arm64', 'Linux-x64', 'Linux-arm64', 'macOS-x64', 'macOS-arm64')
ANDROID = ('arm64-v8a', 'x86_64')


def archive_tree(source, destination, prefix):
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(source.rglob('*')):
            if not path.is_file():
                continue
            relative = path.relative_to(source)
            info = zipfile.ZipInfo.from_file(path, str(pathlib.PurePosixPath(prefix) / relative.as_posix()))
            executable = path.name == 'oneam' or path.suffix in ('.so', '.dylib')
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | (0o755 if executable else 0o644)) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes(), compresslevel=9)


def package(root, output, tag):
    if not re.fullmatch(r'v\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?', tag):
        raise ValueError('Use a version tag such as v2.0.0 or v2.0.0-beta.1')
    version = tag[1:]
    expected = [f'oneam-{target}' for target in DESKTOP]
    expected += [f'oneam-Android-{abi}-debug' for abi in ANDROID]
    expected += ['oneam-WebAssembly']
    for name in expected:
        if not (root / name).is_dir():
            raise ValueError(f'Missing verified artifact: {name}')
    for target in DESKTOP:
        entry = 'oneam.exe' if target.startswith('Windows') else 'oneam'
        if not (root / f'oneam-{target}' / entry).is_file():
            raise ValueError(f'Missing executable in {target}')
    for abi in ANDROID:
        if len(list((root / f'oneam-Android-{abi}-debug').rglob('*.apk'))) != 1:
            raise ValueError(f'Expected exactly one Android {abi} APK')
    web = root / 'oneam-WebAssembly'
    if not all((web / name).is_file() for name in ('index.html', 'oneam.js', 'oneam.wasm', 'oneam_archive.wasm')):
        raise ValueError('WebAssembly package is incomplete')
    source = list(root.rglob('oneam-archive-source.zip'))
    if not source:
        raise ValueError('Missing archive-library source bundle')
    output.mkdir(parents=True, exist_ok=True)
    for target in DESKTOP:
        name = f'oneam-{version}-{target}'
        archive_tree(root / f'oneam-{target}', output / f'{name}.zip', name)
    for abi in ANDROID:
        apk = next((root / f'oneam-Android-{abi}-debug').rglob('*.apk'))
        (output / f'oneam-{version}-Android-{abi}-debug.apk').write_bytes(apk.read_bytes())
    archive_tree(web, output / f'oneam-{version}-WebAssembly.zip', f'oneam-{version}-WebAssembly')
    (output / f'oneam-{version}-archive-source.zip').write_bytes(source[0].read_bytes())
    assets = sorted(path for path in output.iterdir() if path.suffix in ('.zip', '.apk'))
    checksums = ''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n' for path in assets)
    (output / 'SHA256SUMS.txt').write_text(checksums, encoding='utf-8')
    notes = f'''oneam {version}

A small comic reader for Windows, Linux, macOS, Android and the browser.

- Separate reading preferences for each book: layout, zoom, rotation, trimming, fit mode and image adjustments.
- Page thumbnails, direct page navigation, spreads, manga mode and continuous scrolling.
- Bookmarks, library search and progress filters, ComicInfo metadata and next-book navigation.
- Antialiased scaling and mouse/touch controls, with correct page proportions.

[Try in your browser](https://varnholt.github.io/oneam/) · [Reading guide](https://github.com/varnholt/oneam#reading)

Choose x64 or ARM64 for your desktop. Extract its ZIP and keep the files together.
Android downloads are debug-signed APKs for sideloading; x86_64 is primarily for emulators.
The WebAssembly ZIP is for self-hosting and includes a replaceable archive-decoder module.
SHA256SUMS.txt contains checksums for every download. Matching archive-library source accompanies the binaries.
'''
    (output / 'notes.md').write_text(notes, encoding='utf-8')
    return assets


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('artifacts', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('tag')
    args = parser.parse_args()
    for asset in package(args.artifacts, args.output, args.tag):
        print(asset.name)
