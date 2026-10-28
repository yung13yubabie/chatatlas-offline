"""Review exact staged public bytes; do not print suspected secret contents."""
import hashlib
import re
import subprocess
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
ALLOW_SUFFIX = {".py", ".ps1", ".cmd", ".js", ".cjs", ".css", ".html", ".md",
                ".toml", ".yml", ".yaml", ".cpp", ".hpp", ".h", ".mjs",
                ".rc", ".manifest", ".txt"}
ALLOW_NAMES = {"LICENSE", "VERSION", ".gitignore", ".gitattributes", "Makefile"}
ALLOW_JSON = {"package.json", "package-lock.json"}
TOP_DIRS = {".github", "docs", "licenses", "native", "src", "tests", "tools"}
PRIVATE_PARTS = {"private", "archives", "captures", "profiles.local", "output", "dist",
                 "build", "out", "node_modules", ".venv", ".venv-build", ".toolchains"}
PATTERNS = [re.compile(p, re.I) for p in [
    r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----",
    r"gh[pousr]_[A-Za-z0-9]{30,}",
    r"github_pat_[A-Za-z0-9_]{30,}",
    r"sk-[A-Za-z0-9_-]{24,}",
    r"[A-Z0-9._%+-]+@(?!example\.(?:com|invalid)\b|users\.noreply\.github\.com\b)[A-Z0-9.-]+\.[A-Z]{2,}",
    r"[A-Z]:\\Users\\[^\\\s]+\\",
    r"/(?:Users|home|workspace)/[A-Za-z0-9_.-]+/",
    r"AKIA[0-9A-Z]{16}",
    r"xox[baprs]-[A-Za-z0-9-]{20,}",
    r"AIza[0-9A-Za-z_-]{35}",
]]
# Only email-pattern exemptions, restricted to these exact public upstream bytes.
PUBLIC_COPYRIGHT_HASHES = {
    "licenses/nlohmann-headers-notices.txt": "8df2f05fb2c7466803b046b37f729073c1843674ffe2383c61f07e0c60bbcde7",
    "native/vendor/nlohmann/detail/output/serializer.hpp": "4c079acb8f6936c4f787fd5706a52c77a1751dd34787a821705845698ec8540e",
    "native/vendor/nlohmann/thirdparty/hedley/hedley.hpp": "1547b367895f8be9f8283ae21f583182db572ad990236220002b632a15238712",
    "licenses/LLVM-libcxx.txt": "539dd7aed86e8a4f12cbdd0e6c50c189c7d74847e4fecc64ce2c6ee3a01da38b",
    "licenses/LLVM-libcxxabi.txt": "e2b35be49f7284a45b7baca8fc7b3ab7440e7902392b2528a457816b5bb2a15c",
    "licenses/LLVM-libunwind.txt": "b5efebcaca80879234098e52d1725e6d9eb8fb96a19fce625d39184b705f7b6d",
    "licenses/MinGW-w64-COPYING.txt": "99a69660981156c21336fdb5661f89341b013c94e4bf9e1c7467b4745718397f",
    "licenses/Runtime-source-notices.txt": "aab1e1465b8d2e2a71bc5d44fd4254801c7cd456be0db56f247ca73de8540212",
    "licenses/Zig-MIT.txt": "5c537d6853e005298a285d508cff9ac7192cea23576c840d485b2b586a7ff177",
}

def staged_files():
    data = subprocess.run(["git", "ls-files", "--stage", "-z"], cwd=ROOT,
                          capture_output=True, check=True).stdout
    result = {}
    for row in data.split(b"\0"):
        if not row:
            continue
        meta, raw_name = row.split(b"\t", 1)
        mode, _, stage = meta.decode("ascii").split()
        name = raw_name.decode("utf-8")
        if mode not in {"100644", "100755"} or stage != "0":
            raise ValueError("Non-regular or unmerged staged file: " + name)
        content = subprocess.run(["git", "show", ":" + name], cwd=ROOT,
                                 capture_output=True, check=True).stdout
        result[name] = content
    return result

def review(tracked, require_worktree=True):
    failures = []
    if not tracked:
        return [("", "no staged source files")]
    for name, content in tracked.items():
        path = PurePosixPath(name)
        vendor_output = name.startswith("native/vendor/nlohmann/detail/output/")
        if path.is_absolute() or ".." in path.parts or "\\" in name:
            failures.append((name, "unsafe path")); continue
        if len(path.parts) > 1 and path.parts[0] not in TOP_DIRS:
            failures.append((name, "directory not approved")); continue
        if any(part.startswith(".") for part in path.parts if part not in {".github", ".gitignore", ".gitattributes"}):
            failures.append((name, "hidden file or directory")); continue
        if any(part in PRIVATE_PARTS for part in path.parts) and not vendor_output:
            failures.append((name, "private or generated directory")); continue
        if require_worktree and ((ROOT / name).is_symlink() or not (ROOT / name).is_file()
                                 or (ROOT / name).read_bytes() != content):
            failures.append((name, "working bytes differ from reviewed staged bytes")); continue
        if name == "native/src/app.ico":
            if content[:6] != b"\x00\x00\x01\x00\x01\x00" or len(content) > 65536:
                failures.append((name, "unexpected icon"))
            continue
        if path.name not in ALLOW_NAMES and name not in ALLOW_JSON and path.suffix.lower() not in ALLOW_SUFFIX:
            failures.append((name, "file type not approved")); continue
        if "local" in path.name.lower() or len(content) > 1024 * 1024:
            failures.append((name, "local configuration or oversized file")); continue
        try:
            text = content.decode("utf-8")
        except UnicodeDecodeError:
            failures.append((name, "not UTF-8 source")); continue
        if "\x00" in text:
            failures.append((name, "embedded NUL")); continue
        digest = hashlib.sha256(content).hexdigest()
        if any(pattern.search(text) for index, pattern in enumerate(PATTERNS)
               if not (index == 4 and PUBLIC_COPYRIGHT_HASHES.get(name) == digest)):
            failures.append((name, "possible private data; inspect locally"))
    required = {"LICENSE", "VERSION", "README.md", "SECURITY.md", "native/build.mjs",
                "native/vendor/nlohmann/detail/output/binary_writer.hpp",
                "native/vendor/nlohmann/detail/output/output_adapters.hpp",
                "native/vendor/nlohmann/detail/output/serializer.hpp"}
    failures.extend((name, "required public source missing") for name in sorted(required - tracked.keys()))
    return failures

def main():
    try:
        tracked = staged_files()
        failures = review(tracked)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(type(error).__name__ + ": public tree could not be reviewed.")
        return 1
    for name, reason in failures:
        print(name + ": " + reason)
    print(f"Reviewed {len(tracked)} staged files; {len(failures)} findings. Contents were not printed.")
    return bool(failures)

if __name__ == "__main__":
    raise SystemExit(main())
