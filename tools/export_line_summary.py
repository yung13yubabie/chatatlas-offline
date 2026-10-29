"""Export one authorized local LINE range for ChatAtlas's native importer.

Run with the Windows Python environment already installed for line-summary.
--line-summary selects trusted local Python code; its settings.json must already
authorize the account, chat and dates. This script calls line_get_history only;
it does not access a database, extract a key, alter settings or reset budgets.

Only a completed local [since, until) scan produces a snapshot. Completion does
not establish cloud-history completeness. Choose a new file in an existing
private directory outside Git. Message text, names and identifiers are retained.
"""

import argparse
import ctypes
from datetime import datetime, timezone
import importlib
import json
import os
from pathlib import Path
import re
import secrets
import stat
import sys


MAX_SNAPSHOT_BYTES = 64 * 1024 * 1024
MAX_PAGE_BYTES = 262144
CHAT_KINDS = ("group", "direct", "room", "community")
_ISO_DATE = re.compile(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}"
                       r"(?:\.([0-9]{1,9}))?(Z|([+-])([0-9]{2}):([0-9]{2}))")


class ExportError(ValueError):
    """A fixed diagnostic that contains no backend or chat data."""


def _identifier(value):
    if (not isinstance(value, str) or not value or len(value) > 256
            or any(ord(char) < 32 or ord(char) == 127 for char in value)
            or _utf8_size(value) > 1024):
        raise ExportError("A required identifier is invalid.")
    return value


def _utf8_size(value):
    try:
        return len(value.encode("utf-8"))
    except UnicodeError:
        raise ExportError("A text field contains invalid Unicode.") from None


def _title(value):
    if (not isinstance(value, str) or not value.strip() or "\0" in value
            or _utf8_size(value) > 4096):
        raise ExportError("The chat title is invalid.")


def _date(value):
    matched = _ISO_DATE.fullmatch(value) if isinstance(value, str) else None
    if matched is None:
        raise ExportError("Dates must be timezone-aware ISO 8601 values.")
    try:
        if matched[2] != "Z":
            hour, minute = int(matched[4]), int(matched[5])
            if hour > 14 or minute > 59 or hour == 14 and minute:
                raise ValueError()
        result = datetime.fromisoformat(value)
        if not 1970 <= result.year <= 2999 or not 1970 <= result.astimezone(timezone.utc).year <= 2999:
            raise ValueError()
        return result
    except (ValueError, OverflowError):
        raise ExportError("Dates must be timezone-aware ISO 8601 values.") from None


def _range(since, until):
    start, end = _date(since), _date(until)
    for raw, parsed in ((since, start), (until, end)):
        fraction = re.search(r"[.,]([0-9]+)", raw)
        if parsed.microsecond % 1000 or fraction and any(char != "0" for char in fraction[1][3:]):
            raise ExportError("Range boundaries must use millisecond precision or coarser.")
    if end <= start:
        raise ExportError("The exclusive end date must follow the start date.")
    return start, end


def _scope(account_id, chat_id, chat_title, chat_kind, since, until, limit):
    _identifier(account_id)
    _identifier(chat_id)
    _title(chat_title)
    if chat_kind not in CHAT_KINDS:
        raise ExportError("The chat kind is invalid.")
    start, end = _range(since, until)
    if type(limit) is not int or limit <= 0:
        raise ExportError("The message limit must be a positive integer.")
    return start, end


def _json_bytes(value, maximum, *, ascii_only=False):
    """Bound serialization without first constructing an unbounded JSON string."""
    encoder = json.JSONEncoder(ensure_ascii=ascii_only, allow_nan=False)
    chunks, size = [], 0
    try:
        for piece in encoder.iterencode(value):
            encoded = piece.encode("utf-8")
            size += len(encoded)
            if size > maximum:
                raise ExportError("The JSON size limit was exceeded; no snapshot was published.")
            chunks.append(encoded)
    except (TypeError, ValueError, UnicodeError, RecursionError) as error:
        if isinstance(error, ExportError):
            raise
        raise ExportError("The history response contains invalid JSON data.") from None
    return b"".join(chunks)


def collect_snapshot(account_id, chat_id, chat_title, chat_kind, since, until,
                     limit=5000, *, history_client):
    """Read fixed-scope public API pages, never interpreting opaque cursors."""
    start, end = _scope(account_id, chat_id, chat_title, chat_kind, since, until, limit)

    snapshot = {"format": "chatatlas.line-summary", "schema": 1,
                "account_id": account_id,
                "chat": {"source_id": chat_id, "title": chat_title, "kind": chat_kind},
                "since": since, "until": until, "local_range_complete": True,
                "messages": []}
    # Include the envelope and list punctuation in the accumulated byte budget.
    size = len(_json_bytes(snapshot, MAX_SNAPSHOT_BYTES))
    seen_ids, seen_cursors = set(), set()
    cursor, previous = None, None
    while len(snapshot["messages"]) < limit:
        requested = min(500, limit - len(snapshot["messages"]))
        try:
            result = history_client(chat_id=chat_id, since=since, until=until,
                                    limit=requested, cursor=cursor)
        except Exception:
            raise ExportError("The authorized history request failed; no snapshot was published.") from None
        if (not isinstance(result, dict) or not isinstance(result.get("items"), list)
                or type(result.get("has_more")) is not bool
                or result.get("content_complete") is not True
                or result.get("blocked_reason")):
            raise ExportError("History was blocked, incomplete or invalid; no snapshot was published.")
        _json_bytes(result, MAX_PAGE_BYTES, ascii_only=True)
        items, more = result["items"], result["has_more"]
        if len(items) > requested:
            raise ExportError("A history page exceeded the requested message limit.")
        for item in items:
            if not isinstance(item, dict):
                raise ExportError("A history message is invalid.")
            mid = _identifier(item.get("message_id"))
            if mid in seen_ids:
                raise ExportError("History contains a repeated message identifier.")
            sent_at = _date(item.get("sent_at"))
            position = (sent_at, mid)
            if not start <= sent_at < end or previous is not None and position <= previous:
                raise ExportError("History is out of range or its ordering is invalid.")
            for field in ("thread_id", "thread_root_message_id"):
                if field in item:
                    _identifier(item[field])
            if ("thread_id" in item) != ("thread_root_message_id" in item):
                raise ExportError("A history message has incomplete thread identifiers.")
            encoded = _json_bytes(item, MAX_SNAPSHOT_BYTES)
            size += len(encoded) + (2 if snapshot["messages"] else 0)
            if size > MAX_SNAPSHOT_BYTES:
                raise ExportError("The snapshot size limit was exceeded; no snapshot was published.")
            seen_ids.add(mid)
            previous = position
            snapshot["messages"].append(item)
        next_cursor = result.get("next_cursor")
        if not more:
            if next_cursor is not None:
                raise ExportError("The final history page contains an invalid continuation.")
            return snapshot
        if (not items or not isinstance(next_cursor, str) or not next_cursor
                or len(next_cursor) > 4096 or next_cursor in seen_cursors):
            raise ExportError("History pagination cannot advance; no snapshot was published.")
        seen_cursors.add(next_cursor)
        cursor = next_cursor
    raise ExportError("The message limit was reached before completion; no snapshot was published.")


def load_history_client(source):
    """Import the explicit trusted checkout without reloading a live server."""
    root = Path(source).absolute()
    _no_links(root)
    server_file = root / "line_mcp_server.py"
    if not server_file.is_file():
        raise ExportError("The selected line-summary checkout is unavailable.")
    _no_links(server_file)
    for name in ("line_mcp_server", "db_reader", "key_extractor", "safety"):
        existing = sys.modules.get(name)
        if existing is not None:
            origin = getattr(existing, "__file__", None)
            if origin is None or Path(origin).resolve().parent != root.resolve():
                raise ExportError("A conflicting Python module is already loaded; export stopped.")
    sys.path.insert(0, str(root))
    try:
        # import_module returns an existing instance, preserving its session budget.
        server = importlib.import_module("line_mcp_server")
    except Exception:
        raise ExportError("The selected line-summary environment could not be loaded.") from None
    finally:
        sys.path.pop(0)
    if Path(server.__file__).resolve() != server_file.resolve() or not callable(getattr(server, "line_get_history", None)):
        raise ExportError("The selected line-summary API is unavailable.")
    return server.line_get_history


def _no_links(path):
    for node in (path, *path.parents):
        try:
            info = node.lstat()
        except FileNotFoundError:
            continue
        if (stat.S_ISLNK(info.st_mode)
                or getattr(info, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)):
            raise ExportError("Choose a destination without links or reparse points.")


def output_destination(value):
    path = Path(value).absolute()
    _no_links(path)
    for node in (path.parent, *path.parent.parents):
        if (node / ".git").exists():
            raise ExportError("The snapshot must be stored outside Git repositories.")
    if not path.parent.is_dir() or not path.name:
        raise ExportError("Choose a new file in an existing private directory.")
    if os.name == "nt" and (re.search(r'[<>:"/\\|?*\x00-\x1f]', path.name)
            or path.name.endswith((" ", "."))
            or re.fullmatch(r"(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?", path.name)):
        raise ExportError("The output filename is invalid.")
    if os.path.lexists(path):
        raise ExportError("The output already exists; choose a new filename.")
    return path


def _windows_private_open(path):
    """Create a file with its protected current-user/SYSTEM DACL immediately."""
    import ctypes.wintypes as wt
    import msvcrt

    class SecurityAttributes(ctypes.Structure):
        _fields_ = [("nLength", wt.DWORD), ("lpSecurityDescriptor", ctypes.c_void_p),
                    ("bInheritHandle", wt.BOOL)]

    class SidAndAttributes(ctypes.Structure):
        _fields_ = [("Sid", ctypes.c_void_p), ("Attributes", wt.DWORD)]

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    advapi = ctypes.WinDLL("advapi32", use_last_error=True)
    kernel.GetCurrentProcess.restype = wt.HANDLE
    kernel.CloseHandle.argtypes = [wt.HANDLE]
    kernel.LocalFree.argtypes = [ctypes.c_void_p]
    kernel.LocalFree.restype = ctypes.c_void_p
    advapi.OpenProcessToken.argtypes = [wt.HANDLE, wt.DWORD, ctypes.POINTER(wt.HANDLE)]
    advapi.OpenProcessToken.restype = wt.BOOL
    advapi.GetTokenInformation.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_void_p,
                                          wt.DWORD, ctypes.POINTER(wt.DWORD)]
    advapi.GetTokenInformation.restype = wt.BOOL
    advapi.ConvertSidToStringSidW.argtypes = [ctypes.c_void_p, ctypes.POINTER(wt.LPWSTR)]
    advapi.ConvertSidToStringSidW.restype = wt.BOOL
    advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW.argtypes = [
        wt.LPCWSTR, wt.DWORD, ctypes.POINTER(ctypes.c_void_p), ctypes.c_void_p]
    advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW.restype = wt.BOOL
    kernel.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD,
                                  ctypes.POINTER(SecurityAttributes), wt.DWORD,
                                  wt.DWORD, wt.HANDLE]
    kernel.CreateFileW.restype = wt.HANDLE
    token, sid_text, descriptor = wt.HANDLE(), wt.LPWSTR(), ctypes.c_void_p()
    try:
        if not advapi.OpenProcessToken(kernel.GetCurrentProcess(), 0x0008, ctypes.byref(token)):
            raise OSError("Private file creation failed")
        size = wt.DWORD()
        advapi.GetTokenInformation(token, 1, None, 0, ctypes.byref(size))
        if not size.value:
            raise OSError("Private file creation failed")
        buffer = ctypes.create_string_buffer(size.value)
        if not advapi.GetTokenInformation(token, 1, buffer, size, ctypes.byref(size)):
            raise OSError("Private file creation failed")
        user = ctypes.cast(buffer, ctypes.POINTER(SidAndAttributes)).contents
        if not advapi.ConvertSidToStringSidW(user.Sid, ctypes.byref(sid_text)):
            raise OSError("Private file creation failed")
        sddl = "O:" + sid_text.value + "D:P(A;;FA;;;" + sid_text.value + ")(A;;FA;;;SY)"
        if not advapi.ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, 1, ctypes.byref(descriptor), None):
            raise OSError("Private file creation failed")
        security = SecurityAttributes(ctypes.sizeof(SecurityAttributes), descriptor, False)
        handle = kernel.CreateFileW(str(path), 0x40000000, 0, ctypes.byref(security),
                                    1, 0x80 | 0x00200000, None)
        if handle == ctypes.c_void_p(-1).value:
            code = ctypes.get_last_error()
            if code in (80, 183):
                raise FileExistsError()
            raise OSError("Private file creation failed")
        try:
            return msvcrt.open_osfhandle(handle, os.O_WRONLY | os.O_BINARY)
        except BaseException:
            kernel.CloseHandle(handle)
            raise
    finally:
        if descriptor:
            kernel.LocalFree(descriptor)
        if sid_text:
            kernel.LocalFree(ctypes.cast(sid_text, ctypes.c_void_p))
        if token:
            kernel.CloseHandle(token)


def write_snapshot(destination, snapshot):
    """Publish completed bytes atomically and refuse overwrite, including races."""
    path = output_destination(destination)
    data = _json_bytes(snapshot, MAX_SNAPSHOT_BYTES)
    temporary, fd, published = None, None, False
    try:
        for _ in range(20):
            candidate = path.parent / (".chatatlas-" + secrets.token_hex(16) + ".partial")
            try:
                fd = (_windows_private_open(candidate) if os.name == "nt" else
                      os.open(candidate, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0), 0o600))
                temporary = candidate
                break
            except FileExistsError:
                continue
        if fd is None:
            raise ExportError("A private temporary file could not be created.")
        with os.fdopen(fd, "wb") as stream:
            fd = None
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        output_destination(path)
        if os.name == "nt":
            # Windows rename refuses an existing target and retains the file DACL.
            os.rename(temporary, path)
        else:
            # A hardlink is an atomic exclusive publish; replace() could overwrite.
            os.link(temporary, path, follow_symlinks=False)
        published = True
        return len(data)
    finally:
        if fd is not None:
            os.close(fd)
        if temporary is not None:
            try:
                temporary.unlink(missing_ok=True)
            except OSError:
                # Cleanup cannot turn a completed publish into a reported failure
                # or replace the original write exception with a cleanup error.
                warning = ("Snapshot published; its private temporary copy could not be removed."
                           if published else "Temporary cleanup failed; a private partial file may remain.")
                print(warning, file=sys.stderr)


class _Parser(argparse.ArgumentParser):
    def error(self, message):
        raise ExportError("Invalid command-line arguments; use --help.")


def main(argv=None):
    try:
        parser = _Parser(description=__doc__)
        parser.add_argument("--line-summary", required=True, metavar="DIRECTORY")
        parser.add_argument("--account-id", required=True, help="stable local alias for this LINE account")
        parser.add_argument("--chat-id", required=True)
        parser.add_argument("--chat-title", required=True)
        parser.add_argument("--chat-kind", required=True, choices=CHAT_KINDS)
        parser.add_argument("--since", required=True, help="inclusive ISO 8601 with timezone")
        parser.add_argument("--until", required=True, help="exclusive ISO 8601 with timezone")
        parser.add_argument("--output", required=True, metavar="FILE")
        parser.add_argument("--limit", type=int, default=5000)
        args = parser.parse_args(argv)
        # Validate all caller fields before loading trusted code or reading LINE.
        _scope(args.account_id, args.chat_id, args.chat_title, args.chat_kind,
               args.since, args.until, args.limit)
        destination = output_destination(args.output)
        client = load_history_client(args.line_summary)
        snapshot = collect_snapshot(args.account_id, args.chat_id, args.chat_title,
                                    args.chat_kind, args.since, args.until, args.limit,
                                    history_client=client)
        size = write_snapshot(destination, snapshot)
    except Exception as error:
        message = str(error) if isinstance(error, ExportError) else "Export failed; no snapshot was published."
        print(message, file=sys.stderr)
        return 1
    print("messages=" + str(len(snapshot["messages"])))
    print("local_range_complete=true")
    print("bytes=" + str(size))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
