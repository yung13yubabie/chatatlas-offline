from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any

KINDS = frozenset({"message", "album", "album_item", "note", "comment", "thread"})
STATES = frozenset({"active", "recalled", "deleted", "unknown"})
MEDIA_STATES = frozenset({"pending", "saved", "preview", "unavailable", "failed"})


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def timestamp(value: str | None) -> str | None:
    if value is None:
        return None
    if not isinstance(value, str):
        raise ValueError("invalid timestamp")
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None:
        raise ValueError("timestamp must include a timezone")
    return parsed.astimezone(timezone.utc).isoformat(timespec="milliseconds")


@dataclass(frozen=True)
class Chat:
    source_id: str
    title: str
    kind: str = "group"
    parent_source_id: str | None = None

    def __post_init__(self):
        if not isinstance(self.source_id,str) or not self.source_id or not isinstance(self.title,str) or not self.title or self.kind not in {"direct", "group", "room", "community", "subchat"}:
            raise ValueError("invalid chat identity or kind")
        if self.kind == "subchat" and not self.parent_source_id:
            raise ValueError("subchat requires community identity")


@dataclass(frozen=True)
class Item:
    source_id: str
    kind: str
    text: str = ""
    sender_id: str | None = None
    sender_name: str = ""
    sent_at: str | None = None
    title: str = ""
    parent_source_id: str | None = None
    reply_source_id: str | None = None
    state: str = "active"
    ordinal: int = 0
    metadata: dict[str, Any] = field(default_factory=dict)

    def __post_init__(self):
        if not isinstance(self.source_id,str) or not self.source_id or self.kind not in KINDS or self.state not in STATES:
            raise ValueError("invalid item")
        if not all(isinstance(v,str) for v in (self.text,self.title,self.sender_name)) or not isinstance(self.ordinal,int):
            raise ValueError("invalid item fields")
        object.__setattr__(self, "sent_at", timestamp(self.sent_at))


@dataclass(frozen=True)
class Attachment:
    source_id: str
    item_source_id: str
    name: str
    kind: str = "file"
    variant: str = "full"
    version: str = ""
    expected_size: int | None = None
    expected_sha256: str | None = None

    def __post_init__(self):
        if not self.source_id or not self.item_source_id:
            raise ValueError("attachment must have a stable identity and parent")
        if self.kind not in {"image", "video", "audio", "file", "sticker"}:
            raise ValueError("invalid media kind")
        if self.expected_size is not None and (type(self.expected_size) is not int or self.expected_size < 0):
            raise ValueError("invalid size")
        if self.variant not in {"full", "thumbnail"}:
            raise ValueError("invalid variant")
        if self.expected_sha256 is not None:
            import re
            if not re.fullmatch(r"[a-fA-F0-9]{64}", self.expected_sha256):
                raise ValueError("invalid checksum")
            if not self.version:
                object.__setattr__(self,"version","sha256:"+self.expected_sha256.lower())
            object.__setattr__(self,"expected_sha256",self.expected_sha256.lower())
