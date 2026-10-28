"""Build on the Windows computer without a repository or policy changes."""
import os
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run(arguments, **kwargs):
    subprocess.run(arguments, cwd=ROOT, check=True, **kwargs)


def main():
    if os.name != "nt":
        print("請在 Windows 電腦執行建置。")
        return 2
    if not (3, 11) <= sys.version_info[:2] < (3, 14):
        print("建置需要 Python 3.11、3.12 或 3.13 標準版。")
        return 2
    try:
        import tkinter  # Ensure the chosen runtime includes Tcl/Tk.
        environment = ROOT / ".venv-build"
        run([sys.executable, "-m", "venv", str(environment)])
        python = str(environment / "Scripts/python.exe")
        run([python, "-m", "pip", "install", "--require-virtualenv", "pyinstaller==6.16.0", "."])
        env = dict(os.environ, PYTHONPATH=str(ROOT / "src"))
        run([python, "-m", "unittest", "discover", "-s", "tests", "-q"], env=env)
        env["LINE_VAULT_PARSE_FILE"] = str(ROOT / "src/line_vault/windows/uia_worker.ps1")
        parser = ("$tokens=$null;$errors=$null;[void][System.Management.Automation.Language.Parser]::ParseFile("
                  "$env:LINE_VAULT_PARSE_FILE,[ref]$tokens,[ref]$errors);if($errors.Count){exit 1}")
        powershell = str(Path(os.environ.get("SystemRoot", r"C:\Windows")) /
                         "System32/WindowsPowerShell/v1.0/powershell.exe")
        run([powershell, "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", parser], env=env)
        run([python, "-m", "PyInstaller", "--noconfirm", "--windowed", "--onedir", "--name", "LINEVault",
             "--paths", "src", "--add-data", "src/line_vault/web;line_vault/web", "--add-data",
             "src/line_vault/windows/uia_worker.ps1;line_vault/windows", "run_gui.py"])
        package = ROOT / "dist/LINEVault"
        for name in ("README.md", "SECURITY.md", "LICENSE"):
            shutil.copyfile(ROOT / name, package / name)
        shutil.copyfile(ROOT / "docs/USER_GUIDE.md", package / "USER_GUIDE.md")
        name = ROOT / "dist" / ("LINEVault-windows-development-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
        shutil.make_archive(str(name), "zip", root_dir=ROOT / "dist", base_dir="LINEVault")
        print("建置完成，程式包位於 dist 資料夾。真實 LINE 採集仍須通過本機相容性驗證。")
        return 0
    except (ImportError, OSError, subprocess.CalledProcessError):
        print("建置未完成。請檢查 Python 安裝、網路及上方結果；未產生新的可交付程式包。")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
