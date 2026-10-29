# 匯入 LINE 本機訊息

這是原始碼新增的資料來源，需使用包含此功能的建置版本。既有 `v0.3.1-alpha.7` Release 與 ZIP 保持不變，沒有「載入 LINE 本機匯出」按鈕。

資料流程為：

```text
Windows 上已登入的 LINE 與明確選定的本機 .edb
  → line-summary 的授權唯讀 API
  → 私人的 JSON 匯出檔
  → 聊藏的聊天室選擇與收藏流程
  → 離線閱讀頁與知識地圖
```

聊藏原本的 `archive.sqlite3` 是自己的收藏資料庫，不是 LINE 資料庫。這次透過 [line-summary](https://github.com/yung13yubabie/line-summary) 讀取來源，再交給原生程式整理；沒有把加密金鑰寫入 JSON，也沒有在聊藏 EXE 內新增 `.edb` 解密器。

## 先準備 line-summary

請在自己的 Windows 電腦使用已能運作的 line-summary 環境，並保持 LINE 登入。其本機資料庫讀取目前依賴 Windows LINE，不適用於 macOS、Linux 或 WSL。

依 line-summary 文件設定專案的虛擬環境與私人 `settings.json`，確認：

- `enabled` 已明確啟用。
- `db_path` 指向要讀取的帳號資料庫。
- `allowed_chat_ids` 包含這次要匯出的聊天室 ID。
- 要求的日期、每次回應與整個工作階段的預算允許這次讀取。

匯出工具不替你選取最大的 `.edb`、不修改這些設定，也不重啟服務來避開拒絕或用盡的額度。第一次讀取可能需要掃描 LINE 記憶體取得解密金鑰，因此可能等候數分鐘。若 line-summary 不能讀取目前的 LINE 版本，這個匯出工具也不能補足該能力。

## 匯出一個聊天室

以下使用 PowerShell。將範例路徑、聊天室 ID 與名稱替換成自己的值；先準備 Repo 之外的私人資料夾，再將輸出檔放在其中。

```powershell
& 'C:\Tools\line-summary\.venv\Scripts\python.exe' `
  'C:\Tools\chatatlas-offline\tools\export_line_summary.py' `
  --line-summary 'C:\Tools\line-summary' `
  --account-id 'my-line-account' `
  --chat-id 'YOUR_ALLOWED_CHAT_ID' `
  --chat-title '要保存的聊天室' `
  --chat-kind group `
  --since '2026-10-01T00:00:00+08:00' `
  --until '2026-10-02T00:00:00+08:00' `
  --limit 5000 `
  --output 'C:\Private\line-export.json'
```

請使用 **line-summary 已有的 Python 虛擬環境**執行工具，因為來源讀取需要它的相依套件。聊藏 EXE 載入 JSON 與閱讀離線地圖時不需要 Python；這個匯出步驟另有環境需求。

- 每個檔案只包含一個明確指定的聊天室。`--chat-kind` 可用 `group`、`direct`、`room` 或 `community`。
- `--account-id` 是你指定的穩定帳號代號，不是程式驗證過的 LINE 帳號 ID。同一個 LINE 帳號請持續使用相同代號；不同帳號請用不同代號，避免混合來源。不要填入金鑰或密碼。
- `--chat-title` 是你指定的顯示名稱；存取權限仍由聊天室 ID 與 line-summary 設定決定。
- 起訖時間都必須含時區，範圍為 **`[since, until)`**：包含開始時刻、不包含結束時刻。上例涵蓋台灣時間 10 月 1 日一整天。
- 起訖時間支援毫秒精度或更低精度；工具會拒絕非零的次毫秒部分，避免匯入收藏時改變範圍邊界。
- `--limit` 預設為 5,000 則。到達上限時若來源還有下一頁，工具會中止，不產生看似完整的檔案。可縮小日期範圍，或在既有授權與預算內明確提高上限。
- 匯出檔上限為 64 MiB；超出時請拆分日期範圍。

工具沿用 `line_get_history` 的固定聊天室、日期與分頁游標，取得所選範圍內本機可讀取的訊息。來源拒絕、額度用盡、回應截斷、無法前進的分頁或輸出限制都會使這次匯出失敗，不會發布部分檔案或覆寫既有輸出。請依實際錯誤處理，不要把失敗視為聊天室沒有資料。

## 在聊藏整理

1. 開啟由新版原始碼建置的 `ChatAtlas.exe`。
2. 在「整理 LINE」按「載入 LINE 本機匯出」，選取剛才的 JSON。
3. 勾選聊天室，確認保存位置。程式會選取對話文字並清空日期選擇器；日期留空時，使用匯出檔內精確的起訖範圍。需要時可透過日期選擇器縮小範圍，超出匯出範圍則會拒絕整理。這個來源不提供相簿或記事本。
4. 按「開始整理」，使用既有收藏流程產生離線閱讀頁與地圖。

載入 JSON 不需要 `connection.local.json`，也不會操作 LINE 視窗。要更新內容，請重新從 line-summary 匯出，再載入聊藏。匯入到同一份收藏、保持相同帳號代號與聊天室 ID 時，相同訊息 ID 會沿用或更新，不會每次另加一份。更換帳號代號會建立不同來源。

命令列開發環境也可使用：

```sh
native/build/atlas-cli line-summary /private/line-export.json /private/collection
native/build/atlas-cli check /private/collection
```

第一個參數是匯出檔，第二個是新收藏或既有收藏的位置。CLI 同樣只處理匯出檔中的本機範圍，不會自行連接 LINE。

## 保存範圍與限制

| 內容 | 這條匯入路徑的處理方式 |
| --- | --- |
| 本機已同步的文字訊息 | 保存原文、顯示名稱、時間與來源訊息 ID。 |
| 非文字訊息 | 保存來源資訊與類型標示；未下載圖片、影片或其他附件原檔。 |
| 社群討論串 | 以一般訊息平面保存，討論串 ID 留在收藏資料庫的中繼資料，不重建討論串階層；不保證取得日期範圍外的根訊息。 |
| 相簿、記事本與其留言 | 尚未涵蓋。 |
| 雲端歷史、已過期或未同步的內容 | 不會回補；本機讀取完成不代表完整 LINE 備份。 |

這個來源有發訊者顯示名稱，但缺少穩定的發訊者 ID，程式不會由名稱推定人物身分或合併同名者。採集範圍會標記為本機資料；匯出檔的日期範圍是這次可以整理的邊界。

JSON 與收藏都包含私人原文且沒有加密。請勿加入 Git、issue 或 Release；分享前先確認內容適合交付。完成匯入後是否保留 JSON，由你自行決定。

此功能以人工資料驗證匯出分頁、拒絕情況、原生匯入與收藏流程。這些測試不會連接真實 LINE，也不代表目前 Windows／LINE 版本已完成實機驗收；真實讀取仍取決於 line-summary 的相容性與本機授權。
