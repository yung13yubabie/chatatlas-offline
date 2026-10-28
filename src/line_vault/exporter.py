import base64
import hashlib
import json
import shutil
import uuid
import zipfile
from pathlib import Path

from .errors import IntegrityError
from .model import utc_now
from .safeio import atomic_bytes, digest, is_link, safe_name, within
from .store import Store

WEB=Path(__file__).parent/"web"
from .transport import DOWNLOAD_CHUNK,prepare_download


def download_parts(store,blob,checkpoint=lambda: None):
    return prepare_download(store.root,blob,checkpoint)


def safe_json(value) -> str:
    return json.dumps(value,ensure_ascii=False,separators=(",",":" )).replace("<","\\u003c").replace("&","\\u0026").replace("\u2028","\\u2028").replace("\u2029","\\u2029")


def zip_file(archive,path,name,checkpoint):
    info=zipfile.ZipInfo.from_file(path,name,strict_timestamps=False)
    with path.open("rb") as source,archive.open(info,"w",force_zip64=True) as target:
        import os
        before=os.fstat(source.fileno())
        while chunk:=source.read(1024*1024):
            checkpoint();target.write(chunk)
        after=os.fstat(source.fileno())
        if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):
            raise IntegrityError("製作副本時檔案內容改變，尚未完成副本。")


def export_html(store: Store, checkpoint=lambda: None) -> Path:
    problems=store.check(checkpoint,check_reader=False)
    if any(p["code"] in {"identity", "database", "relations"} for p in problems):
        raise IntegrityError("封存識別或關聯不正確，未建立閱讀頁面。")
    generation=uuid.uuid4().hex
    relative="reader/"+generation
    folder=within(store.root,relative)
    folder.mkdir(parents=True,mode=0o700)
    invalid={p.get("id") for p in problems if p["code"]=="file"}
    catalog=[];downloads={};portable=[]
    for blob in store.db.execute("SELECT * FROM blobs ORDER BY id"):
        if blob["id"] in invalid:continue
        checkpoint()
        entry=download_parts(store,blob,checkpoint)
        downloads[blob["sha256"]]=entry
        portable.extend([blob["path"],"downloads/"+blob["sha256"]+"/info.json"]+[p["path"] for p in entry["parts"]])
    for chat in store.db.execute("SELECT c.id,c.title,c.kind,c.account,p.id AS parent_id FROM chats c LEFT JOIN chats p ON p.account=c.account AND p.source_id=c.parent_source_id ORDER BY c.title,c.id"):
        items=[];chunks=[];count=0
        def publish():
            name=chat["id"]+"-"+str(len(chunks))+".js"
            payload="window.VaultData.add("+safe_json(chat["id"])+","+safe_json(items)+");\n"
            atomic_bytes(within(folder,name),payload.encode())
            chunks.append(relative+"/"+name);portable.append(relative+"/"+name)
        for row in store.db.execute("SELECT * FROM items WHERE chat_id=? ORDER BY sent_at,ordinal,id",(chat["id"],)):
            checkpoint()
            # Public reader needs no source account identifiers or raw metadata.
            item={k:row[k] for k in ("id","kind","parent_id","reply_source_id","source_id","text","title","sender_name","sent_at","state","ordinal")}
            item["chat_id"]=chat["id"]
            if row["sender_id"]:
                from .store import key
                item["sender_ref"]=key("person",chat["account"],row["sender_id"])
            metadata=json.loads(row["metadata"])
            if isinstance(metadata.get("tags"),list):
                item["tags"]=[t for t in metadata["tags"] if isinstance(t,str) and 2<=len(t)<=32][:12]
            item["attachments"]=[]
            for a in store.db.execute("""SELECT a.*,b.path,b.size,b.sha256 FROM attachments a
              LEFT JOIN blobs b ON b.id=a.blob_id WHERE a.item_id=? ORDER BY a.id""",(row["id"],)):
                valid=a["blob_id"] not in invalid and bool(a["path"])
                item["attachments"].append({"id":a["id"],"name":safe_name(a["name"]),"kind":a["kind"],
                  "status":a["status"] if valid else ("failed" if a["status"] in {"saved","preview"} else a["status"]),
                  "path":a["path"] if valid and a["status"] in {"saved","preview"} else None,
                  "download":a["sha256"] if valid and a["status"] in {"saved","preview"} else None,
                  "size":a["size"],"reason":a["reason"],"saved_before":bool(a["saved_before"])})
            items.append(item)
            count+=1
            if len(items)==500:publish();items=[]
        if items:publish()
        latest=store.db.execute("SELECT status,coverage,finished_at,counts FROM jobs WHERE chat_id=? ORDER BY id DESC LIMIT 1",(chat["id"],)).fetchone()
        latest=dict(latest) if latest else None
        if latest:latest["counts"]=json.loads(latest["counts"])
        catalog.append({"id":chat["id"],"title":chat["title"],"kind":chat["kind"],"count":count,
                        "chunks":chunks,"parent_id":chat["parent_id"],"last_job":latest})
    accounts=[r[0] for r in store.db.execute("SELECT DISTINCT account FROM chats")]
    manifest={"version":1,"created_at":utc_now(),"chats":catalog,"summary":store.summary(),"downloads":downloads,
              "artificial_data":bool(accounts) and all(a in {"synthetic-local-account","synthetic-native-account"} for a in accounts),
              "problems":problems,"generation":generation}
    script="\n".join((WEB/name).read_text(encoding="utf-8") for name in ("graph-model.js","graph-view.js","download.js","reader.js"))
    css=(WEB/"reader.css").read_text(encoding="utf-8")
    script_hash=base64.b64encode(hashlib.sha256(script.encode()).digest()).decode()
    css_hash=base64.b64encode(hashlib.sha256(css.encode()).digest()).decode()
    template=(WEB/"index.html").read_text(encoding="utf-8")
    document=template.replace("@@CSP_SCRIPT@@",script_hash).replace("@@CSP_STYLE@@",css_hash)
    document=document.replace("@@STYLE@@",css).replace("@@MANIFEST@@",safe_json(manifest)).replace("@@SCRIPT@@",script)
    # A single atomic entrypoint switch exposes only an entire generation.
    checkpoint()
    atomic_bytes(within(store.root,"index.html"),document.encode())
    atomic_bytes(within(store.root,"archive-info.json"),json.dumps({"version":1,"created_at":manifest["created_at"],
                 "summary":manifest["summary"],"problems":problems,"portable_files":sorted(set(portable))},ensure_ascii=False,indent=2).encode())
    # Retire only tool-generated old reader directories. Attachments are retained.
    reader=within(store.root,"reader")
    for old in reader.iterdir():
        if old.name != generation and len(old.name)==32 and all(c in "0123456789abcdef" for c in old.name):
            if old.is_dir() and not is_link(old):
                shutil.rmtree(old)
    return store.root/"index.html"


def make_zip(store: Store, destination: Path, attachment_ids: list[str] | None=None, checkpoint=lambda: None):
    if store.identity_problems(checkpoint):
        raise IntegrityError("封存識別或校驗欄位不正確，未建立副本。")
    destination=Path(destination).absolute()
    if destination.resolve().is_relative_to(store.root.resolve()):
        raise IntegrityError("壓縮副本請存到封存資料夾以外。")
    if destination.exists():
        raise IntegrityError("目的地已有同名檔案，請選擇其他名稱。")
    destination.parent.mkdir(parents=True,exist_ok=True)
    selected=set(attachment_ids) if attachment_ids is not None else None
    partial=destination.with_name(destination.name+".partial")
    if partial.exists():
        raise IntegrityError("目的地有尚未完成的檔案，請選擇其他名稱。")
    try:
        with zipfile.ZipFile(partial,"x",compression=zipfile.ZIP_STORED,allowZip64=True) as z:
            if selected is None:
                problems=store.check(checkpoint)
                if any(p["code"] in {"identity","database","relations","file","attachment","linked_file"} for p in problems):
                    raise IntegrityError("封存有缺檔或關聯問題，請先檢查與補件，再建立完整副本。")
                export_html(store,checkpoint)
                # Consistent SQLite backup, excluding WAL, temporary downloads and profiles.
                import sqlite3,tempfile
                with tempfile.TemporaryDirectory(prefix="vault-backup-") as temp:
                    target=Path(temp)/"archive.sqlite3"
                    conn=sqlite3.connect(target)
                    try:store.db.backup(conn,pages=128,progress=lambda *_:checkpoint())
                    finally:conn.close()
                    zip_file(z,target,"archive.sqlite3",checkpoint)
                metadata=json.loads((store.root/"archive-info.json").read_text(encoding="utf-8"))
                for rel in ["index.html","archive-info.json",*metadata["portable_files"]]:
                    checkpoint()
                    zip_file(z,within(store.root,rel),rel,checkpoint)
            else:
                rows={r["id"]:r for r in store.db.execute("""SELECT a.*,b.path,b.sha256,b.size FROM attachments a
                      LEFT JOIN blobs b ON b.id=a.blob_id""")}
                if not selected <= rows.keys():
                    raise IntegrityError("選取的附件不存在。")
                for aid in sorted(selected):
                    checkpoint()
                    r=rows[aid]
                    if not store.reusable(aid,checkpoint):raise IntegrityError("選取的附件有缺件，未產生不完整壓縮包。")
                    zip_file(z,within(store.root,r["path"]),aid[:12]+"_"+safe_name(r["name"]),checkpoint)
        partial.rename(destination)
    except BaseException:
        partial.unlink(missing_ok=True)
        raise
