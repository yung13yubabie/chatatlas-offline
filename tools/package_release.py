"""Build a private candidate from reviewed staged source and matched native output."""
import argparse
import hashlib
import json
import re
import sys
import tempfile
import zipfile
from pathlib import Path
from check_public_tree import ROOT, staged_files, review

def sha(data):
    return hashlib.sha256(data).hexdigest()

def write_zip(destination, files):
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, payload in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.external_attr = 0o100644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, payload)

def make_synthetic_example():
    # Only the fixed in-tree synthetic generator is used; no user directory is accepted.
    sys.path.insert(0, str(ROOT / "src"))
    from line_vault.store import Store
    from line_vault.demo import SyntheticSource
    from line_vault.collector import Collector
    from line_vault.exporter import export_html
    allow = re.compile(r"(?:index\.html|assets/[0-9a-f]{2}/[0-9a-f]{64}\.[a-z0-9]+|"
                       r"downloads/[0-9a-f]{64}/(?:info\.json|[0-9]+\.js)|"
                       r"reader/[0-9a-f]{32}/[0-9a-f]{64}-[0-9]+\.js)\Z")
    result = {}
    with tempfile.TemporaryDirectory(prefix="chatatlas-synthetic-") as directory:
        base = Path(directory)
        with Store(base) as store:
            source = SyntheticSource(1)
            Collector(store, source).run(source.list_chats())
            export_html(store)
            if store.check():
                raise ValueError("Generated artificial archive failed integrity check")
        for file in base.rglob("*"):
            if file.is_symlink():
                raise ValueError("Synthetic output contains a link")
            if file.is_file():
                name = file.relative_to(base).as_posix()
                if allow.fullmatch(name):
                    result["ChatAtlas/人工範例/" + name] = file.read_bytes()
    if "ChatAtlas/人工範例/index.html" not in result:
        raise ValueError("Synthetic reader was not generated")
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = staged_files()
    findings = review(source)
    if findings:
        raise SystemExit("Source gate failed: " + repr(findings))
    version = source["VERSION"].decode("ascii").strip()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+-alpha\.[0-9]+", version):
        raise SystemExit("This packager only emits explicitly experimental alpha candidates")
    binary_path = ROOT / "native/out/ChatAtlas.exe"
    provenance_path = ROOT / "native/out/build-provenance.json"
    if binary_path.is_symlink() or provenance_path.is_symlink():
        raise SystemExit("Build outputs must be regular files")
    binary = binary_path.read_bytes()
    provenance = json.loads(provenance_path.read_text())
    if binary[:2] != b"MZ" or provenance["version"] != version or sha(binary) != provenance["binary"]["sha256"]:
        raise SystemExit("Binary/provenance mismatch")
    for item in provenance["sourceInputs"]:
        if item["path"] not in source or sha(source[item["path"]]) != item["sha256"]:
            raise SystemExit("Build input differs from reviewed source: " + item["path"])
    args.output.mkdir(parents=True, exist_ok=True)
    source_name = "ChatAtlas-" + version + "-source.zip"
    source_files = {"ChatAtlas-source/" + name: data for name, data in source.items()}
    manifest = "".join(sha(data) + "  " + name + "\n" for name, data in sorted(source.items()))
    source_files["ChatAtlas-source/SOURCE_SHA256SUMS.txt"] = manifest.encode()
    write_zip(args.output / source_name, source_files)
    files = {"ChatAtlas/ChatAtlas.exe": binary,
             "ChatAtlas/BUILD_PROVENANCE.json": provenance_path.read_bytes()}
    for name, payload in source.items():
        if name in {"LICENSE", "SECURITY.md", "THIRD_PARTY_NOTICES.md", "VERSION"} or name.startswith("licenses/") or name in {"native/vendor/LICENSE-nlohmann.txt", "native/vendor/LICENSE-hedley.txt"}:
            files["ChatAtlas/" + name.replace("native/vendor/", "licenses/")] = payload
    files["ChatAtlas/使用說明.txt"] = (
        "請先解壓整包，再雙擊 ChatAtlas.exe。\n"
        "這是 Windows x64 原生程式，不需要安裝 Python、Node.js 或另裝 .NET。\n"
        "人工範例：開啟 人工範例/index.html，保留旁邊的資料夾。\n"
        "本包僅供人工範例與開發驗證，未執行深入安全複核，不是正式穩定版。\n"
        "真實 LINE 自動採集、Windows GUI 與大檔保存均未完成實機驗收。\n"
        "人工範例所有對話與附件均由隨附合成資料產生；不含真實聊天。\n"
        "EXE 尚未簽章，請勿停用防毒或系統保護。\n").encode()
    files.update(make_synthetic_example())
    files["ChatAtlas/SHA256SUMS.txt"] = "".join(sha(data) + "  " + name.removeprefix("ChatAtlas/") + "\n" for name, data in sorted(files.items())).encode()
    binary_name = "ChatAtlas-" + version + "-Windows-x64-candidate.zip"
    write_zip(args.output / binary_name, files)
    names = [source_name, binary_name]
    (args.output / "SHA256SUMS.txt").write_text("".join(sha((args.output / name).read_bytes()) + "  " + name + "\n" for name in names))
    print(json.dumps({"version": version, "files": names + ["SHA256SUMS.txt"],
                      "source_files": len(source), "public_release_approved": False,
                      "validation": "See docs/STATUS.md; independent security review incomplete"}, ensure_ascii=False))

if __name__ == "__main__":
    main()
