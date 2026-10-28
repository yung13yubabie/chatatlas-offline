@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
set "PYTHONPATH=%~dp0src"
where py >nul 2>nul
if errorlevel 1 goto try_python
py -3 -c "import sys,tkinter;raise SystemExit(0 if sys.version_info >= (3,11) else 1)" >nul 2>nul
if errorlevel 1 goto try_python
py -3 run_gui.py
goto done
:try_python
where python >nul 2>nul
if errorlevel 1 goto missing
python -c "import sys,tkinter;raise SystemExit(0 if sys.version_info >= (3,11) else 1)" >nul 2>nul
if errorlevel 1 goto missing
python run_gui.py
goto done
:missing
echo 此程式需要 Python 3.11 以上及 Tkinter。
echo 請從 https://www.python.org/downloads/windows/ 安裝標準版 Python，保留 Tcl/Tk 選項。
echo 安裝後再按兩下 Start.cmd。人工範例的 index.html 可以直接開啟。
pause
exit /b 1
:done
if errorlevel 1 pause
