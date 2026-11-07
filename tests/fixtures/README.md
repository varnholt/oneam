# Archive fixtures

`pages.zip`, `pages.7z`, and `pages.tar` contain synthetic 2x3 BMPs, a text entry,
and an ignored macOS metadata entry. Their pixel colors distinguish page 2 from
page 10 when testing filtering, ordering and extraction. They contain no comic art.

`rar4.uu` and `rar5.uu` are the small reference fixtures
`test_read_format_rar.rar.uu` and `test_read_format_rar5_stored.rar.uu` from libarchive
revision `03159acce4617eb244e0b86f70cfe884ebce49ad`.
They contain text used for decoder interoperability tests.
See `LIBARCHIVE-LICENSE.txt` for the upstream license.
