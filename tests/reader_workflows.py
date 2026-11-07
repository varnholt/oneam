"""Exercise the real application with generated, redistributable comic fixtures."""
import argparse
import os
import pathlib
import struct
import subprocess
import tempfile
import zipfile


def page(width=320, height=480, color=80):
    row = (width * 3 + 3) & ~3
    pixels = bytearray(row * height)
    for y in range(height):
        for x in range(width):
            value = 255 if x < 24 or x >= width - 24 or y < 24 or y >= height - 24 else color
            pixels[y * row + x * 3:y * row + x * 3 + 3] = bytes((value, value, value))
    header = struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54)
    header += struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, len(pixels), 0, 0, 0, 0)
    return header + pixels


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    args = parser.parse_args()
    executable = pathlib.Path(args.executable).resolve()
    with tempfile.TemporaryDirectory(prefix='oneam-workflows-') as directory:
        root = pathlib.Path(directory)
        for number in (1, 2):
            with zipfile.ZipFile(root / f'Night Tales {number:02}.cbz', 'w') as archive:
                archive.writestr('ComicInfo.xml', f'<ComicInfo><Series>Night Tales</Series><Number>{number}</Number><Title>After midnight</Title><Writer>Example Author</Writer></ComicInfo>')
                for index in range(6):
                    archive.writestr(f'page{index+1}.bmp', page(color=40+index*25))
        scenarios = [
            ('goto', 1, 'single,goto:4,pages', 4),
            ('spread', 1, 'spread,next,next,previous,manga', 2),
            ('continuous-forward', 1, 'continuous,scroll:2000', 2),
            ('continuous-backward', 2, 'continuous,scroll:-60', 1),
            ('images', 3, 'single,trim,rotate,brightness,contrast,gamma,reset-image', 3),
            ('bookmarks', 2, 'single,bookmark,bookmarks', 2),
            ('library', 2, 'single,library,mark-read,filter-read,sort-title,search:Night', 2),
            ('metadata', 1, 'single,info', 1),
            ('next-book', 1, 'single,auto-next,last,next', 1),
        ]
        env = os.environ | {'SDL_VIDEODRIVER': 'dummy', 'SDL_RENDER_DRIVER': 'software'}
        for name, start, actions, expected in scenarios:
            command = [str(executable), '--smoke', str(root / f'{name}.bmp'),
                       '--smoke-library', str(root), '--smoke-page', str(start),
                       '--smoke-actions', actions, '--smoke-expect-page', str(expected),
                       '--size', '800', '800', str(root / 'Night Tales 01.cbz')]
            result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=45)
            if result.returncode:
                raise RuntimeError(f'{name} failed ({result.returncode}):\n{result.stdout}\n{result.stderr}')
            if not (root / f'{name}.bmp').exists():
                raise RuntimeError(f'{name} did not render a screenshot')
            output = result.stdout + result.stderr
            if name == 'next-book' and 'Night Tales #2' not in output:
                raise RuntimeError('automatic next book did not open the second archive')
            print(f'{name}: passed', flush=True)


if __name__ == '__main__':
    main()
