"""Verified local script parts for downloads from a file:// reader."""
import base64
import hashlib
import json
import re

from .errors import IntegrityError
from .safeio import atomic_bytes, canonical_hash, digest, within

DOWNLOAD_CHUNK=768*1024


def safe_json(value):
    return json.dumps(value,ensure_ascii=True,separators=(",",":"))


def cached_download(root,blob,checkpoint=lambda: None):
    sha=blob["sha256"]
    if not canonical_hash(sha) or type(blob["size"]) is not int or blob["size"] < 0:
        return None
    expected=max(1,(blob["size"]+DOWNLOAD_CHUNK-1)//DOWNLOAD_CHUNK)
    try:
        info=within(root,"downloads/"+sha+"/info.json")
        limit=expected*400+4096
        with info.open("r",encoding="utf-8") as source:raw=source.read(limit+1)
        if len(raw)>limit:return None
        cached=json.loads(raw)
        if (cached["sha256"]==sha and cached["size"]==blob["size"]
            and len(cached["parts"])==expected
            and sum(p["size"] for p in cached["parts"])==blob["size"]
            and all(p["path"]=="downloads/"+sha+"/"+str(n)+".js" and re.fullmatch(r"[a-f0-9]{64}",p["sha256"])
                    and type(p["size"]) is int and 0<=p["size"]<=DOWNLOAD_CHUNK for n,p in enumerate(cached["parts"]))
            and all(digest(within(root,p["path"]),checkpoint)[0]==p["script_sha256"] for p in cached["parts"])):
            return cached
    except (OSError,ValueError,KeyError,TypeError,IntegrityError):pass
    return None


def prepare_download(root,blob,checkpoint=lambda: None):
    sha=blob["sha256"]
    if not canonical_hash(sha) or type(blob["size"]) is not int or blob["size"] < 0:
        raise IntegrityError("附件校驗欄位不正確，未建立下載資料。")
    cached=cached_download(root,blob,checkpoint)
    if cached is not None:return cached
    directory=within(root,"downloads/"+sha)
    directory.mkdir(parents=True,exist_ok=True,mode=0o700)
    parts=[];whole=hashlib.sha256();size=0
    with within(root,blob["path"]).open("rb") as source:
        number=0
        while True:
            checkpoint()
            chunk=source.read(DOWNLOAD_CHUNK)
            if not chunk and number:break
            whole.update(chunk);size+=len(chunk)
            payload=("window.VaultBytes.add("+safe_json(sha)+","+str(number)+","+
                     safe_json(base64.b64encode(chunk).decode("ascii"))+");\n").encode("ascii")
            rel="downloads/"+sha+"/"+str(number)+".js"
            atomic_bytes(within(root,rel),payload)
            parts.append({"path":rel,"sha256":hashlib.sha256(chunk).hexdigest(),
                          "script_sha256":hashlib.sha256(payload).hexdigest(),"size":len(chunk)})
            number+=1
            if not chunk:break
    if (whole.hexdigest(),size)!=(sha,blob["size"]):
        raise IntegrityError("附件在建立閱讀頁面時改變，尚未更新入口。")
    result={"sha256":sha,"size":size,"parts":parts}
    atomic_bytes(within(root,"downloads/"+sha+"/info.json"),safe_json(result).encode("ascii"))
    return result
