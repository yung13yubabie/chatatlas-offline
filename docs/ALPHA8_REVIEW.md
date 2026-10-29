# alpha.8 原生資料庫與附件發布核對

本版將 LINE 本機讀取與圖片、影片、檔案取得改為原生 EXE 路徑。使用者不需 Python、Node.js、MCP 或先匯出 JSON。原有 alpha.1 至 alpha.7 的 tag、Release 與發布檔案不變。

## 本版範圍

- 靜態編入固定雜湊的 SQLite3MultipleCiphers，以 wxSQLite3 相容的 AES-128-CBC 唯讀開啟明確選定的 `.edb`。
- 核對目前使用者與 LINE 發行者簽章；資料庫來源綁定路徑及檔案識別，偵測來源替換。
- 從同一個已核對 LINE 程序辨識具名登入工作階段，以原生 OBS／LEG Y 路徑嘗試取得附件授權與媒體。
- 使用 Windows 加密元件驗證及解密支援的 E2EENext 圖片、影片與檔案；原檔與預覽分開記錄，失敗保留待補狀態。
- 原檔已保存時，可省略的縮圖不再阻止完整 ZIP。另保留逐筆補入原檔功能。

目前沒有以真實 LINE 帳號驗證本版。缺少可辨識的工作階段、媒體金鑰，或附件已過期、遭伺服器拒絕時，仍可能無法取得。這不等於支援全部僅在 LINE 畫面內可見的媒體，也不包含相簿、記事本或全部雲端歷史回補。

## 上傳與清理

Repo 保留原始碼、人工測試、授權告示與固定雜湊的建置描述；EXE、ZIP、編譯輸出、工具鏈、快取與執行資料不進來源樹。真實 `.edb`／收藏資料庫、聊天、附件、金鑰、登入憑證與 `ChatAtlasData` 不得加入 Repo、issue 或 Release。

先前新增的 Python／JSON 匯出橋接及其 CI 已移除。本版也移除會引導使用者安裝、啟動 Python 的舊 `Start.cmd`。連帶的舊 Python 模組、測試、啟動與建置工具也已移除，僅保留原生版需要的共用網頁資源與 UIA 工作者。

單一 EXE 內嵌完整第三方授權告示。Windows ZIP 只包含原生執行檔、必要文件與人工離線範例，不夾帶 Python、Node.js、DLL 或開發工具。

## 驗證來源

版本更新前的原生程式來源為 `ea7b523376e17d6b0870ccdf7ee6555792e9fe49`，其 [Source CI](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40834861312) Linux、Windows 與瀏覽器工作全部成功。Windows 人工資料檢查包含 152 項 UI、42 項解密、89 項媒體、45 項資料庫來源、1,762 項辨識及 66 項 OBS／LEG Y 協定檢查，以及 EXE 系統 DLL、隔離 PATH 啟動與可攜搬移後重開。

這些是原生程式的基準驗證，不冒充 alpha.8 最終發布證據。加入版本、文件及發布流程後，必須對最終合併來源重跑檢查。

| alpha.8 最終證據 | 狀態 |
| --- | --- |
| 主線合併提交與來源檢查 | 待本次合併與精確來源 CI 完成後記錄。 |
| 原生建置與來源證明 | 待本次發布 workflow 完成後記錄。 |
| 實際封裝 EXE 的 Windows 啟動、關閉與可攜測試 | 待 `exact-release-smoke` 完成後記錄。 |
| Release 與校驗檔 | 以 [v0.3.1-alpha.8 Release](https://github.com/yung13yubabie/chatatlas-offline/releases/tag/v0.3.1-alpha.8) 的實際狀態與檔案為準。 |

## 發布產物

發布工作只上傳同一輪 CI 的原始位元組，先核對 `SHA256SUMS.txt`、來源證明與實際封裝 EXE，不以本機重包檔案替代：

- `ChatAtlas-0.3.1-alpha.8-Windows-x64.exe`
- `ChatAtlas-0.3.1-alpha.8-Windows-x64-candidate.zip`
- `ChatAtlas-0.3.1-alpha.8-source.zip`
- `ChatAtlas-0.3.1-alpha.8-CHECKLIST.txt`
- `SHA256SUMS.txt`

各檔案雜湊以本次 Release 的校驗檔為準，不複製 alpha.7 或本機測試包的值。Authenticode 程式碼簽章與真實 LINE 相容性驗收仍未完成；建置來源證明不代表這兩項已通過。
