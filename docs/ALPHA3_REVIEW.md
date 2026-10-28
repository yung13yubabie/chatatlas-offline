# alpha.3：第二輪報告核對與修正

本文件記錄有界範圍的程式核對、修正與測試，不是「紅隊認證」，也不宣稱沒有漏洞。
比對基準是已發布的 alpha.2 commit `e3e332a6792cab1b72882ce4ffe5d011d9aaeb72`；
其來源 ZIP SHA-256 為 `4f4cfc481a1b604a4b45d085294df01495bdd84bb2af610964aaecd199a9fbe1`。
不因外部報告的名稱、風險等級或「100%」字樣而跳過實際原始碼比對。

## 報告描述與實際程式

| 項目 | alpha.2 核對結果 | 本輪處理 |
| --- | --- | --- |
| configDir 先預建目錄繞過 DACL | 實際沒有該函式或那段預建程式；原本直接呼叫 privateDir | 不套用不存在的修正；補既有目錄的可繼承權限驗證 |
| callback ACE 被忽略 | 原本已拒絕標準 allow/deny 以外的 ACE | 保留拒絕規則，新增對應測試 |
| stderr pipe 死鎖、stdout 無上限 | 原本 stderr 已導向 NUL；有 16 MiB 上限、35 秒逾時與可取消輪詢 | 保留既有較小上限，增加防退化契約測試 |
| ZIP32 超過 4 GiB 或 65535 項截斷 | 原本已有 ZIP64 的 64 位元計數、extras、EOCD 與 locator | 不倒退成 ZIP32；補 uint64 算術與來源增長的寫入前檢查 |
| Windows 名稱、鎖檔與讀取端邊角 | 確有可補強處 | 裝置名稱／尾隨點空白拒絕，鎖檔權限與 handle 核對，VerifiedInput 串流讀取 |
| 另存覆寫確認窗 | 原本沒有報告所稱的 save_incomplete 迴圈；仍可增加窄範圍清理 | 僅取消已追蹤、同程序、正確 owner、前景與 runtime ID 的新確認窗；不接受覆寫 |
| file:// 瀏覽器能力差異 | 主要是功能能力限制，不是雜湊繞過 | 保留 WebCrypto 與 64 MiB Blob 限制；補明確提示、取消與大小溢出檢查 |
| GUI 指標與 SQLite「已安全」描述 | 指標實際為 shared_ptr；部分宣稱已存在的 SQLite 檢查不在來源中 | 完成連線後才交 UI 執行緒發布；補 SQLite 長度、NULL／OOM 與資源錯誤處理 |

沒有加入未經官方支持與驗證的根憑證指紋。既有 Authenticode Valid、發行組織、PID、啟動時間與執行路徑檢查保留。
沒有為了符合外部報告而加入未要求的 SQLite PRAGMA 行為。

## 可攜設定：使用者選定的做法一

- 設定、連線設定及預設 `Collections`／`Example` 位於 EXE 旁的 `ChatAtlasData`。
- 不改用 LocalAppData 或暫存資料夾，不自動搬移或刪除舊版資料。
- 程式位置不可安全寫入時，明確提示並停止；不靜默換位置。
- 程式資料夾內的收藏位置以帶標記的相對路徑保存，移動整包後重新解析；手動選定的外部位置維持明確的絕對路徑。
- 需可寫入且可驗證 Windows 私人 ACL 的位置。這不等於所有隨身碟格式皆受支援，也不能保證 Windows、瀏覽器或防毒不留下自己的系統紀錄。

## 驗證項目

CI 應對最終 commit 執行下列測試；僅編譯成功不能替代 Windows 執行結果。

- 原有 native core、離線 UI／CSP、真實原生視窗啟閉與實際發布 EXE 測試。
- Windows DACL 繼承、callback 拒絕、鎖檔、讀取 handle、取消釋放、短路徑與可用時的 subst 測試。
- ZIP64 實際 65,536 項、長名稱、4 GiB 與 uint64 算術邊界、來源增長拒絕。
- 小型人工 SQLite 資料：NULL／空字串／內嵌 NUL、交易回滾、準備／綁定錯誤後可恢復、唯讀開啟。
- 下載器 14 項人工 VM 回歸：能力檢查、雜湊失敗、取消與大小上限；瀏覽器結果另行驗證。
- 可攜 GUI：EXE 旁資料、搬移重開、舊 AppData 不變、唯讀位置停止且無 fallback。

本地可執行的測試與交叉編譯已分別進行；Store 的來源目錄防護未被移除或繞過。
SQLite ASan／UBSan 不涵蓋被環境限制而停用的 LeakSanitizer；OOM、缺少動態符號等路徑未作完整故障注入。

## 保留的界線

- 不宣稱消除 SQLite VFS、祖先目錄替換或所有 rename／同帳號程序競態。
- 新建檔建構失敗只用原 handle 嘗試清理，不按不確定路徑刪檔；OS 拒絕清理時仍可能留下空檔。
- 特殊或分拆權限的 DACL 可能被保守拒絕；8.3／subst 無支援時必須記為跳過，不能算執行通過。
- 串流下載核對分塊 SHA-256 與總大小；不要將它描述為另行計算了完整檔案 SHA-256。
- 真實 LINE 採集、完整使用者電腦相容性、多 GB 真實工作負載仍需另外實機驗收。Authenticode 未簽章、CFG 未啟用的限制未改變。
