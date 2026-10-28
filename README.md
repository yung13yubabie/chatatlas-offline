# 聊藏 ChatAtlas · LINE 離線知識地圖

**0.3.1-alpha.2 — 實驗預覽版，不是已完成的 LINE 備份工具。**

把本機封存內容整理成可探索的 HTML：點人物、主題或附件，查看相關原文、預覽圖片、另存檔案。閱讀頁不依賴外部 CDN、遠端模型或本機伺服器。

> **真實 LINE 自動採集尚未完成。** 沒有隨附已驗證的 LINE 版面設定；Windows GUI、完整採集與大檔保存仍待實機驗收。請勿把人工範例或成功編譯當成這些功能已通過。
>
> 此預覽版仍有[未完成檢查](docs/STATUS.md)，本次不包含深入獨立安全複核；不要用它處理唯一一份重要資料。

## 預覽

Windows x64 候選包內的 `ChatAtlas.exe` 是原生程式，使用者不需安裝 Python 或 Node.js。先解壓整包；人工範例可直接開啟 `人工範例/index.html`。請保留 HTML 旁邊的資料夾。

程式尚未簽章。不要為了執行它停用防毒、SmartScreen 或系統保護。Windows 版本相容性尚未做完整實機驗收。

## 隱私

- 收藏與附件留在本機；沒有內建雲端上傳、遙測或遠端模型。
- 封存**沒有加密或密碼保護**；請用作業系統權限保護，保留另一份備份。
- 不要把聊天、媒體、私人連線設定、資料庫、診斷日誌或憑證放進 Repo / issue / Release。
- 只使用已取得授權的對話；分享封存前先確認所有參與者與附件的隱私。
- 公開來源碼與候選包只允許來源、授權說明與人工生成範例；見 [安全與隱私](SECURITY.md)。

## 原始碼與建置

原生 Windows 程式在 `native/`；共用閱讀頁在 `src/line_vault/web/`。舊 Python 來源僅作歷史參考，不會被原生 EXE、CI 或發布管線載入。請使用 Release 的原生 EXE，不使用歷史 Python 啟動入口。

原生建置與打包需要 **Zig 0.14.1**、Node.js；Linux 核心測試另需 GNU C++20 和系統 SQLite。原生管線不使用 Python：

```sh
# Linux x64 下載並校驗指定 Zig
node tools/fetch_zig.mjs
ATLAS_ZIG="$PWD/.toolchains/zig-x86_64-linux-0.14.1/zig" node native/build.mjs

make -C native test build/atlas-cli
node tools/graph_model_test.cjs
npm ci --ignore-scripts
npx playwright install chromium
node tools/check_public_tree.mjs
node tools/package_test.mjs
node tools/package_release.mjs --output dist
```

全部 CI 採原生 C++、Node.js 與 PowerShell，不執行舊 Python 相容層或其測試。

Windows 開發者安裝相同版本 Zig 與 Node.js 後，可執行 `Build-Windows.cmd`。
詳細步驟見 [native/README.md](native/README.md)。

來源與說明放 Repo；可執行檔、範例包與校驗碼放 Release，不提交二進位到 Git。
打包只讀取經檢查的 Git 暫存區白名單，不遞迴複製整個工作目錄。

## 授權與驗證

- 專案來源：[MIT](LICENSE)；第三方與編譯器執行環境另見 [授權清單](THIRD_PARTY_NOTICES.md)。
- [目前狀態](docs/STATUS.md) · [測試紀錄](docs/TEST_RESULTS.md) · [相容性限制](docs/COMPATIBILITY.md)
- SHA-256 用於核對檔案，不代表防毒認證、安全認證或作者簽章。
