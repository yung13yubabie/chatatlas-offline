import json
import tempfile
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol

from .errors import Cancelled, CapabilityError, DownloadUnavailable, IntegrityError, RetryableDownload, VaultError
from .model import Attachment, Chat, Item, KINDS, utc_now
from .safeio import copy_verified, is_link
from .store import Store


class Source(Protocol):
    account_id: str

    def list_chats(self) -> list[Chat]: ...
    def read(self, chat: Chat, since: str | None, until: str | None,
             control: "Control") -> tuple[list[Item], list[Attachment], str]: ...
    def download(self, chat: Chat, attachment: Attachment, destination: Path,
                 control: "Control") -> Path: ...


class Control:
    def __init__(self):
        self.stopped = threading.Event()
        self.running = threading.Event()
        self.running.set()

    def pause(self): self.running.clear()
    def resume(self): self.running.set()
    def stop(self):
        self.stopped.set()
        self.running.set()

    def checkpoint(self):
        while not self.running.wait(.1):
            if self.stopped.is_set():
                raise Cancelled("保存已停止，已完成的內容仍會保留。")
        if self.stopped.is_set():
            raise Cancelled("保存已停止，已完成的內容仍會保留。")

    def wait(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.checkpoint()
            self.stopped.wait(min(.1, max(0, deadline-time.monotonic())))
        self.checkpoint()


@dataclass(frozen=True)
class CaptureOptions:
    kinds: frozenset[str] = KINDS
    attempts: int = 3

    def __post_init__(self):
        if not self.kinds or not self.kinds <= KINDS or not 1 <= self.attempts <= 3:
            raise VaultError("請選擇要保存的內容。")
        for pair in ({"message", "thread"}, {"album", "album_item"}, {"note", "comment"}):
            if self.kinds & pair and not pair <= self.kinds:
                raise VaultError("相關內容必須一起保存，才能保留完整來源。")


def extension(path: Path) -> str:
    with path.open("rb") as f:
        header = f.read(32)
    if header.startswith(b"\x89PNG\r\n\x1a\n"): return ".png"
    if header.startswith(b"\xff\xd8\xff"): return ".jpg"
    if header.startswith((b"GIF87a",b"GIF89a")): return ".gif"
    if header[:4]==b"RIFF" and header[8:12]==b"WEBP": return ".webp"
    if header[:4]==b"RIFF" and header[8:12]==b"WAVE": return ".wav"
    if header.startswith(b"OggS"): return ".ogg"
    if header.startswith(b"ID3"): return ".mp3"
    if header[4:8]==b"ftyp": return ".mp4"
    return ".bin"


class Collector:
    def __init__(self, store: Store, source: Source, control=None, progress=lambda event: None, options=None):
        self.store,self.source = store,source
        self.control = control or Control()
        self.progress = progress
        self.options = options or CaptureOptions()

    def _parents(self, selected):
        if not any(c.parent_source_id for c in selected):
            return []
        available = {c.source_id:c for c in self.source.list_chats()}
        parents = {}
        for chat in selected:
            if chat.parent_source_id:
                parent = available.get(chat.parent_source_id)
                if not parent or parent.kind != "community" or parent.source_id == chat.source_id:
                    raise IntegrityError("選取的子聊天室找不到所屬社群，尚未開始保存。")
                parents[parent.source_id] = parent
        return list(parents.values())

    def run(self, selected: list[Chat], since=None, until=None) -> dict:
        if not selected or len({c.source_id for c in selected}) != len(selected):
            raise VaultError("請先選擇有效且不重複的聊天室。")
        if since and until and since >= until:
            raise VaultError("結束時間必須晚於開始時間。")
        counts = {"items":0,"reused":0,"downloaded":0,"failed":0,"unavailable":0}
        if hasattr(self.source, "set_kinds"):
            self.source.set_kinds(self.options.kinds)
        with self.store.transaction():
            for parent in self._parents(selected):
                self.store.put_chat(self.source.account_id, parent)
        for chat in selected:
            local = dict.fromkeys(counts, 0)
            self.control.checkpoint()
            with self.store.transaction():
                cid = self.store.put_chat(self.source.account_id, chat)
                cursor = self.store.db.execute("INSERT INTO jobs(chat_id,status,started_at) VALUES(?,?,?)",
                      (cid,"running",utc_now()))
                jid = cursor.lastrowid
            try:
                self.progress({"stage":"read","chat":chat.title})
                items,attachments,coverage = self.source.read(chat,since,until,self.control)
                provided={item.source_id for item in items}
                if any(a.item_source_id not in provided for a in attachments):
                    raise IntegrityError("附件缺少本次可核對的所屬內容，未建立孤立紀錄。")
                # Defense in depth; source-side filtering also prevents opening unselected panels.
                items = [item for item in items if item.kind in self.options.kinds]
                kept = {item.source_id for item in items}
                attachments = [a for a in attachments if a.item_source_id in kept]
                if coverage not in {"local_only","visible_only","source_range","unknown"}:
                    raise IntegrityError("來源回傳範圍資訊不正確。")
                with self.store.transaction():
                    self.store.put_items(cid,items)
                    work = [(a,self.store.put_attachment(cid,a)) for a in attachments]
                local["items"] += len(items)
                for index,(attachment,aid) in enumerate(work,1):
                    self.control.checkpoint()
                    self.progress({"stage":"files","current":index,"total":len(work),"chat":chat.title})
                    if self.store.reusable(aid,self.control.checkpoint):
                        local["reused"] += 1
                        continue
                    with self.store.transaction():
                        known=self.store.reuse_known_content(aid,self.control.checkpoint)
                    if known:
                        local["reused"] += 1
                        continue
                    # A failed refresh must not leave a corrupt blob marked saved.
                    with self.store.transaction():
                        self.store.db.execute("UPDATE attachments SET status='pending',reason='' WHERE id=?",(aid,))
                    for attempt in range(self.options.attempts):
                      try:
                        with tempfile.TemporaryDirectory(prefix="download-",dir=self.store.root) as temp:
                            stagedir = Path(temp).resolve()
                            src = Path(self.source.download(chat,attachment,stagedir,self.control))
                            if is_link(src) or not src.resolve().is_relative_to(stagedir):
                                raise IntegrityError("下載結果超出指定工作區。")
                            from .safeio import digest
                            checksum,_ = digest(src,self.control.checkpoint)
                            rel = "assets/"+checksum[:2]+"/"+checksum+extension(src)
                            sha,size=copy_verified(src,self.store.root,rel,
                                expected_size=attachment.expected_size,
                                expected_hash=attachment.expected_sha256,checkpoint=self.control.checkpoint)
                            with self.store.transaction():
                                self.store.save_blob(aid,rel,sha,size,attachment.variant=="thumbnail",self.control.checkpoint)
                            local["downloaded"] += 1
                        break
                      except (Cancelled, CapabilityError):
                        # A changed account, room or focus is a source failure, not a missing file.
                        raise
                      except DownloadUnavailable:
                        local["unavailable"] += 1
                        with self.store.transaction():
                            self.store.db.execute("UPDATE attachments SET status='unavailable',reason=? WHERE id=?",
                               ("來源目前無法提供這份檔案；已保存的舊副本仍會保留。",aid))
                        break
                      except (OSError,RetryableDownload):
                        if attempt + 1 < self.options.attempts:
                            self.progress({"stage":"retry","attempt":attempt+2,"total":self.options.attempts,"chat":chat.title})
                            self.control.wait(.2 * (2**attempt))
                            continue
                        local["failed"] += 1
                        with self.store.transaction():
                            self.store.db.execute("UPDATE attachments SET status='failed',reason=? WHERE id=?",
                               ("暫時無法取得完整檔案，可稍後重試。",aid))
                      except (IntegrityError,ValueError):
                        local["failed"] += 1
                        with self.store.transaction():
                            self.store.db.execute("UPDATE attachments SET status='failed',reason=? WHERE id=?",
                               ("檔案內容檢查未通過，未列為完整副本。",aid))
                      break
                with self.store.transaction():
                    dated=[item.sent_at for item in items if item.sent_at]
                    job_counts={**local,"range_start":min(dated) if dated else None,
                                "range_end":max(dated) if dated else None,"requested_since":since,
                                "requested_until":until,"kinds":sorted(self.options.kinds)}
                    self.store.db.execute("UPDATE jobs SET status=?,finished_at=?,coverage=?,counts=? WHERE id=?",
                      ("complete_with_issues" if local["failed"] or local["unavailable"] else "complete",utc_now(),coverage,json.dumps(job_counts),jid))
                for name in counts:counts[name] += local[name]
            except BaseException as exc:
                with self.store.transaction():
                    self.store.db.execute("UPDATE jobs SET status=?,finished_at=?,counts=? WHERE id=?",
                      ("stopped" if isinstance(exc,Cancelled) else "failed",utc_now(),json.dumps(local),jid))
                raise
        self.progress({"stage":"done",**counts})
        return counts
