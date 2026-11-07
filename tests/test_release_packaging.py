import hashlib
import importlib.util
import pathlib
import stat
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('package_release', pathlib.Path(__file__).parents[1] / 'scripts/package_release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
version_spec = importlib.util.spec_from_file_location('find_release_build', pathlib.Path(__file__).parents[1] / 'scripts/find_release_build.py')
version_module = importlib.util.module_from_spec(version_spec)
version_spec.loader.exec_module(version_module)


class ReleasePackaging(unittest.TestCase):
    def test_project_version(self):
        self.assertEqual(version_module.project_version('cmake_minimum_required(VERSION 3.22)\nproject(oneam VERSION 2.0.0 LANGUAGES C CXX)'), '2.0.0')

    def test_complete_release(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            artifacts, output = root / 'artifacts', root / 'release'
            for target in release.DESKTOP:
                directory = artifacts / f'oneam-{target}'
                directory.mkdir(parents=True)
                (directory / ('oneam.exe' if target.startswith('Windows') else 'oneam')).write_bytes(b'executable')
                (directory / 'oneam-archive-source.zip').write_bytes(b'source')
            for abi in release.ANDROID:
                directory = artifacts / f'oneam-Android-{abi}-debug' / 'android/app/build/outputs/apk/debug'
                directory.mkdir(parents=True)
                (directory / 'app-debug.apk').write_bytes(b'apk')
            web = artifacts / 'oneam-WebAssembly'
            web.mkdir()
            for name in ('index.html', 'oneam.js', 'oneam.wasm', 'oneam_archive.wasm'):
                (web / name).write_bytes(b'web')
            assets = release.package(artifacts, output, 'v2.0.0')
            self.assertEqual(len(assets), 10)
            for line in (output / 'SHA256SUMS.txt').read_text().splitlines():
                checksum, name = line.split('  ')
                self.assertEqual(checksum, hashlib.sha256((output / name).read_bytes()).hexdigest())
            with zipfile.ZipFile(output / 'oneam-2.0.0-Linux-x64.zip') as archive:
                entry = archive.getinfo('oneam-2.0.0-Linux-x64/oneam')
                self.assertTrue(stat.S_IMODE(entry.external_attr >> 16) & stat.S_IXUSR)
            with self.assertRaises(ValueError):
                release.package(artifacts, output, '../invalid')
            (web / 'oneam_archive.wasm').unlink()
            with self.assertRaises(ValueError):
                release.package(artifacts, root / 'incomplete', 'v2.0.0')


if __name__ == '__main__':
    unittest.main()
