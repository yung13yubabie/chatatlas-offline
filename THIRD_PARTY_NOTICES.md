# Third-party components

ChatAtlas project source is licensed under the existing [MIT license](LICENSE).
A Windows executable also contains compiler/runtime code; it is **not described as an MIT-only binary**.

| Component | Version / origin | Use | Notices |
| --- | --- | --- | --- |
| nlohmann/json | 3.12.0, upstream release `include.zip` | Vendored C++ JSON headers | `native/vendor/LICENSE-nlohmann.txt`, `licenses/nlohmann-headers-notices.txt` |
| Hedley | Upstream copy shipped with nlohmann/json 3.12.0 | Header portability macros | `native/vendor/LICENSE-hedley.txt` |
| Zig and compiler runtime | Zig 0.14.1 official distribution | Build tool and compiler-generated runtime | `licenses/Zig-MIT.txt` |
| LLVM libc++, libc++abi, libunwind | Bundled by Zig 0.14.1 | Statically linked C++ runtime components | `licenses/LLVM-*.txt` |
| MinGW-w64 / winpthreads | Bundled by Zig 0.14.1 | Windows runtime compatibility | `licenses/MinGW-w64-COPYING.txt`, `licenses/Runtime-source-notices.txt` |
| SQLite3MultipleCiphers | 2.6.1, SQLite 3.54.0 upstream amalgamation | Statically linked, wxSQLite3-compatible AES-128-CBC for the read-only LINE source | `licenses/SQLite3MultipleCiphers.txt`; upstream MIT and retained cryptographic notices; SQLite public domain |
| linejs | evex-dev/linejs commit `4fc512ab8c19a2490ca73bb71777beb775ca3ae3` | Protocol reference and adapted native OBS token / LEGY implementation; no JavaScript runtime is used | MIT; `licenses/linejs-MIT.txt` |
| Windows system DLLs / UCRT / SQLite | User's Windows installation | Dynamically loaded operating-system components | Not redistributed in this package |
| Python | 3.11+ | Legacy implementation and tests only | Not bundled in native executable |
| Playwright / playwright-core | 1.62.1 | Development/browser tests only | Apache-2.0; package lock pins dependency bytes |
| fsevents | 2.3.2, optional macOS-only | Development file watcher only | MIT; install scripts disabled by documented npm command |

The LINE cipher source is downloaded and checksum-verified only during development builds.
Its C object is linked into the executable; users do not install a cipher package or DLL.
The LINE connection explicitly selects wxSQLite3-compatible AES-128-CBC without
falling back to another database cipher. Upstream ChaCha20 support is also compiled
for the secure-memory allocator’s random generator. The retained notices cover the
AES, MD5, SHA-1, SHA-2, Sodium, PBKDF2 and related upstream sources used by this build.
SQLite3MultipleCiphers does not replace the Windows system SQLite used for
ChatAtlas’s own collection database.

Runtime-source-notices contains a conservative set of notices from runtime source inputs
observed during a Zig 0.14.1 build, not a promise that every listed object survives linking.
Third-party notices are preserved unchanged where possible.

## Native LINE protocol references

The native OBS token and LEGY implementation follows the protocol shapes in
[evex-dev/linejs at the pinned commit](https://github.com/evex-dev/linejs/tree/4fc512ab8c19a2490ca73bb71777beb775ca3ae3),
including compact Thrift token requests, JWT handling, AES/RSA envelope formatting
and response parsing. The upstream MIT notice is retained in `licenses/linejs-MIT.txt`.
Only the needed protocol behavior is implemented in C++; linejs and its JavaScript
dependencies are not installed or executed by ChatAtlas.

OBS token replies containing a record separator follow the convention documented
in [CHRLINE](https://github.com/DeachSword/CHRLINE). A protocol reference is not a
claim of endorsement, current LINE compatibility or successful live-account testing.

## Recovered dependency provenance

The old source ZIP omitted three files under `native/vendor/nlohmann/detail/output/`.
All 43 retained headers matched the official 3.12.0 include archive byte-for-byte.
The three missing headers were restored from that archive; no vendor code was modified.

- [Official nlohmann/json 3.12.0 release](https://github.com/nlohmann/json/releases/tag/v3.12.0)
- include.zip SHA-256: `b8cb0ef2dd7f57f18933997c9934bb1fa962594f701cd5a8d3c2c80541559372`
- [Official SQLite3MultipleCiphers 2.6.1 release](https://github.com/utelle/SQLite3MultipleCiphers/releases/tag/v2.6.1)
- SQLite3MultipleCiphers source commit: `45d4ed3487bc02cb08ac73afff47c031c3f88ee0`
- `sqlite3mc-2.6.1-sqlite-3.54.0-amalgamation.zip` SHA-256: `3f25af603566a23c821d8e581778b5ece2a541d2435776de57b8d845c176815a`
- [Official Zig release index](https://ziglang.org/download/index.json)
- Linux x64 Zig 0.14.1 archive SHA-256: `24aeeec8af16c381934a6cd7d95c807a8cb2cf7df9fa40d359aa884195c4716c`

An unrelated Power Grid Model license file in the old archive was removed after
the vendored headers were verified against nlohmann's upstream release. No Power
Grid Model code or dependency is used.

Dependency inventory and advisory lookups are not a complete security audit or a
guarantee that no vulnerabilities exist. The prerelease validation gaps are recorded
in [STATUS](docs/STATUS.md).
