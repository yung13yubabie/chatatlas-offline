# 聊藏原生開發

目前版本：0.3.1-alpha.1。原生 Windows EXE 不啟動 Python 或 Node.js；兩者只作建置／開發用途。

## Windows x64 交叉編譯

使用 [Zig 0.14.1](https://ziglang.org/download/) 與 Node.js：

```sh
node native/build.mjs
```

從 Repo 根目錄執行。若 Zig 不在 PATH，設定 `ATLAS_ZIG`。
建置腳本拒絕其他 Zig 版本，將閱讀頁與靜態 PowerShell 工作者嵌入 EXE，
輸出 `native/out/ChatAtlas.exe` 及來源輸入雜湊 `build-provenance.json`。
執行檔尚未簽章。

在 Linux x64，`python3 tools/fetch_zig.py` 可下載官方校驗碼固定的工具鏈；
需要 Python 3.12+ 安全解壓支援。這不是程式執行時的下載行為。

## Linux 核心測試

需要 GNU C++20、make、系統 SQLite：

```sh
make -C native test
make -C native build/atlas-cli
```

暫存收藏不能放在 Git 樹內。若環境的暫存根目錄本身被標成 Git 資料夾，
請在正常的非 Git 私人測試目錄執行；不要刪除或停用保護。

nlohmann/json 3.12.0 標頭與其授權已完整附在 `vendor/`；
Windows SQLite 只從固定的系統目錄載入。編譯器執行環境告示見根目錄
`licenses/` 及 `THIRD_PARTY_NOTICES.md`。

## 打包

先完成測試、暫存明確允許公開的來源，執行 `python3 tools/check_public_tree.py`，
再執行 `python3 tools/package_release.py --output <候選輸出資料夾>`。
打包不代表授權公開；未執行的深入複核及未完成的 Windows 與 LINE 驗收見 `docs/STATUS.md`。

舊 Python 建置只作相容性開發，入口改名為 `tools/build_legacy_windows.py`，
不得冒充本版原生 EXE。
