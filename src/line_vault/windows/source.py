import json
import time
from datetime import datetime
from pathlib import Path

from ..errors import CapabilityError, IntegrityError
from ..model import Attachment,Chat,Item,KINDS
from ..safeio import digest
from .bridge import WindowsBridge


def single(nodes,message="無法唯一辨識來源項目，已停止操作。"):
    if not isinstance(nodes,list):nodes=[nodes] if nodes else []
    if len(nodes)!=1:raise CapabilityError(message)
    return nodes[0]


def validate_profile(profile):
    if not isinstance(profile,dict) or profile.get("schema")!=1:
        raise CapabilityError("本機連線設定格式不相容。")
    def selector(value):
        if (not isinstance(value,dict) or not value or set(value)-{"automation_id","name","control_type"}
            or any(not isinstance(v,str) or not v or len(v)>1024 for v in value.values())):
            raise CapabilityError("連線設定內的畫面辨識欄位不完整。")
    def field(value,identity=False):
        if isinstance(value,str) and not identity:return
        if not isinstance(value,dict) or set(value)-{"selector","property","prefix","suffix","line"}:
            raise CapabilityError("連線設定內的內容欄位不正確。")
        if value.get("selector") is not None:selector(value["selector"])
        if value.get("property","name") not in {"name","text","automation_id"}:
            raise CapabilityError("連線設定使用不支援的來源欄位。")
        if any(not isinstance(value[k],str) for k in ("prefix","suffix") if k in value):
            raise CapabilityError("內容欄位的固定文字格式不正確。")
        if "line" in value and (type(value["line"]) is not int or not 0<=value["line"]<=4096):
            raise CapabilityError("內容欄位的行數格式不正確。")
    for name in ("account","chats","panels"):
        if not isinstance(profile.get(name),dict):
            raise CapabilityError("本機連線設定不完整，請先完成相容性設定。")
    account=profile["account"]
    if not account.get("selector") or not account.get("expected_value"):
        raise CapabilityError("尚未設定帳號核對方式，不能開始採集。")
    selector(account["selector"])
    if str(account.get("local_id", "")).startswith("synthetic-"):raise CapabilityError("連線設定使用了保留的範例識別。")
    if not isinstance(account.get("local_id"),str) or not account["local_id"] or not isinstance(account["expected_value"],str):
        raise CapabilityError("連線設定缺少本機帳號核對資訊。")
    if not profile["chats"].get("id") or not profile["chats"].get("row"):
        raise CapabilityError("尚未設定聊天室的穩定識別。")
    if not profile["chats"].get("selected_id"):
        raise CapabilityError("目前聊天室缺少穩定識別核對，不能只靠同名標題判斷。")
    chats=profile["chats"]
    for key in ("container","row","selected_title"):selector(chats.get(key))
    field(chats["id"],True);field(chats.get("title"))
    selected=chats["selected_id"]
    if not isinstance(selected,dict) or not selected.get("selector"):
        raise CapabilityError("目前聊天室缺少穩定識別核對。")
    field(selected,True)
    if "kind" in chats:field(chats["kind"])
    if "parent_source_id" in chats:field(chats["parent_source_id"],True)
    if not {"message","album","album_item","note","comment","thread"} <= profile["panels"].keys():
        raise CapabilityError("本機連線設定尚未涵蓋相簿、記事本或討論串，不能當成完整版採集。")
    for name, panel in profile["panels"].items():
        if name not in {"message","album","album_item","note","comment","thread"}:
            raise CapabilityError("連線設定含未知內容類型。")
        if not isinstance(panel,dict) or not isinstance(panel.get("fields"),dict) or not panel["fields"].get("source_id"):
            raise CapabilityError("內容來源缺少穩定識別，不能用猜測方式合併。")
        selector(panel.get("container"));selector(panel.get("row"))
        for key in ("open","close"):
            if key in panel:selector(panel[key])
        if set(panel["fields"])-{"source_id","text","sender_id","sender_name","sent_at","title","parent_source_id","reply_source_id","state","ordinal"}:
            raise CapabilityError("連線設定含不支援的內容欄位。")
        for key,value in panel["fields"].items():field(value,key in {"source_id","parent_source_id","reply_source_id","sender_id"})
        if name in {"album_item","comment"} and not panel["fields"].get("parent_source_id"):
            raise CapabilityError("圖片或留言缺少所屬內容的辨識方式。")
        if type(panel.get("max_scrolls",200)) is not int or not 1 <= panel.get("max_scrolls",200) <= 1000:
            raise CapabilityError("捲動範圍設定不正確。")
        if type(panel.get("scroll",False)) is not bool:
            raise CapabilityError("捲動設定格式不正確。")
        if not set(panel.get("chat_kinds",[]))<={"direct","group","room","community","subchat"}:
            raise CapabilityError("內容適用的聊天室類型不正確。")
        for media in panel.get("attachments",[]):
            if not isinstance(media,dict):raise CapabilityError("附件設定格式不正確。")
            selector(media.get("row"));selector(media.get("save_button"))
            fields=media.get("fields")
            if not isinstance(fields,dict) or not {"source_id","name"}<=fields.keys() or set(fields)-{"source_id","name","kind","variant","version","expected_size","expected_sha256"}:
                raise CapabilityError("附件缺少必要的來源辨識欄位。")
            for key,value in fields.items():field(value,key in {"source_id","version","expected_size","expected_sha256"})
    return profile


class WindowsSource:
    """Real UIA operations with an explicit, locally verified layout binding.

    No fabricated LINE selectors are shipped. A layout whose stable identities
    are not exposed is rejected rather than deduplicated by name or OCR.
    """
    def __init__(self,profile,bridge=None):
        self.profile=validate_profile(profile)
        self.bridge=bridge or WindowsBridge()
        self.info=self.bridge.call("connect")
        self.account_id=self.profile["account"].get("local_id","")
        if not self.account_id:
            self.close()
            raise CapabilityError("本機連線設定缺少帳號識別。")
        self._tokens={}
        self._downloads={}
        self.kinds=KINDS
        try:self.assert_account()
        except BaseException:
            self.close()
            raise

    def close(self):self.bridge.close()

    def set_kinds(self,kinds):self.kinds=frozenset(kinds)

    def wait_chat(self,chat):
        deadline=time.monotonic()+8
        while True:
            try:self.assert_chat(chat);return
            except CapabilityError:
                if time.monotonic()>=deadline:raise
                time.sleep(.1)

    def find(self,selector,root=None):
        if not isinstance(selector,dict) or not selector or set(selector)-{"automation_id","name","control_type"}:
            raise CapabilityError("控制項設定不正確。")
        result=self.bridge.call("find",selector=selector,root=root)
        return result if isinstance(result,list) else [result] if result else []

    def assert_account(self):
        p=self.profile["account"]
        node=single(self.find(p["selector"]))
        actual=self.bridge.call("text",token=node["token"])
        if actual!=p["expected_value"]:
            raise CapabilityError("LINE 帳號或畫面已改變，請重新核對連線。")

    def field(self,node,config):
        if isinstance(config,str):return config
        if not isinstance(config,dict):return None
        target=single(self.find(config["selector"],node["token"])) if config.get("selector") else node
        prop=config.get("property","name")
        if prop not in {"name","automation_id","text"}:
            raise CapabilityError("不使用暫時視窗編號作為來源識別。")
        value=self.bridge.call("text",token=target["token"]) if prop=="text" else target.get(prop,"")
        if value is None:return None
        if len(value)>262144:raise IntegrityError("來源內容過長，尚未更新。")
        if config.get("pattern"):
            raise CapabilityError("連線設定不支援正規式，請改用固定前後綴與控制項欄位。")
        if config.get("prefix"):
            if not value.startswith(config["prefix"]):
                raise CapabilityError("來源欄位格式與設定不相符，尚未更新。")
            value=value[len(config["prefix"]):]
        if config.get("suffix"):
            if not value.endswith(config["suffix"]):
                raise CapabilityError("來源欄位格式與設定不相符，尚未更新。")
            value=value[:-len(config["suffix"])]
        if config.get("line") is not None:
            lines=value.splitlines();n=int(config["line"])
            if n<0 or n>=len(lines):raise CapabilityError("來源欄位不完整。")
            value=lines[n]
        return value

    def list_chats(self):
        self.assert_account()
        config=self.profile["chats"]
        container=single(self.find(config["container"]))
        result=[]
        for row in self.find(config["row"],container["token"]):
            sid=self.field(row,config["id"])
            name=self.field(row,config["title"])
            if not sid or not name:raise CapabilityError("聊天室缺少可確認的名稱或識別。")
            kind=self.field(row,config.get("kind","group"))
            parent=self.field(row,config["parent_source_id"]) if config.get("parent_source_id") else None
            chat=Chat(sid,name,kind,parent)
            self._tokens[sid]=row["token"];result.append(chat)
        if len({c.source_id for c in result})!=len(result):
            raise IntegrityError("聊天室識別重複，尚未開始採集。")
        return result

    def select(self,chat):
        self.assert_account()
        # Rediscover stale visual handles while preserving exact stable identities.
        chats=self.list_chats()
        candidates=[c for c in chats if c.source_id==chat.source_id and c.title==chat.title]
        if len(candidates)!=1:raise CapabilityError("選取的聊天室已改變，請重新選擇。")
        self.bridge.call("activate")
        self.bridge.call("select",token=self._tokens[chat.source_id])
        self.wait_chat(chat)

    def assert_chat(self,chat):
        self.assert_account()
        node=single(self.find(self.profile["chats"]["selected_title"]))
        if self.bridge.call("text",token=node["token"])!=chat.title:
            raise CapabilityError("目前畫面不是選定的聊天室，已停止操作。")
        expected=self.profile["chats"]["selected_id"]
        node=single(self.find(expected["selector"]))
        value=self.field(node,{k:v for k,v in expected.items() if k!="selector"})
        if value!=chat.source_id:
            raise CapabilityError("目前聊天室識別已改變，已停止操作。")

    def read(self,chat,since,until,control):
        self.select(chat)
        items,attachments=[],[]
        seen={}
        for kind,config in self.profile["panels"].items():
            control.checkpoint();self.assert_chat(chat)
            if kind not in self.kinds:continue
            applicable=config.get("chat_kinds",["direct","group","room","community","subchat"])
            if chat.kind not in applicable:continue
            if config.get("open"):
                button=single(self.find(config["open"]))
                self.bridge.call("invoke_navigation",token=button["token"])
            container=single(self.find(config["container"]))
            unchanged=0
            for turn in range(int(config.get("max_scrolls",200))):
                control.checkpoint();self.assert_chat(chat)
                before=len(seen)
                for node in self.find(config["row"],container["token"]):
                    self.assert_chat(chat)
                    values={name:self.field(node,value) for name,value in config["fields"].items()}
                    sid=values.get("source_id")
                    if not sid:raise CapabilityError("內容缺少穩定識別，未覆蓋舊訊息。")
                    values["kind"]=kind
                    values["ordinal"]=int(values.get("ordinal") or (seen[sid].ordinal if sid in seen else len(seen)))
                    item=Item(**values)
                    if sid in seen and seen[sid]!=item:
                        raise IntegrityError("同一來源識別的內容不一致，未覆蓋舊資料。")
                    seen[sid]=item
                    for media in config.get("attachments",[]):
                        for candidate in self.find(media["row"],node["token"]):
                            fields={k:self.field(candidate,v) for k,v in media["fields"].items()}
                            fields["item_source_id"]=sid
                            if fields.get("expected_size") is not None:fields["expected_size"]=int(fields["expected_size"])
                            a=Attachment(**fields)
                            key=(chat.source_id,a.item_source_id,a.source_id,a.variant,a.version)
                            self._downloads[key]=(kind,media)
                            if a not in attachments:attachments.append(a)
                    self.assert_chat(chat)
                unchanged=unchanged+1 if before==len(seen) else 0
                if unchanged>=2 or not config.get("scroll",False):break
                if not self.bridge.call("scroll_up",token=container["token"]):break
                control.wait(.15)
                container=single(self.find(config["container"]))
            if config.get("close"):
                button=single(self.find(config["close"]))
                self.bridge.call("invoke_navigation",token=button["token"])
        items=list(seen.values())
        # Preserve context ancestors even if they fall outside the date range.
        keep={i.source_id for i in items if i.kind!="message" or not i.sent_at or not (
              (since and i.sent_at<since) or (until and i.sent_at>=until))}
        queue=list(keep)
        while queue:
            parent=seen[queue.pop()].parent_source_id
            if parent and parent not in keep:
                if parent not in seen:raise IntegrityError("來源未提供所屬內容，未建立孤立項目。")
                keep.add(parent);queue.append(parent)
        items=[i for i in items if i.source_id in keep]
        attachments=[a for a in attachments if a.item_source_id in keep]
        return items,attachments,"visible_only"

    def download(self,chat,attachment,destination,control):
        control.checkpoint();self.assert_chat(chat)
        key=(chat.source_id,attachment.item_source_id,attachment.source_id,attachment.variant,attachment.version)
        if key not in self._downloads:raise CapabilityError("附件目前不在可辨識的畫面中，請重新載入後補件。")
        kind,media=self._downloads[key]
        config=self.profile["panels"][kind]
        self.select(chat)
        if config.get("open"):
            button=single(self.find(config["open"]))
            self.bridge.call("invoke_navigation",token=button["token"])
        container=single(self.find(config["container"]))
        # Start at the oldest loaded position; locate by exact identities, not filenames.
        if config.get("scroll"):
            for _ in range(int(config.get("max_scrolls",200))):
                control.checkpoint();self.assert_chat(chat)
                if not self.bridge.call("scroll_up",token=container["token"]):break
                control.wait(.15)
                container=single(self.find(config["container"]))
        save=None
        for _ in range(int(config.get("max_scrolls",200))):
            control.checkpoint();self.assert_chat(chat)
            for node in self.find(config["row"],container["token"]):
                if self.field(node,config["fields"]["source_id"])!=attachment.item_source_id:continue
                for candidate in self.find(media["row"],node["token"]):
                    values={k:self.field(candidate,v) for k,v in media["fields"].items()}
                    values["item_source_id"]=attachment.item_source_id
                    if values.get("expected_size") is not None:values["expected_size"]=int(values["expected_size"])
                    observed=Attachment(**values)
                    if (observed.source_id,observed.variant,observed.version,observed.expected_size,observed.expected_sha256)==(
                         attachment.source_id,attachment.variant,attachment.version,attachment.expected_size,attachment.expected_sha256):
                        save=single(self.find(media["save_button"],candidate["token"]))
                        break
            if save or not config.get("scroll") or not self.bridge.call("scroll_down",token=container["token"]):break
            control.wait(.15)
            container=single(self.find(config["container"]))
        if not save:raise CapabilityError("目前無法重新定位這份附件，未改下載其他檔案。")
        self.assert_chat(chat)
        if attachment.expected_size is None and attachment.expected_sha256 is None:
            raise IntegrityError("來源缺少完整檔案的大小或校驗資訊，未啟動下載。")
        path=destination/"payload.bin"
        self.bridge.call("download",token=save["token"],destination=str(path))
        deadline=time.monotonic()+120;previous=None;stable=0
        while time.monotonic()<deadline:
            control.checkpoint()
            if path.is_file():
                stat=path.stat();observed=(stat.st_size,stat.st_mtime_ns)
                stable=stable+1 if observed==previous and (stat.st_size or attachment.expected_size==0) else 0
                previous=observed
                if stable>=4:
                    self.assert_chat(chat)
                    if attachment.expected_size is not None and stat.st_size!=attachment.expected_size:
                        control.stopped.wait(.25)
                        continue
                    if attachment.expected_size is None and attachment.expected_sha256 is None:
                        raise IntegrityError("缺少完整附件的大小或校驗資訊，未列為已保存。")
                    digest(path,control.checkpoint)
                    return path
            control.stopped.wait(.25)
        raise CapabilityError("附件下載尚未完成，可稍後補件。")


def probe():
    bridge=WindowsBridge()
    try:
        info=bridge.call("connect")
        shape=bridge.call("inspect")
        return {"version":info["version"],"publisher_verified":info["publisher_verified"],
                "accessible_lists":shape.get("control_types",{}).get("List",0),
                "capabilities":shape,
                "integration_verified":False}
    finally:bridge.close()
