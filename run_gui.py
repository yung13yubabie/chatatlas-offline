"""Source and packaged desktop entry point; startup errors stay on this computer."""
import os
import sys
from pathlib import Path


def main():
    if not getattr(sys, 'frozen', False):
        sys.path.insert(0, str(Path(__file__).resolve().parent / 'src'))
    try:
        from line_vault.gui import main as open_app
        open_app()
        return 0
    except Exception:
        message='程式尚未啟動。請確認使用含 Tkinter 的 Python 3.11 以上標準版，並保留整個程式資料夾。'
        if os.name=='nt':
            import ctypes
            ctypes.windll.user32.MessageBoxW(None,message,'LINE Vault',0x10)
        else:
            print(message)
        return 2


if __name__=='__main__':raise SystemExit(main())
