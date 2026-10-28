# Baseline build provenance — ChatAtlas 0.3.0

Checked 2026-10-28. The supplied Windows executable was not executed.

## Inputs

- Original package: `ChatAtlas-0.3.0-Windows-x64.zip`, Library version 2; SHA-256 `58a94cd943eeef5c010da1e35debdecfc7881b4802d6d88cfb5fe33e12e6d797`.
- Embedded source ZIP SHA-256: `a5bd50e687e477fb4d99bcedbe139e36d361d481298bdd08f4b155404758be3f`.
- Original executable SHA-256: `c120f2f7bf988798f1ba8495448338dfbdacbe3f455431959b19a30163dbf520`.
- Toolchain: Zig 0.14.1, official Linux x86-64 archive SHA-256 `24aeeec8af16c381934a6cd7d95c807a8cb2cf7df9fa40d359aa884195c4716c`, verified against <https://ziglang.org/download/index.json>.
- Headers: nlohmann/json v3.12.0 official `include.zip`, SHA-256 `b8cb0ef2dd7f57f18933997c9934bb1fa962594f701cd5a8d3c2c80541559372`, verified against <https://github.com/nlohmann/json/releases/tag/v3.12.0>.

## Incomplete bundled source and repair

The source ZIP omitted `native/vendor/nlohmann/detail/output/` entirely.
All 43 existing vendor headers exactly matched the official v3.12.0 `include/nlohmann/` tree. Three missing files were restored, without replacing any existing source:

- `detail/output/binary_writer.hpp`
- `detail/output/output_adapters.hpp`
- `detail/output/serializer.hpp`

## Rebuild comparison

Running the original `native/build.mjs` against the repaired copy succeeded.
Rebuilt executable SHA-256: `1d7279aa23265dc91c00ba574b1bc4d8b5509b13ef45ae1ea821af6aff569abc`.
Both files are 1,648,640 bytes. This is **not** a byte-identical reproducible build.

Every byte of `.text`, `.rdata`, `.data`, `.pdata`, `.tls`, `.rsrc`, and `.reloc` matches. Exactly 20 bytes differ, all in build-identification metadata: the COFF reproducible-build hash, two debug-directory timestamps, and eight bytes of the CodeView GUID. After normalizing only these specific metadata fields, the complete files match. This provides strong code/resource correspondence evidence, not proof of runtime behavior or overall security.

The final public candidate is a separate version and must be rebuilt from its own final source tree. These baseline hashes must not be used to describe a later binary.

## Limits

No Windows GUI, real LINE capture, real profile, or multigigabyte Windows save test was performed. A successful cross-build does not establish these results.

Runtime license files and an inclusive source-notice appendix were prepared from the verified toolchain. The appendix contains unchanged copyright/license blocks from runtime build inputs, not an exact assertion of which archive members the linker retained.
