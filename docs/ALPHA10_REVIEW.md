# alpha.10 原生 LINE 自動抓取、批次來源與 TXT

本版讓 EXE 直接尋找並整理可讀的 LINE 本機資料庫，不要求先手動匯出聊天或安裝 Python、Node.js、MCP 等執行套件。alpha.10 已公開發布；alpha.9 與更早版本的 tag、Release 和檔案不覆寫，既有證據保留於 [alpha.9 核對紀錄](ALPHA9_REVIEW.md) 等歷史文件。

## 修改範圍

- 首頁新增「自動抓取」：使用既有的常見位置尋找，逐份核對可讀來源，整理全部聊天室的對話／討論串與附件，再自動產生 TXT 和離線閱讀頁。使用目前日期、文字保存方式與收藏位置，不使用手動聊天室或內容類型勾選。
- 「批次選擇資料庫…」支援勾選、全選、清除與完整路徑；手動檔案視窗支援 Ctrl／Shift 多選 `.edb`／`.db`，一次最多 64 份。連線後仍可自行選聊天室。
- 對每個已核對簽章與工作階段的 LINE 程序共用一次金鑰掃描，逐份匹配資料庫；不混用不同程序的金鑰與登入工作階段。不可讀、重複及格式不支援的來源會略過並報告。
- 批次清單顯示資料庫來源，各來源仍使用原本的單庫帳號與聊天室識別，保留去重、採集紀錄與附件歸屬；不因名稱、訊息 ID 或金鑰相同而合併來源。可讀來源可能是歷史副本，不代表全部屬於目前登入帳號。
- 整理後為選定聊天室各產生 TXT，保存於 `text-exports/<新批次>/`，不覆寫先前匯出。文字來自本次選定聊天室在收藏中所有已保存的對話／討論串，可能含先前期間；日期只限制本次抓取，不刪舊文字。附件原檔另存，TXT 只列名稱及已核對的保存狀態，不輸出原始 `metadata` 欄位、登入資料或金鑰。既有完整收藏 ZIP 不包含新的 `text-exports`，TXT 需另行保存。

這是直接讀取資料庫的免手動匯出流程，沒有操作 LINE 內建「儲存聊天」選單，不構成原有 UI Automation 已適配的證據。相簿、記事本、雲端歷史回補及所有僅在 LINE 可見的媒體仍不宣稱完整支援；真實 LINE 帳號與附件下載相容性仍待實機驗證。詳見 [LINE 本機資料庫說明](LINE_DATABASE_IMPORT.md)。

## 本版驗證與發布

發行來源固定為 [`3ef487ee17030e387733dfa2a6ff05dd61ab7015`](https://github.com/yung13yubabie/chatatlas-offline/commit/3ef487ee17030e387733dfa2a6ff05dd61ab7015)，tag `v0.3.1-alpha.10` 直接指向這個提交。後續文件與截圖提交不會改動已發布的 tag 或檔案，也不冒充 EXE 的建置來源。

| alpha.10 證據 | 結果 |
| --- | --- |
| 最終主線來源與公開來源檢查 | 發行來源 `3ef487ee`，222 個 Git 追蹤來源檔；公開來源檢查零發現，保留必要來源、測試與授權。 |
| Source CI | [本次工作](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40859729627) 的 Linux、Windows 與瀏覽器三個工作全部成功。 |
| 批次與 TXT | 本次人工資料測試通過 39 項批次來源與 30 項 TXT 檢查，涵蓋來源隔離、單庫識別沿用、文字範圍、附件狀態與取消行為。 |
| Windows 操作與既有原生回歸 | Windows 通過 313 項 UI、40 項候選尋找、1772 項金鑰／程序、42 項加密資料庫、89 項媒體、45 項資料庫來源與 66 項 OBS 檢查。 |
| 原生建置與來源證明 | [發布流程](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40859729726) 四個工作全部成功；[來源證明](https://github.com/yung13yubabie/chatatlas-offline/attestations/61253269) 已逐檔核對五個產物皆來自同一發行來源。 |
| 實際封裝 EXE | [exact-release-smoke](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40859729726/job/123408526110) 成功，驗證實際套件及 EXE 的 Windows 啟動、關閉與可攜行為。 |
| Release | [v0.3.1-alpha.10](https://github.com/yung13yubabie/chatatlas-offline/releases/tag/v0.3.1-alpha.10) 於 2026-10-29 11:04:30 UTC 公開發布為預覽版，僅包含下列五檔。 |

## 公開產物讀回核對

五個檔案已從公開 Release 網址另外下載，大小與 SHA-256 均與該輪建置 CI 的原始輸出一致，包含 `SHA256SUMS.txt` 本身。來源證明亦逐檔驗證發行來源為 `3ef487ee`。

公開 Windows ZIP 通過消費者套件契約；單一 EXE 與 ZIP 內執行檔位元組相同、保有完整內嵌授權，沒有執行環境套件。來源 ZIP 恰含發行提交的 222 個 Git 追蹤檔與 `SOURCE_SHA256SUMS.txt`，共 223 項；獨立逐檔位元組、SHA-256 與完整校驗清單核對通過，沒有額外私人資料、編譯產物、執行環境包或舊 Python 實作，公開來源檢查零發現。

另核對 alpha.9 的五個資產，其識別、大小、雜湊與發布時間保持原值，沒有覆寫。

README 的[本版首頁截圖](screenshots/windows-native-alpha10.png) 來自發行來源 `3ef487ee` 的同次 [exact-release-smoke](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40859729726/job/123408526110)，SHA-256 為 `0ccd98ef1bc3a99ceedc8be70b8044a6970af0de7f909609eeb6ab9b26429f7b`；人工環境尚未連接 LINE，不是實機擷取證明。圖片只隨後續文件提交保存，不加入 Release 或重包產物。

## 發布檔案

- `ChatAtlas-0.3.1-alpha.10-Windows-x64.exe`
- `ChatAtlas-0.3.1-alpha.10-Windows-x64-candidate.zip`
- `ChatAtlas-0.3.1-alpha.10-source.zip`
- `ChatAtlas-0.3.1-alpha.10-CHECKLIST.txt`
- `SHA256SUMS.txt`

本版只採用通過本次驗證的原始 CI 產物，沒有沿用舊版測試包或本機重包檔案。驗證使用人工目錄、資料庫、程序候選與附件，不代表所有 Windows LINE 帳號或僅在 LINE 可見的媒體皆已完成實機驗收；原有相容性限制仍保留。
