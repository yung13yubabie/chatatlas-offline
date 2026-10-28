@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
node native\build.mjs
if errorlevel 1 pause
