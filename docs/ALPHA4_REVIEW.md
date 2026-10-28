# alpha.4：相容性與邊界修正

基準為 alpha.3 commit `5fcc53675d214efe068f1a4413e04899b21db74d`。先比對發布來源，再修正可確認的行為；不把外部報告的「100%」或「全部實機」當作驗證證據。本文件不是獨立安全認證。

## 報告核對與修正範圍

| 項目 | 原碼核對 | 本輪處理 |
| --- | --- | --- |
| 高 DPI | 固定字型高度、缺 DPI 變更；原本已有隨視窗大小調整控制項 | 依 DPI 建立字型與布局、處理 DPI 變更與可用工作區，空間不足可捲動 |
| 雲端同步位置 | 實際 reparse 路徑與祖先會被拒絕，但不是所有同步桌面都必定如此 | 保持拒絕規則，提示具體保存位置原因；沒有放行一整段 reparse tag |
| 已存在寬 ACL 資料夾 | 工作收藏會拒絕不符合私人 DACL 的目錄 | 新收藏在明確確認後建立專用私人子資料夾；不改使用者原目錄 ACL |
| 包內人工範例被拒 | 套件範例只有 HTML／附件，不含收藏資料庫；不是報告所稱的 ACL 原因 | 文件區分包內網頁與程式建立的 Example 收藏 |
| ZIP 父目錄限制 | ExclusiveFile 沒有強制父目錄私人 ACL；報告的 D 槽／exFAT 必定失敗前提不成立 | 不移除不存在的限制；說明無 ACL 媒體的隱私及 FAT32 單檔限制 |
| 路徑大小寫／別名 | 字面相對路徑不能代替實際目錄身分 | 使用驗證後的目錄身分處理可攜相對位置與 ZIP 包含關係，新增 case／短路徑／subst 測試 |
| 遍歷例外 | GUI 已有中文 catch-all，並非直接印出英文與完整路徑 | 補較明確的檔案系統錯誤分類，不洩漏完整私人路徑、不跳過未驗項目 |
| 大收藏掃描 | 重複驗證成本存在，但外部報告的次數與耗時未經量測 | 增加可取消檢查點；保留每個檔案的連結、控制代碼與權限檢查，不宣稱已有性能提升 |
| UIA 延遲回焦 | 確認框關閉後立即查前景，既有 mock 同步回焦而未覆蓋延遲 | 有界等待並重新驗證身分；不搶焦點、不接受覆寫，逾時請使用者手動取消 |
| SQLite 額外保護 | C++ 連線尚未設定兩項 PRAGMA，報告對保護效果有誇大 | 連線初期設定並查回能力；不支援的舊元件明確提示，支援卻未生效則拒絕 |
| 開發者閱讀回歸腳本 | 舊 reader_test 仍使用歷史 Python 產生器，不是 EXE 或 CI 執行依賴 | 改為原生 CLI 與專用人工 C++ fixture，保留更新、搬移、雜湊失敗、多分塊與空檔下載驗證 |
| OS／瀏覽器全面相容 | CI 的 windows-latest／Chromium 不等於完整平台矩陣 | 文件改用能力與實際測試版本描述，清除舊 Python／CLI 操作誤導 |

## 不能放寬的界線

- 不自動重設既有資料夾 ACL，不把雲端同步位置等同完全離線。
- 來源與目的檔案仍驗證 handle 與 reparse；路徑身分判斷不等於已消除所有祖先 rename 或同帳號競態。
- SQLite `trusted_schema=OFF` 限制 schema 中部分函式／virtual table 使用，不是全面禁止 Trigger／View；`cell_size_check` 是 B-tree 一致性檢查。未知 PRAGMA 可能被 SQLite 忽略，因此需查回而非只看執行成功。
- 收藏與 ZIP 未加密，私人 ACL 不能防止管理員或同帳號所有程式讀取。
- 程式不安裝 Python、Node.js 或其他套件。離線 HTML 不使用外部 CDN；系統元件及瀏覽器自己的網路／暫存行為不在「零痕跡」承諾內。

## 驗證要求

本版 source contract、Linux 原生／路徑／資料庫測試、Windows 合成權限／UIA 延遲測試，以及實際封裝 EXE 的啟動與可攜驗收，需對最終 commit 分別執行。新版截圖必須來自實際 alpha.4 執行，不沿用 alpha.3 圖片改標版本。

資料庫能力提示出現在原生 GUI 的完成／檢查結果及開發 CLI 的 stderr。EXE 的無視窗測試命令不應被當作一般使用者介面；開發者另可查詢 Db 的能力狀態。

合成 DPI 訊息測試不會更改作業系統縮放設定，不等於真實跨螢幕驗收。真實 Windows 10／11 build、4K 多螢幕、OneDrive、exFAT／FAT32 媒體、Firefox／Brave／Safari，以及完整 LINE 採集仍需另列已跑與未跑項目。未測不得標為通過。

## 已取得的程式驗收證據

程式 commit `6a15c4b02282573a699871a79cb3c2ebe29d7cf5` 的 Source 工作 `40659395672` 與 Native 工作 `40659516136` 已全數成功。Windows 實際執行包含 58 項合成 DPI／資料夾流程、127 項核心、32 項 ZIP 邊界、資料庫能力、8.3／subst 與其他磁碟包含關係；PowerShell 合成延遲回焦及實際封裝 EXE 的可攜／唯讀測試也通過。Chromium 閱讀回歸保留多次下載、更新、搬移、損毀分塊拒絕、多分塊／空檔與零外連正常流程。

這些證據僅覆蓋上述程式與測試環境。文件與實際截圖整合後，最終發布仍需重新建置、核對資產與 exact-release-smoke，不替代真實 LINE 或所有使用環境驗證。

## 官方參考

- [Windows 高 DPI](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)
- [檔案系統功能比較](https://learn.microsoft.com/en-us/windows/win32/fileio/filesystem-functionality-comparison)
- [SQLite trusted_schema](https://www.sqlite.org/pragma.html#pragma_trusted_schema)
- [SQLite cell_size_check](https://www.sqlite.org/pragma.html#pragma_cell_size_check)
- [File System Access](https://developer.chrome.com/docs/capabilities/web-apis/file-system-access)
- [Secure Contexts](https://www.w3.org/TR/secure-contexts/)
