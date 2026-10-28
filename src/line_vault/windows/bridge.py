import json
import os
import queue
import subprocess
import threading
from pathlib import Path

from ..errors import CapabilityError

MESSAGES={
 "focus_changed":"LINE 視窗已切換，請切回原視窗後繼續。",
 "line_window_ambiguous":"請只保留一個已登入的 LINE 主視窗。",
 "publisher_unverified":"無法核對 LINE 的發行者，已停止連線。",
 "save_dialog_missing":"LINE 未顯示可辨識的另存視窗，未取得完整檔案。",
 "save_dialog_unsupported":"目前的另存視窗尚未驗證，未自動填入路徑。",
 "scroll_unsupported":"目前視窗無法自動捲動，取得範圍可能不完整。",
 "tree_limit":"LINE 畫面項目過多，請縮小目前顯示範圍。",
}


class WindowsBridge:
    def __init__(self):
        if os.name!="nt":
            raise CapabilityError("連接 LINE 需要在 Windows 電腦上執行。")
        systemroot=os.environ.get("SystemRoot",r"C:\Windows")
        executable=Path(systemroot)/"System32/WindowsPowerShell/v1.0/powershell.exe"
        script=(Path(__file__).parent/"uia_worker.ps1").read_text(encoding="utf-8-sig")
        # Static, bundled code only; neither UI strings nor profile values become code.
        self.process=subprocess.Popen([str(executable),"-NoLogo","-NoProfile","-NonInteractive","-Command",script],
             stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,
             text=True,encoding="utf-8",creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
        self.responses=queue.Queue()
        self.lock=threading.Lock()
        def read():
            for line in self.process.stdout:
                if len(line)>16*1024*1024:
                    self.responses.put(None);break
                self.responses.put(line)
            self.responses.put(None)
        threading.Thread(target=read,daemon=True).start()

    def call(self,action,**values):
        with self.lock:
            if self.process.poll() is not None:
                raise CapabilityError("本機連線已停止，請重新連接 LINE。")
            self.process.stdin.write(json.dumps({"action":action,**values},ensure_ascii=True)+"\n")
            self.process.stdin.flush()
            try:line=self.responses.get(timeout=35)
            except queue.Empty:
                self.close()
                raise CapabilityError("LINE 操作逾時，已停止本機連線。") from None
            if not line:
                raise CapabilityError("本機連線無法啟動，請查看相容性說明。")
            try:r=json.loads(line)
            except ValueError:
                raise CapabilityError("本機連線回傳格式不正確。") from None
            if not r.get("ok"):
                raise CapabilityError(MESSAGES.get(r.get("code"),"目前無法安全完成 LINE 操作，已停止。"))
            return r.get("result")

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:self.process.kill()
        for stream in (self.process.stdin,self.process.stdout):
            if stream:stream.close()
