"""Fetch the checksum-pinned Linux x64 build toolchain, never at app runtime."""
import hashlib
import sys
import tarfile
import urllib.request
from pathlib import Path

VERSION = "0.14.1"
NAME = "zig-x86_64-linux-" + VERSION
URL = "https://ziglang.org/download/" + VERSION + "/" + NAME + ".tar.xz"
SHA256 = "24aeeec8af16c381934a6cd7d95c807a8cb2cf7df9fa40d359aa884195c4716c"

def main():
    if sys.version_info < (3, 12):
        raise SystemExit("Python 3.12+ is required for safe archive extraction.")
    root = Path(__file__).resolve().parents[1] / ".toolchains"
    root.mkdir(exist_ok=True)
    archive = root / (NAME + ".tar.xz")
    with urllib.request.urlopen(URL, timeout=120) as response:
        payload = response.read(64 * 1024 * 1024 + 1)
    if len(payload) > 64 * 1024 * 1024 or hashlib.sha256(payload).hexdigest() != SHA256:
        raise SystemExit("Toolchain checksum mismatch; nothing extracted.")
    archive.write_bytes(payload)
    with tarfile.open(archive, "r:xz") as bundle:
        bundle.extractall(root, filter="data")
    print(root / NAME / "zig")

if __name__ == "__main__":
    main()
