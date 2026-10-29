# alpha.9 LINE 來源尋找與首頁操作

本版處理「不知道 LINE 資料庫在哪裡、開啟後不知道先按哪裡」的使用問題。alpha.8 的原生資料庫與附件功能保留；不要求 Python、JSON 匯出或額外執行環境。alpha.8 與更早版本的 tag、Release 及檔案不覆寫，既有驗證見 [alpha.8 核對紀錄](ALPHA8_REVIEW.md)。

## 修改範圍

- 首頁入口改為「尋找並連接 LINE」，先顯示常見位置的資料庫候選。
- 尋找範圍限目前使用者的 `%LocalAppData%\LINE\Data\db` 及下方最多兩層子資料夾，不掃描整台電腦、不讀取檔案內容來猜帳號。
- 只列出非空的 `.edb` 候選，排除已知 `album_`、`keep_`、`chatStats_` 類別；候選不是已驗證的帳號或可解密資料庫。
- 顯示完整路徑。單一候選可預選，但仍須確認；多個候選由使用者選擇，不自動挑最大的檔案。
- 手動選擇保留，優先開啟可用的常見 LINE 位置。
- 將下方欄位明確標為「收藏保存位置」，區分 LINE 來源 `.edb` 與聊藏輸出的資料夾。

確認來源後仍沿用原生金鑰、程序、檔案識別及資料庫格式檢查。自動找到候選不代表已登入、帳號相符或附件必定可下載；真實 LINE 版本與帳號尚需實機驗證。相簿、記事本、全部雲端歷史與所有僅在 LINE 開啟的媒體仍不宣稱已完整支援。

## 本版驗證與發布

發行來源固定為 [`76817baab51b7630f3b0cf3313b06451cea9a740`](https://github.com/yung13yubabie/chatatlas-offline/commit/76817baab51b7630f3b0cf3313b06451cea9a740)，tag `v0.3.1-alpha.9` 直接指向這個提交。後續文件提交不會改動已發布的 tag 或檔案，也不冒充 EXE 的建置來源。

| alpha.9 證據 | 結果 |
| --- | --- |
| 最終來源與公開來源檢查 | 發行來源 `76817ba`；[Source CI](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40850573531) 三個工作成功，公開來源檢查零發現。 |
| 候選尋找與 Windows UI | Windows 執行 40 項候選尋找、216 項 UI 檢查；包含候選確認、取消、手動選擇，以及 96／192 DPI 的控制項位置檢查。既有原生資料庫、加密與附件回歸亦通過。 |
| 原生建置與來源證明 | [發布流程](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40850573625) 四個工作成功；[來源證明](https://github.com/yung13yubabie/chatatlas-offline/attestations/61229084) 已逐檔核對五個產物皆來自同一發行來源。 |
| 實際封裝 EXE | [exact-release-smoke](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40850573625/job/123378231986) 成功，驗證實際套件及 EXE 的 Windows 啟動、關閉與可攜行為。 |
| Release | [v0.3.1-alpha.9](https://github.com/yung13yubabie/chatatlas-offline/releases/tag/v0.3.1-alpha.9) 於 2026-10-29 09:56:25 UTC 公開發布為預覽版，僅包含下列五檔。 |

## 公開產物讀回核對

五個檔案已從公開 Release 網址另外下載，大小與 SHA-256 均與該輪建置 CI 的原始輸出一致，包含 `SHA256SUMS.txt` 本身。來源證明亦逐檔驗證發行來源為 `76817ba`。

公開 Windows ZIP 通過消費者套件契約；單一 EXE 與 ZIP 內執行檔位元組相同、保有完整內嵌授權，沒有執行環境套件。來源 ZIP 恰含發行提交的 215 個 Git 追蹤檔與 `SOURCE_SHA256SUMS.txt`，共 216 項；逐檔位元組及校驗清單核對通過，清單完整、無重複，也沒有私密執行資料、編譯產物或舊 Python 實作。

另核對 alpha.8 的 Release 與五個資產，其識別、大小、雜湊與發布時間保持原值，沒有覆寫。

## 發布檔案

- `ChatAtlas-0.3.1-alpha.9-Windows-x64.exe`
- `ChatAtlas-0.3.1-alpha.9-Windows-x64-candidate.zip`
- `ChatAtlas-0.3.1-alpha.9-source.zip`
- `ChatAtlas-0.3.1-alpha.9-CHECKLIST.txt`
- `SHA256SUMS.txt`

本版只採用通過本次驗證的原始 CI 產物，沒有沿用 alpha.8 測試包或本機重包檔案。驗證使用人工目錄、資料庫、程序候選與附件，不代表所有 Windows LINE 帳號或僅在 LINE 可見的媒體皆已完成實機驗收；原有相容性限制仍保留。
