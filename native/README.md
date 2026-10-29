# 聊藏原生開發

版本以根目錄 VERSION 與對應 Release 為準。原生 Windows EXE 不啟動 Python 或 Node.js；原生建置與打包也不需 Python。

## Windows x64 建置

使用 Zig 0.14.1 與 Node.js，從 Repo 根目錄執行：

```sh
node tools/fetch_zig.mjs
node tools/fetch_line_cipher.mjs
# Linux：ATLAS_ZIG 指向 .toolchains/zig-x86_64-linux-0.14.1/zig
# Windows：ATLAS_ZIG 指向 .toolchains/zig-x86_64-windows-0.14.1/zig.exe
node native/build.mjs
```

下載腳本只接受官方固定 SHA-256 的 Linux／Windows x64 工具鏈與 SQLite3MultipleCiphers 原始碼；這些只在開發者建置時下載，程式執行時不下載或安裝套件。
若 Zig 不在 PATH，設定 `ATLAS_ZIG`。建置拒絕其他 Zig 版本，輸出 `native/out/ChatAtlas.exe`
及包含全部編譯來源雜湊的 `build-provenance.json`。Authenticode 仍未簽章。

## 原生測試與封裝

Linux 需要 GNU C／C++20、make、系統 SQLite，並先執行上方的解密來源下載腳本：

```sh
make -C native test build/atlas-cli
node tools/graph_model_test.cjs
node native/tests/windows_bridge_contract.cjs
node native/tests/app_flow_contract.cjs
node native/tests/ui_dpi_contract.cjs
node tools/package_test.mjs
node tools/check_public_tree.mjs
node tools/package_release.mjs --output dist
```

Windows 的合成 ACL / 核心測試：`node tools/windows_native_tests.mjs`；
PowerShell 合成測試：`powershell -File native/tests/windows_bridge_test.ps1`。
Windows 原生測試亦包含實際控制項的同程序合成 DPI 訊息、字型／尺寸／捲動驗證，不修改 OS 縮放設定。這些不開啟 LINE、不讀真實聊天，也不能取代真實多螢幕與完整 GUI 實機驗收。

## alpha.11 介面與保存驗證

EXE 將聊天室、保存結果、連線、抓取紀錄及設定分頁，聊天室可依名稱／類型／來源篩選並保留跨頁選取。自動抓取不把手動清單全部勾上。原生視窗測試涵蓋控制項、選取與分頁；Source CI 的人工範例截圖用於實際版面核對，不包含使用者聊天照片。

核心測試以人工資料驗證名稱只在同一來源及身分中沿用、原始訊息正文不因顯示清理而截斷、檔名優先用原名及逐檔進度／結果。停止後建立可再次取消的輕量閱讀頁，不重做大檔下載或完整雜湊；完整檢查仍為獨立操作。輕量頁明示開啟本機原檔，正常完整匯出保留分塊驗證另存。

瀏覽器測試須用真實 file:// 開啟人工資料，驗證分頁、篩選、完整閱讀、預覽、本機原檔及正常分塊下載；禁止 CDN 與遠端字型，不能以開發伺服器測試取代離線驗收。設計依據見 [UI_DESIGN.md](../docs/UI_DESIGN.md)。實際通過項目以本輪 CI 為準。

## LINE 本機來源

本機 LINE 資料庫使用獨立的 `LineCipher`，以唯讀連線開啟選定的 `.edb`；訊息紀錄自己的 `Db` 仍使用系統 SQLite。SQLite3MultipleCiphers 以 C 編譯成靜態物件，連入 EXE。LINE 來源明確使用與 wxSQLite3 相容的 AES-128-CBC，不嘗試其他資料庫密碼方案；內部安全記憶體配置仍使用上游所需的 ChaCha20 隨機數支援。Windows 程序讀取與金鑰驗證由原生程式執行，不呼叫 Python、PowerShell 或外部轉檔程式。

固定來源為 SQLite3MultipleCiphers 2.6.1／SQLite 3.54.0，下載內容與 SHA-256 由 `tools/fetch_line_cipher.mjs` 核對，開發快取放在被忽略的 `.toolchains/`。這是編譯輸入，不是使用者必須另裝的執行套件。相關授權見 [第三方清單](../THIRD_PARTY_NOTICES.md)。

`LineSession` 只接受在同一個已簽章、目前使用者的 LINE 程序中實際觀察到的具名登入資訊。該程序的資料庫金鑰必須能開啟使用者選定的 `.edb`；session 保持程序 handle，`current()` 在原程序退出後拒絕沿用。一般登入 token 與 `X-Line-Application` 來自具名 HTTP／JSON 欄位；已觀察到的 OBS token 另須有 LINE OBS 來源上下文。這些資料只留在記憶體，不進入設定、訊息紀錄、HTML 或日誌。

原生 OBS 授權只實作 `acquireEncryptedAccessToken(featureType=2)` 的 compact Thrift RPC。既有 access token 使用 LINE 的 `/S4` 途徑；JWT 類型使用固定 LINE 來源的 LEGY 包裝，經 Windows CNG 執行 AES-128-CBC 與 RSA-OAEP，不呼叫 JavaScript、Python 或外部工具。具名憑證、`X-Line-Application` 與 `X-Talk-Meta` 沿用實際工作階段和所選訊息，不建立假 token、跨帳號改用其他憑證或停用 TLS。原生協定參考及授權見 [第三方清單](../THIRD_PARTY_NOTICES.md)。

附件使用資料庫訊息 ID 與已知 OBS 格式 `GET /r/{talk|g2}/m/{messageId}`；E2EENext 使用實際 SID／OID。訊息內明確提供的下載／預覽網址亦受固定 LINE OBS HTTPS 來源限制。若實際訊息含可辨識的媒體 `keyMaterial`，以 HKDF-SHA-256 派生金鑰、先核對 HMAC 完整性，再由 Windows CNG 執行 AES-256-CTR 解密；影片的分塊驗證按已知格式處理。缺少具名工作階段、應用程式資訊或媒體金鑰、授權遭拒或附件過期時仍會列為未取得；不能以人工協定測試宣稱所有真實 LINE 版本和圖片均已可用。

資料庫、程序候選擷取、檔案解析與訊息保存流程的測試均使用人工資料。通過加密 fixture 或 Windows 合成程序測試，不表示已驗證每一版真實 LINE 的資料表、記憶體配置或圖片快取。使用步驟與目前界線見 [LINE 本機資料庫說明](../docs/LINE_DATABASE_IMPORT.md)。

打包只接受經檢查的 Git index 來源與相符的 EXE 來源雜湊，人工範例由原生 CLI 生成；
輸出資料夾需為新目標，不覆寫既有產物。ZIP 每一項會在寫出前解壓比對。
暫存訊息紀錄不能放在 Git 樹內。若環境暫存根目錄被標成 Git 資料夾，換正常非 Git
私人測試環境；不可刪除或停用保護。

## 發布及限制

`windows-build.yml` 在主線 VERSION 更新或 alpha tag 時執行；手動執行只產生候選。先通過 Windows 合成測試，再建置、打包、簽發 GitHub build provenance attestation，並對精確封裝 EXE 執行 Windows smoke。建置 job 只取得短效 OIDC 與 attestation 寫入權限；獨立發布 job 在前項成功後建立預覽 Release，核對五個產物的來源證明、上傳位元組與 tag，既有 Release 不覆寫。

必須下載 CI 產物的原始位元組發布，不能在本機重新打包後宣稱繼承 CI attestation。
下載後可用 `gh attestation verify <檔案> --repo yung13yubabie/chatatlas-offline` 驗證。
attestation 是建置來源證明，不是 Authenticode 簽章或安全認證。

`-fstack-protector-strong` 已加入原生編譯。CFG 尚未啟用：Zig 0.14.1 接受 `-mguard=cf`
但測試產物未設 GUARD_CF，而 `--guard-cf` 與 `/guard:cf` linker 參數皆被拒絕。
不會僅因編譯旗標存在就聲稱有 CFG。

nlohmann/json 3.12.0 標頭授權在 `vendor/`，執行環境告示見根目錄 `licenses/`。
`src/line_vault` 的舊 Python 實作、測試與啟動／建置工具已移除，只保留原生版需要的網頁資源與 Windows UIA 工作者；CI、原生建置及發布流程均不執行 Python。原生本機資料庫、圖片與附件來源沒有 Python 執行需求。

附件儲存路徑契約測試：`build/blob-path-contract-test` 不開啟訊息紀錄；
`make -C native test-blob-path-store` 在全新私人暫存目錄驗證登記、沿用、快取、HTML 與 ZIP 拒絕邊界。兩者已接入 Linux 與 Windows 原生測試；全數使用人工資料，不能視為防毒或真實 LINE 採集驗收。
