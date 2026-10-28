import argparse
import json
from pathlib import Path

from .errors import VaultError


def main():
    p=argparse.ArgumentParser(description="LINE Vault · 私人離線封存")
    from . import __version__
    p.add_argument("--version",action="version",version=__version__)
    sub=p.add_subparsers(dest="command")
    for name in ("demo","check","rebuild"):
        q=sub.add_parser(name);q.add_argument("directory",type=Path)
        if name=="demo":q.add_argument("--revision",type=int,choices=[1,2],default=1)
    q=sub.add_parser("backup");q.add_argument("directory",type=Path);q.add_argument("destination",type=Path)
    sub.add_parser("doctor")
    args=p.parse_args()
    try:
        if args.command is None:
            from .gui import main as gui
            gui();return 0
        if args.command=="doctor":
            from .windows.source import probe
            print(json.dumps(probe(),ensure_ascii=False,indent=2));return 0
        from .safeio import private_destination
        from .store import Store
        root=private_destination(args.directory)
        if args.command!="demo" and not (root/"archive.sqlite3").is_file():
            raise VaultError("這個資料夾內還沒有封存資料。")
        with Store(root) as store:
            if args.command=="demo":
                from .demo import SyntheticSource
                from .collector import Collector
                from .exporter import export_html
                source=SyntheticSource(args.revision)
                result=Collector(store,source).run(source.list_chats());export_html(store)
                print(json.dumps({"artificial_data":True,**result},ensure_ascii=False))
            elif args.command=="check":
                problems=store.check();print(json.dumps({"problems":problems,"summary":store.summary()},ensure_ascii=False,indent=2))
                return 1 if problems else 0
            elif args.command=="rebuild":
                from .exporter import export_html
                export_html(store);print("閱讀頁面已重建，沒有下載附件。")
            elif args.command=="backup":
                from .exporter import make_zip
                make_zip(store,args.destination);print("完整副本已保存。")
        return 0
    except VaultError as e:
        print(str(e));return 2
    except (ValueError,OSError):
        print("操作未完成，請檢查輸入與保存位置。");return 2


if __name__=="__main__":raise SystemExit(main())
