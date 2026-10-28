import json
import os
import queue
import threading
import tkinter as tk
import webbrowser
from pathlib import Path
from tkinter import filedialog,messagebox,ttk

from .collector import Collector,Control,CaptureOptions
from .demo import SyntheticSource
from .errors import VaultError,Cancelled
from .exporter import export_html,make_zip
from .model import timestamp
from .safeio import atomic_bytes,private_dir,private_destination
from .store import Store
from . import __version__


def config_dir():
    base=Path(os.environ.get("LOCALAPPDATA",Path.home()/".local/share"))
    return private_dir(base/"LINEVault")


class App:
    def __init__(self,root):
        self.root=root;self.source=None;self.chats=[];self.control=Control();self.events=queue.Queue();self.busy=False
        root.title("LINE Vault "+__version__+" · 聊天與附件離線封存");root.geometry("1040x790");root.minsize(850,690)
        style=ttk.Style();style.theme_use("clam")
        style.configure("TFrame",background="#f5f7f5");style.configure("TLabel",background="#f5f7f5",foreground="#2d4134",font=("Microsoft JhengHei",10))
        style.configure("Title.TLabel",font=("Microsoft JhengHei",21,"bold"))
        style.configure("Hint.TLabel",foreground="#617467",font=("Microsoft JhengHei",9))
        style.configure("TButton",font=("Microsoft JhengHei",10),padding=(12,8))
        style.configure("Treeview",font=("Microsoft JhengHei",10),rowheight=30)
        style.configure("Treeview.Heading",font=("Microsoft JhengHei",10,"bold"))
        outer=ttk.Frame(root,padding=24);outer.pack(fill="both",expand=True)
        ttk.Label(outer,text="我的 LINE 離線封存",style="Title.TLabel").pack(anchor="w")
        ttk.Label(outer,text="選擇聊天室，保存訊息與完整附件。離線閱讀不需要登入或密碼。",style="Hint.TLabel").pack(anchor="w",pady=(5,20))
        self.tabs=ttk.Notebook(outer);self.tabs.pack(fill="both",expand=True)
        self.capture=ttk.Frame(self.tabs,padding=18);self.archive=ttk.Frame(self.tabs,padding=18);self.issues=ttk.Frame(self.tabs,padding=18);self.settings=ttk.Frame(self.tabs,padding=18)
        for frame,name in [(self.capture,"開始保存"),(self.archive,"我的封存"),(self.issues,"需處理項目"),(self.settings,"連線設定")]:self.tabs.add(frame,text=name)
        self.status=tk.StringVar(value="尚未連接 LINE。你也可以使用人工範例先查看離線閱讀功能。")
        self.path=tk.StringVar();self.filter=tk.StringVar();self.profile_path=config_dir()/"connection.local.json"
        self.build_capture();self.build_archive();self.build_settings()
        self.issue_text=tk.Text(self.issues,wrap="word",font=("Microsoft JhengHei",10),state="disabled",background="#fff",relief="flat")
        self.issue_text.pack(fill="both",expand=True)
        ttk.Label(outer,textvariable=self.status,style="Hint.TLabel",wraplength=950).pack(anchor="w",pady=(14,0))
        root.after(100,self.pump);root.protocol("WM_DELETE_WINDOW",self.quit)

    def row(self,parent):
        r=ttk.Frame(parent);r.pack(fill="x",pady=5);return r

    def build_capture(self):
        r=self.row(self.capture)
        self.connect_button=ttk.Button(r,text="連接 LINE",command=self.connect);self.connect_button.pack(side="left")
        ttk.Button(r,text="使用人工範例",command=self.demo).pack(side="left",padx=10)
        ttk.Label(r,text="範例不會讀取你的 LINE。",style="Hint.TLabel").pack(side="left")
        r=self.row(self.capture);ttk.Label(r,text="搜尋聊天室").pack(side="left",padx=(0,12))
        ttk.Entry(r,textvariable=self.filter).pack(fill="x",expand=True);self.filter.trace_add("write",lambda *_:self.draw_chats())
        r=self.row(self.capture)
        ttk.Button(r,text="全選目前清單",command=lambda:self.tree.selection_set(self.tree.get_children())).pack(side="left")
        ttk.Button(r,text="取消選取",command=lambda:self.tree.selection_remove(self.tree.selection())).pack(side="left",padx=8)
        ttk.Label(r,text="按 Ctrl 可多選。",style="Hint.TLabel").pack(side="left",padx=8)
        self.tree=ttk.Treeview(self.capture,columns=("name","type"),show="headings",selectmode="extended",height=7)
        self.tree.heading("name",text="聊天室／對象");self.tree.heading("type",text="類型");self.tree.column("name",width=620);self.tree.column("type",width=130)
        self.tree.pack(fill="both",expand=True)
        r=self.row(self.capture);ttk.Label(r,text="保存內容").pack(side="left",padx=(0,12))
        self.kind_vars={}
        for name,kinds in [("對話與討論串",{"message","thread"}),("相簿",{"album","album_item"}),("記事本與留言",{"note","comment"})]:
            var=tk.BooleanVar(value=True);self.kind_vars[tuple(sorted(kinds))]=var
            ttk.Checkbutton(r,text=name,variable=var).pack(side="left",padx=(0,12))
        r=self.row(self.capture);ttk.Label(r,text="訊息期間").pack(side="left",padx=(0,10))
        self.since=tk.StringVar();self.until=tk.StringVar()
        ttk.Entry(r,textvariable=self.since,width=13).pack(side="left");ttk.Label(r,text=" 至 ").pack(side="left");ttk.Entry(r,textvariable=self.until,width=13).pack(side="left")
        ttk.Label(r,text="YYYY-MM-DD；空白代表可取得範圍",style="Hint.TLabel").pack(side="left",padx=12)
        r=self.row(self.capture);ttk.Label(r,text="保存位置").pack(side="left",padx=(0,10));ttk.Entry(r,textvariable=self.path).pack(side="left",fill="x",expand=True);ttk.Button(r,text="選擇資料夾",command=self.choose_path).pack(side="left",padx=(8,0))
        ttk.Label(self.capture,text="自動載入可能改變已讀狀態；保存時請保持 LINE 視窗可用。",style="Hint.TLabel").pack(anchor="w",pady=8)
        r=self.row(self.capture)
        self.start=ttk.Button(r,text="開始保存／更新",command=self.collect);self.start.pack(side="left")
        self.pause=ttk.Button(r,text="暫停",command=self.toggle_pause,state="disabled");self.pause.pack(side="left",padx=8)
        self.stop=ttk.Button(r,text="停止並保留",command=lambda:self.control.stop(),state="disabled");self.stop.pack(side="left")
        self.progress=ttk.Progressbar(r,mode="indeterminate",length=150);self.progress.pack(side="right")

    def build_archive(self):
        ttk.Label(self.archive,text="開啟或維護已保存的資料",style="Title.TLabel").pack(anchor="w",pady=(0,14))
        ttk.Label(self.archive,text="先選擇封存資料夾。重新建立閱讀頁面不需要重新下載附件。",wraplength=800).pack(anchor="w",pady=(0,12))
        for name,fn in [("選擇封存資料夾",self.choose_path),("開啟離線閱讀",self.open_reader),("檢查檔案與資料關聯",self.check),("重新建立閱讀頁面",self.rebuild),("製作完整 ZIP 副本",self.backup)]:
            ttk.Button(self.archive,text=name,command=fn).pack(anchor="w",pady=7)
        self.summary=tk.StringVar(value="尚未選擇封存資料夾。")
        ttk.Label(self.archive,textvariable=self.summary,wraplength=850).pack(anchor="w",pady=22)

    def build_settings(self):
        ttk.Label(self.settings,text="本機連線與相容性",style="Title.TLabel").pack(anchor="w",pady=(0,12))
        ttk.Label(self.settings,text="程式版本 "+__version__,style="Hint.TLabel").pack(anchor="w",pady=4)
        ttk.Label(self.settings,text="LINE 的可讀取欄位會隨版本改變。此版本尚未完成真實 Windows LINE 整合驗收；來源不符時會停止，不會用人工資料代替。",wraplength=850).pack(anchor="w",pady=8)
        ttk.Button(self.settings,text="檢查目前 LINE 連線",command=self.probe).pack(anchor="w",pady=8)
        self.connection_detail=tk.StringVar(value="檢查會顯示 LINE 版本及可讀取範圍，不會傳出聊天內容。")
        ttk.Label(self.settings,textvariable=self.connection_detail,wraplength=800).pack(anchor="w",pady=8)
        ttk.Button(self.settings,text="另存不含聊天內容的檢查結果",command=self.save_probe).pack(anchor="w",pady=8)
        ttk.Label(self.settings,text="進階連線設定只保存在本機，可能含帳號名稱及畫面識別，請勿公開上傳。",style="Hint.TLabel",wraplength=850).pack(anchor="w",pady=8)
        ttk.Button(self.settings,text="載入本機連線設定",command=self.load_profile).pack(anchor="w",pady=8)
        ttk.Label(self.settings,text="資料保護：不使用遠端模型、不自動回報、不讀取 LINE 程序記憶體、不要求管理員權限。",wraplength=850).pack(anchor="w",pady=18)

    def task(self,fn,stoppable=False):
        if self.busy:return
        if stoppable:
            self.control=Control();self.pause.configure(state="normal");self.stop.configure(state="normal")
        self.busy=True;self.start.configure(state="disabled");self.connect_button.configure(state="disabled");self.progress.start(12)
        def worker():
            try:self.events.put(("result",fn()))
            except Cancelled:self.events.put(("result","工作已停止，原封存與已完成內容仍會保留。"))
            except VaultError as exc:self.events.put(("error",str(exc)))
            except Exception:self.events.put(("error","操作未完成，已保存的內容仍在。請檢查保存位置或本機連線。"))
            finally:self.events.put(("idle",None))
        threading.Thread(target=worker,daemon=True).start()

    def pump(self):
        try:
            while True:
                kind,value=self.events.get_nowait()
                if kind=="result" and value:self.status.set(str(value))
                elif kind=="error":self.status.set(value);messagebox.showerror("尚未完成",value)
                elif kind=="chats":self.chats=value;self.draw_chats()
                elif kind=="connection":self.connection_detail.set(value)
                elif kind=="summary":self.summary.set(value)
                elif kind=="issues":
                    self.issue_text.configure(state="normal");self.issue_text.delete("1.0","end")
                    self.issue_text.insert("end","\n\n".join(" · ".join(str(p[k]) for k in ("chat","filename") if p.get(k))+"\n"+p["detail"] for p in value) if value else "檔案與資料關聯檢查通過。")
                    self.issue_text.configure(state="disabled");self.tabs.select(self.issues)
                elif kind=="progress":
                    stage=value.get("stage")
                    if stage=="files":self.status.set("正在保存附件 "+str(value["current"])+" / "+str(value["total"])+"；完整舊檔會直接沿用。")
                    elif stage=="retry":self.status.set("連線暫時中斷，正在第 "+str(value["attempt"])+" 次嘗試。")
                    else:self.status.set({"read":"正在讀取選定內容…","done":"本次保存工作完成，正在建立離線閱讀頁面…"}.get(stage,"正在處理…"))
                elif kind=="idle":
                    self.busy=False;self.progress.stop();self.start.configure(state="normal");self.connect_button.configure(state="normal");self.pause.configure(state="disabled",text="暫停");self.stop.configure(state="disabled")
        except queue.Empty:pass
        self.root.after(100,self.pump)

    def draw_chats(self):
        selected=set(self.tree.selection())
        self.tree.delete(*self.tree.get_children())
        kinds={"group":"群組","direct":"好友","room":"多人聊天","community":"社群","subchat":"社群子聊天室"}
        for i,c in enumerate(self.chats):
            if self.filter.get().casefold() in c.title.casefold():
                sid=str(i);self.tree.insert("","end",iid=sid,values=(c.title,kinds[c.kind]))
                if sid in selected:self.tree.selection_add(sid)

    def choose_path(self):
        if self.busy:return
        value=filedialog.askdirectory(title="選擇私人封存資料夾")
        if value:
            try:private_destination(value)
            except VaultError as e:messagebox.showerror("請更換位置",str(e));return
            self.path.set(value)

    def load_profile(self):
        if self.busy:return
        value=filedialog.askopenfilename(title="選擇本機連線設定",filetypes=[("連線設定","*.json")])
        if not value:return
        try:
            from .windows.source import validate_profile
            with Path(value).open("rb") as selected:raw=selected.read(131073)
            if len(raw)>131072:raise VaultError("連線設定過大。")
            validate_profile(json.loads(raw))
            atomic_bytes(self.profile_path,raw)
            self.status.set("本機連線設定已保存。請連接 LINE 進行核對。")
        except (ValueError,OSError,VaultError):messagebox.showerror("無法載入","連線設定不完整或無法讀取，請查看相容性文件。")

    def probe(self):
        def work():
            from .windows.source import probe
            r=probe();self.last_probe=r
            message="LINE 版本："+str(r["version"])+"。發行者已核對，可辨識清單 "+str(r["accessible_lists"])+" 個。完整採集仍須實機驗證。"
            self.events.put(("connection",message));return "連線檢查完成。"
        self.task(work)

    def save_probe(self):
        if self.busy:return
        if not getattr(self,"last_probe",None):self.status.set("請先執行 LINE 連線檢查。");return
        dest=filedialog.asksaveasfilename(title="另存檢查結果",defaultextension=".json",initialfile="line-connection-report.json",filetypes=[("檢查結果","*.json")])
        if not dest:return
        try:
            atomic_bytes(Path(dest),json.dumps(self.last_probe,ensure_ascii=False,indent=2).encode())
            self.status.set("檢查結果已保存，不含聊天文字、對象名稱、帳號或檔案路徑。")
        except (OSError,VaultError):messagebox.showerror("無法保存","請選擇一般資料夾後重試。")

    def connect(self):
        if not self.profile_path.exists():
            self.tabs.select(self.settings);self.status.set("此 LINE 版本尚未有已驗證的連線設定，請先檢查相容性。");return
        def work():
            from .windows.source import WindowsSource
            if self.source and hasattr(self.source,"close"):self.source.close()
            self.source=WindowsSource(json.loads(self.profile_path.read_text(encoding="utf-8")))
            self.events.put(("chats",self.source.list_chats()));return "已列出可辨識的聊天室，請選擇要保存的項目。"
        self.task(work)

    def demo(self):
        if self.busy:return
        if self.source and hasattr(self.source,"close"):self.source.close()
        self.source=SyntheticSource();self.chats=self.source.list_chats();self.draw_chats();self.status.set("目前使用人工範例，沒有讀取真實 LINE。")

    def date(self,value,following=False):
        if not value:return None
        from datetime import datetime,timedelta,timezone
        try:d=datetime.strptime(value,"%Y-%m-%d").replace(tzinfo=timezone(timedelta(hours=8)))
        except ValueError:raise VaultError("日期請使用 YYYY-MM-DD。") from None
        if following:d+=timedelta(days=1)
        return timestamp(d.isoformat())

    def collect(self):
        if self.busy:return
        if not self.source:self.status.set("請先連接 LINE 或選擇人工範例。");return
        selection=[self.chats[int(i)] for i in self.tree.selection()]
        if not selection:self.status.set("請先選擇聊天室。");return
        if not self.path.get():self.choose_path()
        if not self.path.get():return
        try:
            root=private_destination(self.path.get());since=self.date(self.since.get());until=self.date(self.until.get(),True)
            options=CaptureOptions(frozenset(k for kinds,v in self.kind_vars.items() if v.get() for k in kinds))
        except VaultError as e:messagebox.showerror("請檢查設定",str(e));return
        def work():
            with Store(root) as store:
                try:
                    result=Collector(store,self.source,self.control,lambda e:self.events.put(("progress",e)),options).run(selection,since,until)
                except Cancelled:
                    export_html(store)
                    return "保存已停止；已完成的資料已保留，可開啟離線閱讀。"
                except VaultError:
                    export_html(store)
                    raise
                export_html(store)
                self.show_summary(store)
                problems=store.check()
                if problems:self.events.put(("issues",problems))
                return "本次更新 "+str(result["items"])+" 筆內容；沿用 "+str(result["reused"])+" 個附件，新增 "+str(result["downloaded"])+" 個，需處理 "+str(result["failed"]+result["unavailable"])+" 個。"
        self.task(work,stoppable=True)

    def show_summary(self,store):
        r=store.summary();a=r["attachments"]
        self.events.put(("summary",str(r["chats"])+" 個聊天室 · "+str(r["items"])+" 筆內容 · "+str(a.get("saved",0))+" 個完整附件 · "+str(a.get("preview",0))+" 個預覽 · "+str(sum(v for k,v in a.items() if k not in {"saved","preview"}))+" 個待補件"))

    def toggle_pause(self):
        if self.control.running.is_set():self.control.pause();self.pause.configure(text="繼續");self.status.set("保存已暫停，已完成的內容仍會保留。")
        else:self.control.resume();self.pause.configure(text="暫停")

    def archive_root(self):
        if not self.path.get():raise VaultError("請先選擇封存資料夾。")
        path=private_destination(self.path.get())
        if not (path/"archive.sqlite3").is_file():raise VaultError("這個資料夾內還沒有封存資料。")
        return path

    def open_reader(self):
        try:
            p=self.archive_root()/"index.html"
            if not p.is_file():raise VaultError("請先重新建立閱讀頁面。")
            webbrowser.open(p.resolve().as_uri())
        except VaultError as e:messagebox.showerror("無法開啟",str(e))

    def check(self):
        # Tk variables are read on the UI thread, never inside the worker.
        try:root=self.archive_root()
        except VaultError as e:messagebox.showerror("無法檢查",str(e));return
        def work_safe():
            with Store(root) as store:problems=store.check(self.control.checkpoint);self.show_summary(store)
            self.events.put(("issues",problems));return "檢查完成："+str(len(problems))+" 個需處理項目。"
        self.task(work_safe,stoppable=True)

    def rebuild(self):
        try:root=self.archive_root()
        except VaultError as e:messagebox.showerror("無法重建",str(e));return
        def work():
            with Store(root) as store:export_html(store,self.control.checkpoint)
            return "閱讀頁面已重建，沒有重新下載附件。"
        self.task(work,stoppable=True)

    def backup(self):
        try:root=self.archive_root()
        except VaultError as e:messagebox.showerror("無法建立副本",str(e));return
        dest=filedialog.asksaveasfilename(title="製作完整副本",defaultextension=".zip",filetypes=[("ZIP","*.zip")])
        if not dest:return
        def work():
            with Store(root) as store:make_zip(store,Path(dest),checkpoint=self.control.checkpoint)
            return "完整副本已保存。"
        self.task(work,stoppable=True)

    def quit(self):
        if self.busy:
            self.control.stop();self.status.set("正在停止；已完成資料會保留。請稍後關閉。");return
        if self.source and hasattr(self.source,"close"):self.source.close()
        self.root.destroy()


def main():
    root=tk.Tk();App(root);root.mainloop()
