"""Explicit, deterministic artificial data. Never used as a live LINE fallback."""
import hashlib
import struct
import zlib
from pathlib import Path

from .model import Attachment,Chat,Item


def png():
    width,height=160,100
    rows=[]
    for y in range(height):
        row=bytearray()
        for x in range(width):
            row.extend((53+int(x/5),101+int(y/5),76+int((x+y)/10)))
        rows.append(b"\x00"+bytes(row))
    def chunk(tag,data):
        return struct.pack(">I",len(data))+tag+data+struct.pack(">I",zlib.crc32(tag+data)&0xffffffff)
    return b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">2I5B",width,height,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(b"".join(rows)))+chunk(b"IEND",b"")


class SyntheticSource:
    account_id="synthetic-local-account"
    def __init__(self,revision=1):
        self.revision=revision
        self.downloads=0
        self.kinds=None

    def set_kinds(self,kinds):
        self.kinds=kinds

    def list_chats(self):
        return [Chat("example-project","範例專案工作群"),Chat("example-community","範例社群","community")]

    def read(self,chat,since,until,control):
        control.checkpoint()
        date="2026-01-12T09:00:00+08:00"
        items=[Item("m1","message", "這是人工建立的範例。已存附件在再次保存時會沿用。" if self.revision==1 else "第二次更新的內容；同一則訊息沒有重複出現。",sender_name="範例成員 A",sent_at=date),
          Item("m2","message","圖片與文件都可以離線另存。",sender_name="範例成員 B",sent_at="2026-01-12T09:02:00+08:00",reply_source_id="m1"),
          Item("al1","album",title="範例相簿",sent_at=date),
          Item("p1","album_item",title="範例圖片",parent_source_id="al1",sender_name="範例成員 A",sent_at=date),
          Item("n1","note","記事本內文也能搜尋。\n所有人物及內容均為人工資料。",title="範例記事本",sender_name="範例成員 A",sent_at=date),
          Item("c1","comment","這是一則範例留言。",parent_source_id="n1",sender_name="範例成員 B",sent_at=date)]
        if chat.kind=="community":
            items.extend([Item("t1","thread",title="範例討論串",parent_source_id="m1",sent_at=date),
                          Item("r1","message","討論串回覆保留自己的來源關聯。",parent_source_id="t1",sender_name="範例暱稱",sent_at=date)])
        payload=b"This is synthetic example content. No private data.\n"
        media=[Attachment("file1","m2","範例文件.txt",expected_size=len(payload),expected_sha256=hashlib.sha256(payload).hexdigest()),
               Attachment("image1","p1","範例圖片.png","image",expected_size=len(png()),expected_sha256=hashlib.sha256(png()).hexdigest())]
        if self.kinds is not None:
            items=[i for i in items if i.kind in self.kinds]
            keep={i.source_id for i in items}
            media=[a for a in media if a.item_source_id in keep]
        return items,media,"source_range"

    def download(self,chat,attachment,destination,control):
        control.checkpoint();self.downloads+=1
        p=destination/"payload.bin"
        p.write_bytes(png() if attachment.kind=="image" else b"This is synthetic example content. No private data.\n")
        return p
