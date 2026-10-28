import hashlib
import os
import re
import stat
import tempfile
from pathlib import Path

from .errors import IntegrityError

CHUNK = 1024 * 1024


def canonical_hash(value) -> bool:
    """Archive-generated identifiers and checksums are lowercase SHA-256 hex."""
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def is_link(path: Path) -> bool:
    """Include Windows junctions and other reparse points on Python 3.11+."""
    try:
        info = Path(path).lstat()
    except FileNotFoundError:
        return False
    return stat.S_ISLNK(info.st_mode) or bool(
        getattr(info, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def no_links(path: Path):
    for node in (Path(path).absolute(), *Path(path).absolute().parents):
        if is_link(node):
            raise IntegrityError("保存位置包含連結，請改用一般資料夾。")


def private_destination(path):
    from .errors import VaultError
    path = Path(path).absolute()
    for ancestor in (path, *path.parents):
        if (ancestor / ".git").exists():
            raise VaultError("聊天與附件請存到程式原始碼資料夾以外，避免被提交到 Git。")
    return path


def safe_name(value: str) -> str:
    value = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", str(value)).strip(" .")
    value = value[:150] or "attachment"
    if re.fullmatch(r"(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?", value):
        value = "_" + value
    return value


def private_dir(path: Path) -> Path:
    path = Path(path).absolute()
    # No links in an archive path, including an existing ancestor.
    no_links(path)
    path.mkdir(parents=True, exist_ok=True, mode=0o700)
    return path


def within(root: Path, relative: str) -> Path:
    no_links(root)
    root = Path(root).resolve()
    rel = Path(relative)
    if rel.is_absolute() or not relative or ".." in rel.parts or "\\" in relative or ":" in relative:
        raise IntegrityError("檔案路徑不正確。")
    dest = root / rel
    for node in (dest, *dest.parents):
        if node == root:
            break
        if is_link(node):
            raise IntegrityError("檔案位置包含連結，已停止處理。")
    if not dest.resolve().is_relative_to(root):
        raise IntegrityError("檔案位置超出封存範圍。")
    return dest


def digest(path: Path, checkpoint=lambda: None) -> tuple[str, int]:
    path = Path(path)
    if is_link(path):
        raise IntegrityError("不使用連結作為附件副本。")
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0)
    fd = os.open(path, flags)
    with os.fdopen(fd, "rb") as f:
        before = os.fstat(f.fileno())
        if not stat.S_ISREG(before.st_mode):
            raise IntegrityError("附件不是一般檔案。")
        h, size = hashlib.sha256(), 0
        while chunk := f.read(CHUNK):
            checkpoint()
            h.update(chunk)
            size += len(chunk)
        after = os.fstat(f.fileno())
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise IntegrityError("附件在檢查期間發生變動，請重試。")
        return h.hexdigest(), size


def atomic_bytes(path: Path, data: bytes):
    no_links(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix=".vault-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def copy_verified(source: Path, root: Path, relative: str, *, expected_size=None,
                  expected_hash=None, checkpoint=lambda: None) -> tuple[str, int]:
    source = Path(source)
    if is_link(source) or not source.is_file():
        raise IntegrityError("附件不是可讀取的一般檔案。")
    dest = within(root, relative)
    dest.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, temp = tempfile.mkstemp(prefix=".partial-", dir=dest.parent)
    try:
        with os.fdopen(fd, "wb") as out:
            srcfd = os.open(source, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0))
            with os.fdopen(srcfd, "rb") as src:
                before = os.fstat(src.fileno())
                if not stat.S_ISREG(before.st_mode):
                    raise IntegrityError("附件不是一般檔案。")
                h, size = hashlib.sha256(), 0
                while chunk := src.read(CHUNK):
                    checkpoint()
                    h.update(chunk)
                    size += len(chunk)
                    out.write(chunk)
                after = os.fstat(src.fileno())
                if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                    raise IntegrityError("來源檔案仍在變動，尚未完成下載。")
                out.flush()
                os.fsync(out.fileno())
        checksum = h.hexdigest()
        if size == 0 and expected_size != 0 and not expected_hash or expected_size is not None and size != expected_size:
            raise IntegrityError("附件大小不符，未列為已保存。")
        if expected_hash and checksum.lower() != expected_hash.lower():
            raise IntegrityError("附件內容檢查不符，未列為已保存。")
        if digest(Path(temp),checkpoint) != (checksum, size):
            raise IntegrityError("寫入副本檢查失敗。")
        os.replace(temp, dest)
        return checksum, size
    finally:
        if os.path.exists(temp):
            os.unlink(temp)
