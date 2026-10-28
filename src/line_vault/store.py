import hashlib
import json
import sqlite3
from contextlib import contextmanager
from pathlib import Path

from .errors import IntegrityError
from .model import Attachment, Chat, Item, utc_now
from .safeio import canonical_hash, digest, is_link, private_dir, within
from .locking import ArchiveLock

SCHEMA = """
CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS chats(
 id TEXT PRIMARY KEY, account TEXT NOT NULL, source_id TEXT NOT NULL,
 title TEXT NOT NULL, kind TEXT NOT NULL, parent_source_id TEXT,
 UNIQUE(account,source_id));
CREATE TABLE IF NOT EXISTS items(
 id TEXT PRIMARY KEY,chat_id TEXT NOT NULL REFERENCES chats(id),source_id TEXT NOT NULL,
 kind TEXT NOT NULL CHECK(kind IN ('message','album','album_item','note','comment','thread')),
 parent_id TEXT REFERENCES items(id) DEFERRABLE INITIALLY DEFERRED,
 reply_source_id TEXT,text TEXT NOT NULL,title TEXT NOT NULL,
 sender_id TEXT,sender_name TEXT NOT NULL,sent_at TEXT,state TEXT NOT NULL,
 ordinal INTEGER NOT NULL,metadata TEXT NOT NULL,seen_at TEXT NOT NULL,
 UNIQUE(chat_id,source_id));
CREATE INDEX IF NOT EXISTS items_chat_time ON items(chat_id,sent_at,ordinal,id);
CREATE TABLE IF NOT EXISTS blobs(
 id TEXT PRIMARY KEY,path TEXT UNIQUE NOT NULL,sha256 TEXT NOT NULL,size INTEGER NOT NULL,
 CHECK(size>=0));
CREATE TABLE IF NOT EXISTS attachments(
 id TEXT PRIMARY KEY,chat_id TEXT NOT NULL REFERENCES chats(id),
 source_id TEXT NOT NULL,variant TEXT NOT NULL,version TEXT NOT NULL,
 item_id TEXT NOT NULL REFERENCES items(id),name TEXT NOT NULL,kind TEXT NOT NULL,
 status TEXT NOT NULL CHECK(status IN ('pending','saved','preview','unavailable','failed')),
 blob_id TEXT REFERENCES blobs(id),expected_size INTEGER,expected_hash TEXT,
 reason TEXT NOT NULL DEFAULT '',saved_before INTEGER NOT NULL DEFAULT 0,
 UNIQUE(chat_id,item_id,source_id,variant,version));
CREATE INDEX IF NOT EXISTS attachments_item ON attachments(item_id);
CREATE TABLE IF NOT EXISTS jobs(
 id INTEGER PRIMARY KEY,chat_id TEXT NOT NULL REFERENCES chats(id),
 status TEXT NOT NULL,started_at TEXT NOT NULL,finished_at TEXT,
 coverage TEXT NOT NULL DEFAULT 'unknown',counts TEXT NOT NULL DEFAULT '{}');
CREATE INDEX IF NOT EXISTS jobs_chat ON jobs(chat_id,id);
CREATE TABLE IF NOT EXISTS issues(
 id INTEGER PRIMARY KEY,chat_id TEXT REFERENCES chats(id),
 code TEXT NOT NULL,detail TEXT NOT NULL,created_at TEXT NOT NULL);
"""


def key(*parts: str) -> str:
    return hashlib.sha256(json.dumps(parts, ensure_ascii=True).encode()).hexdigest()


class Store:
    def __init__(self, root: Path):
        self.root = private_dir(root)
        self.lock = ArchiveLock(self.root)
        self.db = None
        try:
            self._open()
        except sqlite3.DatabaseError:
            self.close()
            raise IntegrityError("封存資料庫無法讀取，請先保留整個資料夾，再檢查可用的副本。") from None
        except BaseException:
            self.close()
            raise

    def _open(self):
        dbpath = within(self.root, "archive.sqlite3")
        existed = dbpath.exists()
        self.db = sqlite3.connect(dbpath, timeout=10)
        self.db.row_factory = sqlite3.Row
        if existed and self.db.execute("PRAGMA application_id").fetchone()[0] != 0x4C564C54:
            raise IntegrityError("這個資料庫不是可辨識的 LINE Vault 封存，未修改內容。")
        version = self.db.execute("PRAGMA user_version").fetchone()[0]
        if version not in (0, 1):
            raise IntegrityError("封存格式較新，請使用相容版本開啟。")
        self.db.execute("PRAGMA foreign_keys=ON")
        if self.db.execute("PRAGMA foreign_keys").fetchone()[0] != 1:
            raise IntegrityError("資料庫無法啟用關聯檢查。")
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("PRAGMA synchronous=FULL")
        self.db.executescript(SCHEMA)
        self.db.execute("PRAGMA user_version=1")
        self.db.execute("PRAGMA application_id=1280724052")
        # The archive lock proves no previous writer is still running.
        self.db.execute("UPDATE jobs SET status='interrupted',finished_at=? WHERE status='running'", (utc_now(),))
        self.db.commit()

    def close(self):
        if self.db:
            self.db.close()
            self.db = None
        self.lock.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    @contextmanager
    def transaction(self):
        try:
            self.db.execute("BEGIN IMMEDIATE")
            yield
            self.db.commit()
        except BaseException:
            self.db.rollback()
            raise

    def chat_id(self, account: str, source_id: str) -> str:
        return key("chat", account, source_id)

    def item_id(self, chat_id: str, source_id: str) -> str:
        return key("item", chat_id, source_id)

    def put_chat(self, account: str, chat: Chat) -> str:
        if not account:
            raise ValueError("account identity required")
        cid = self.chat_id(account, chat.source_id)
        if chat.parent_source_id:
            parent = self.db.execute("SELECT kind FROM chats WHERE account=? AND source_id=?",
                                     (account, chat.parent_source_id)).fetchone()
            if chat.parent_source_id == chat.source_id or not parent or parent[0] != "community" or chat.kind != "subchat":
                raise IntegrityError("子聊天室缺少有效的所屬社群，已停止配對。")
        elif chat.kind == "subchat":
            raise IntegrityError("子聊天室缺少所屬社群。")
        if chat.kind != "community" and self.db.execute(
                "SELECT 1 FROM chats WHERE account=? AND parent_source_id=?", (account, chat.source_id)).fetchone():
            raise IntegrityError("所屬社群類型改變，未覆蓋子聊天室關聯。")
        self.db.execute("""INSERT INTO chats VALUES(?,?,?,?,?,?)
          ON CONFLICT(id) DO UPDATE SET title=excluded.title,kind=excluded.kind,
          parent_source_id=excluded.parent_source_id""",
          (cid, account, chat.source_id, chat.title, chat.kind, chat.parent_source_id))
        return cid

    def put_items(self, cid: str, items: list[Item]):
        if len({i.source_id for i in items}) != len(items):
            raise IntegrityError("來源含重複識別，尚未更新現有內容。")
        existing = {r["source_id"]: r for r in self.db.execute("SELECT * FROM items WHERE chat_id=?", (cid,))}
        mapping = {sid: r["id"] for sid, r in existing.items()}
        mapping.update({i.source_id: self.item_id(cid, i.source_id) for i in items})
        incoming = {i.source_id: i for i in items}
        for item in items:
            if item.source_id in existing and existing[item.source_id]["kind"] != item.kind:
                raise IntegrityError("同一來源識別的項目類型改變，尚未覆蓋舊資料。")
            if item.parent_source_id and item.parent_source_id not in mapping:
                raise IntegrityError("項目缺少所屬來源，已停止配對。")
            if item.kind in {"album_item", "comment"} and not item.parent_source_id:
                raise IntegrityError("相簿圖片或留言缺少所屬內容，已停止配對。")
            if item.parent_source_id == item.source_id:
                raise IntegrityError("來源關聯不能指向自己。")
            parent = incoming.get(item.parent_source_id) or existing.get(item.parent_source_id)
            if item.parent_source_id:
                pkind = parent.kind if isinstance(parent, Item) else parent["kind"]
                allowed = {"album_item": {"album"}, "comment": {"note", "comment"},
                           "message": {"thread"}, "thread": {"message"}}
                if pkind not in allowed.get(item.kind, set()):
                    raise IntegrityError("來源項目類型不相符。")
        # Validate the merged graph once. Deep note replies must not cause a
        # quadratic walk or one SQL query for each ancestor of every new item.
        reverse={row["id"]:sid for sid,row in existing.items()}
        graph={sid:reverse.get(row["parent_id"]) for sid,row in existing.items()}
        if any(row["parent_id"] and row["parent_id"] not in reverse for row in existing.values()):
            raise IntegrityError("現有內容包含無效的所屬來源，請先檢查封存。")
        graph.update({item.source_id:item.parent_source_id for item in items})
        done=set()
        for start in graph:
            visited, node = set(), start
            while node:
                if node in done:break
                if node in visited:
                    raise IntegrityError("來源關聯形成循環，已停止更新。")
                visited.add(node)
                node=graph.get(node)
            done.update(visited)
        for item in items:
            iid = mapping[item.source_id]
            hidden = item.state in {"recalled", "deleted"}
            text = item.text if not hidden else ""
            self.db.execute("""INSERT INTO items VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
             ON CONFLICT(id) DO UPDATE SET kind=excluded.kind,parent_id=excluded.parent_id,
             reply_source_id=excluded.reply_source_id,text=excluded.text,title=excluded.title,
             sender_id=excluded.sender_id,sender_name=excluded.sender_name,sent_at=excluded.sent_at,
             state=excluded.state,ordinal=excluded.ordinal,metadata=excluded.metadata,seen_at=excluded.seen_at""",
             (iid,cid,item.source_id,item.kind,mapping.get(item.parent_source_id),item.reply_source_id,
              text,"" if hidden else item.title,item.sender_id,item.sender_name,item.sent_at,item.state,item.ordinal,
              json.dumps({} if hidden else item.metadata,ensure_ascii=False),utc_now()))
            if item.state in {"recalled", "deleted"}:
                self.db.execute("UPDATE attachments SET saved_before=1 WHERE item_id=?", (iid,))

    def put_attachment(self, cid: str, attachment: Attachment) -> str:
        iid = self.item_id(cid, attachment.item_source_id)
        if not self.db.execute("SELECT 1 FROM items WHERE id=? AND chat_id=?",(iid,cid)).fetchone():
            raise IntegrityError("附件找不到有效來源。")
        aid = key("attachment",cid,iid,attachment.source_id,attachment.variant,attachment.version)
        existing = self.db.execute("SELECT expected_size,expected_hash FROM attachments WHERE id=?", (aid,)).fetchone()
        if existing and ((existing["expected_size"] is not None and attachment.expected_size is not None and existing["expected_size"]!=attachment.expected_size)
                         or (existing["expected_hash"] and attachment.expected_sha256 and existing["expected_hash"].lower()!=attachment.expected_sha256)):
            raise IntegrityError("同一附件版本的內容資訊改變，請先核對來源；舊副本已保留。")
        self.db.execute("""INSERT INTO attachments
         (id,chat_id,source_id,variant,version,item_id,name,kind,status,expected_size,expected_hash)
         VALUES(?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET
         name=excluded.name,expected_size=coalesce(excluded.expected_size,attachments.expected_size),
         expected_hash=coalesce(excluded.expected_hash,attachments.expected_hash)""",
         (aid,cid,attachment.source_id,attachment.variant,attachment.version,iid,attachment.name,
          attachment.kind,"pending",attachment.expected_size,attachment.expected_sha256))
        row = self.db.execute("SELECT item_id,kind FROM attachments WHERE id=?",(aid,)).fetchone()
        if row["item_id"] != iid or row["kind"] != attachment.kind:
            raise IntegrityError("同一附件識別對應不同來源，已停止更新。")
        self.db.execute("""UPDATE attachments SET saved_before=1
         WHERE chat_id=? AND item_id=? AND source_id=? AND variant=? AND id<>?""",
         (cid,iid,attachment.source_id,attachment.variant,aid))
        return aid

    def reusable(self, aid: str, checkpoint=lambda: None) -> bool:
        row = self.db.execute("""SELECT a.*,b.path,b.sha256,b.size FROM attachments a
         LEFT JOIN blobs b ON b.id=a.blob_id WHERE a.id=?""",(aid,)).fetchone()
        if not row or row["status"] not in {"saved","preview"} or not row["path"]:
            return False
        try:
            sha,size = digest(within(self.root,row["path"]),checkpoint)
        except (OSError,IntegrityError):
            return False
        return ((sha,size) == (row["sha256"],row["size"])
          and (row["expected_size"] is None or size==row["expected_size"])
          and (not row["expected_hash"] or sha==row["expected_hash"].lower()))

    def reuse_known_content(self, aid: str, checkpoint=lambda: None) -> bool:
        row=self.db.execute("SELECT * FROM attachments WHERE id=?",(aid,)).fetchone()
        if not row or not row["expected_hash"]:
            return False
        blob=self.db.execute("SELECT * FROM blobs WHERE sha256=?",(row["expected_hash"].lower(),)).fetchone()
        if not blob:
            # Recover a fully copied object whose database commit was interrupted.
            # Only an exact source checksum permits this, never a filename or size.
            sha=row["expected_hash"].lower()
            for suffix in ("png","jpg","gif","webp","wav","ogg","mp3","mp4","bin"):
                rel="assets/"+sha[:2]+"/"+sha+"."+suffix
                try:observed,size=digest(within(self.root,rel),checkpoint)
                except (OSError,IntegrityError):continue
                if observed==sha and (row["expected_size"] is None or row["expected_size"]==size):
                    self.save_blob(aid,rel,sha,size,row["variant"]=="thumbnail",checkpoint)
                    return True
            return False
        try:sha,size=digest(within(self.root,blob["path"]),checkpoint)
        except (OSError,IntegrityError):return False
        if (sha,size)!=(blob["sha256"],blob["size"]) or row["expected_size"] is not None and row["expected_size"]!=size:
            return False
        self.db.execute("UPDATE attachments SET blob_id=?,status=?,reason='' WHERE id=?",
                        (blob["id"],"preview" if row["variant"]=="thumbnail" else "saved",aid))
        return True

    def save_blob(self, aid: str, path: str, sha: str, size: int, preview=False, checkpoint=lambda: None):
        attachment=self.db.execute("SELECT expected_size,expected_hash FROM attachments WHERE id=?",(aid,)).fetchone()
        if not attachment:
            raise IntegrityError("檔案找不到有效附件來源。")
        if ((attachment["expected_size"] is not None and attachment["expected_size"]!=size)
            or (attachment["expected_hash"] and attachment["expected_hash"].lower()!=sha)):
            raise IntegrityError("附件內容不符合原來源資訊，未列為完整副本。")
        if digest(within(self.root,path),checkpoint) != (sha,size):
            raise IntegrityError("附件副本檢查失敗。")
        bid = key("blob",sha)
        old = self.db.execute("SELECT * FROM blobs WHERE id=?", (bid,)).fetchone()
        if old and old["path"] != path:
            raise IntegrityError("相同檔案內容的儲存位置不一致，未建立重複副本。")
        self.db.execute("INSERT INTO blobs VALUES(?,?,?,?) ON CONFLICT(id) DO UPDATE SET size=excluded.size",(bid,path,sha,size))
        self.db.execute("UPDATE attachments SET blob_id=?,status=?,reason='' WHERE id=?",
                        (bid,"preview" if preview else "saved",aid))

    def issue(self, cid: str | None, code: str, detail: str):
        # Callers must pass fixed, content-free messages, never raw exceptions.
        self.db.execute("INSERT INTO issues(chat_id,code,detail,created_at) VALUES(?,?,?,?)",(cid,code,detail,utc_now()))

    def identity_problems(self, checkpoint=lambda: None) -> list[dict]:
        """Treat stored SQLite values as untrusted, before deriving any filenames."""
        problems = []
        specs = {
            "chats": (("id",), (), ("chat", "account", "source_id")),
            "items": (("id", "chat_id"), ("parent_id",), ("item", "chat_id", "source_id")),
            "attachments": (("id", "chat_id", "item_id"), ("blob_id", "expected_hash"),
                            ("attachment", "chat_id", "item_id", "source_id", "variant", "version")),
            "blobs": (("id", "sha256"), (), ("blob", "sha256")),
            "jobs": (("chat_id",), (), None),
            "issues": ((), ("chat_id",), None),
        }
        for table, (required, optional, identity) in specs.items():
            for row in self.db.execute("SELECT * FROM " + table):
                checkpoint()
                valid = all(canonical_hash(row[name]) for name in required)
                valid = valid and all(row[name] is None or canonical_hash(row[name]) for name in optional)
                if valid and identity:
                    values = [row[name] for name in identity[1:]]
                    valid = all(isinstance(value, str) for value in values)
                    valid = valid and row["id"] == key(identity[0], *values)
                if not valid:
                    # Never place a hostile identifier or path in the public reader.
                    problems.append({"code": "identity", "table": table,
                                     "detail": "封存識別或校驗欄位不正確，請保留原檔並使用可信副本。"})
                    break
        return problems

    def check(self, checkpoint=lambda: None, check_reader=True) -> list[dict]:
        problems = self.identity_problems(checkpoint)
        if problems:
            return problems
        if self.db.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            problems.append({"code":"database","detail":"資料庫結構檢查失敗。"})
        if list(self.db.execute("PRAGMA foreign_key_check")):
            problems.append({"code":"relations","detail":"資料關聯需要檢查。"})
        for row in self.db.execute("""SELECT i.id FROM items i JOIN items p ON p.id=i.parent_id
                     WHERE i.chat_id<>p.chat_id UNION
                     SELECT a.id FROM attachments a JOIN items i ON i.id=a.item_id WHERE a.chat_id<>i.chat_id"""):
            problems.append({"code":"relations", "id":row[0], "detail":"內容與所屬聊天室不相符。"})
        for row in self.db.execute("""SELECT c.id FROM chats c LEFT JOIN chats p
                     ON p.account=c.account AND p.source_id=c.parent_source_id
                     WHERE (c.kind='subchat' AND (p.id IS NULL OR p.kind<>'community'))
                        OR (c.parent_source_id IS NOT NULL AND c.kind<>'subchat')"""):
            problems.append({"code":"relations", "id":row[0], "detail":"子聊天室找不到所屬社群。"})
        for row in self.db.execute("SELECT id FROM items WHERE kind IN ('album_item','comment') AND parent_id IS NULL"):
            problems.append({"code":"relations", "id":row[0], "detail":"圖片或留言缺少所屬內容。"})
        graph={r["id"]:r for r in self.db.execute("SELECT id,parent_id,kind FROM items")}
        allowed={"album_item":{"album"},"comment":{"note","comment"},"message":{"thread"},"thread":{"message"}}
        done=set()
        for iid,row in graph.items():
            parent=graph.get(row["parent_id"])
            if parent and parent["kind"] not in allowed.get(row["kind"],set()):
                problems.append({"code":"relations","id":iid,"detail":"內容與所屬來源的類型不相符。"})
            visited=set();node=iid
            while node and node not in done:
                if node in visited:
                    problems.append({"code":"relations","id":iid,"detail":"來源關聯形成循環，需要檢查。"})
                    break
                visited.add(node);node=graph[node]["parent_id"] if node in graph else None
            done.update(visited)
        registered = set()
        for row in self.db.execute("SELECT id,path,sha256,size FROM blobs"):
            checkpoint()
            registered.add(row["path"])
            try:
                valid = digest(within(self.root,row["path"]),checkpoint) == (row["sha256"],row["size"])
            except (OSError,IntegrityError):
                valid = False
            if not valid:
                problems.append({"code":"file","id":row["id"],"detail":"檔案遺失或內容不符。"})
            if check_reader and (self.root/"index.html").exists() and valid:
                from .transport import cached_download
                if cached_download(self.root,row,checkpoint) is None:
                    problems.append({"code":"reader_data","id":row["id"],"detail":"離線另存資料不完整。請重新建立閱讀頁面，不需要從 LINE 下載。"})
        for row in self.db.execute("SELECT id FROM attachments WHERE status IN ('saved','preview') AND blob_id IS NULL"):
            problems.append({"code":"attachment","id":row["id"],"detail":"附件缺少完整副本。"})
        for row in self.db.execute("SELECT id FROM blobs WHERE id NOT IN (SELECT blob_id FROM attachments WHERE blob_id IS NOT NULL)"):
            problems.append({"code":"unreferenced", "id":row[0], "detail":"已保存的檔案未連到任何內容；仍保留副本。"})
        for row in self.db.execute("SELECT id FROM attachments WHERE status NOT IN ('saved','preview')"):
            problems.append({"code":"pending_attachment", "id":row[0], "detail":"附件尚未取得完整副本，可重新保存所屬聊天室補件。"})
        for row in self.db.execute("SELECT id FROM attachments WHERE status='preview'"):
            problems.append({"code":"preview_only", "id":row[0], "detail":"這份附件只有預覽圖，尚未取得完整原檔。"})
        assets = within(self.root, "assets")
        if assets.exists():
            # Explicit walk without following directory links or Windows junctions.
            import os
            for directory, dirs, files in os.walk(assets, followlinks=False):
                checkpoint()
                for name in list(dirs):
                    if is_link(Path(directory)/name):
                        dirs.remove(name)
                        problems.append({"code":"linked_file", "detail":"附件資料夾包含連結，未讀取連結內容。"})
                for name in files:
                    path = Path(directory)/name
                    if is_link(path):
                        problems.append({"code":"linked_file", "detail":"附件位置包含連結，未讀取連結內容。"})
                    elif path.relative_to(self.root).as_posix() not in registered:
                        problems.append({"code":"unregistered_file", "detail":"有檔案尚未登記到封存；仍保留原檔，不會自動刪除。"})
        # Useful local labels, without exposing raw source IDs or filesystem paths.
        for problem in problems:
            if not problem.get("id"):continue
            row=self.db.execute("""SELECT c.title AS chat,a.name AS filename FROM attachments a
                 JOIN chats c ON c.id=a.chat_id WHERE a.id=? OR a.blob_id=? LIMIT 1""",
                 (problem["id"],problem["id"])).fetchone()
            if row:problem.update(dict(row))
        return problems

    def summary(self) -> dict:
        counts = {r[0]:r[1] for r in self.db.execute("SELECT status,count(*) FROM attachments GROUP BY status")}
        return {"chats":self.db.execute("SELECT count(*) FROM chats").fetchone()[0],
                "items":self.db.execute("SELECT count(*) FROM items").fetchone()[0],
                "attachments":counts,"issues":self.db.execute("SELECT count(*) FROM issues").fetchone()[0]}
