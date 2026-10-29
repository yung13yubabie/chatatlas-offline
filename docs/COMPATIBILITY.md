# LINE 連線相容性

目前沒有隨附「已通過真實 LINE 驗證」的版面設定。這是實際整合的阻擋項，不能用編造的 AutomationId 或單純視窗座標掩蓋。

## 已實作的來源機制

原生 C++ 程式在使用者要求連接 LINE 時，啟動 Windows 系統 PowerShell 並透過管線載入內嵌腳本，使用 Windows 內建 UI Automation。程序只接受固定的 JSON 操作：辨識官方簽章的 LINE 視窗、讀取可存取控制項、核對帳號、選取聊天室、捲動、開啟相簿／記事本／討論串，以及經過名稱限制的下載／另存。

不使用程序注入、記憶體讀取、登入憑證、攔截憑證 或管理員權限。窗口焦點、帳號、來源身份或另存視窗不符時停止。連線程式沒有任意鍵盤輸入、傳送訊息或刪除資料命令。

## 真實 Windows 上需要完成的驗證

1. 在原生程式的「連線檢查」使用「檢查這台電腦」，確認可辨識 LINE、發行者簽章、版本及可讀控制項。這不會自動建立一份可用的連線設定。
2. 在本機確認帳號、聊天室、訊息、相簿、記事本及討論串是否提供穩定識別。UIA RuntimeId 只用作短期操作把手，不能當成跨次更新身份。
3. 依實際控制項製作私人連線設定，核對同名聊天室、日期、父子關聯、完整附件大小或校驗資訊，以及官方另存視窗。
4. 測試至少兩次同一來源採集，確認文字更新、完整附件零重複下載；再測試中斷、缺檔、錯誤焦點及帳號切換。
5. 用去敏的能力紀錄更新此文件。不得把私人控制項快照、完整 profile 或實際聊天提交到公開 Repo。

LINE 自訂控制項若不暴露所需資訊，這條來源方式可能不足；此時應重新研究可授權的本機來源，而不是聲稱已取得全部內容。

## 私人連線設定格式

設定由使用者在「連線檢查」載入，驗證後存於 EXE 旁的 `ChatAtlasData/connection.local.json`；未載入時不會自動產生此設定。不使用 AppData 作為替代位置。它需要 `schema: 1`、`account`、`chats` 及 `panels`。`account` 包含本機識別、實際帳號欄位選擇器與預期值；`chats` 包含清單、列、穩定識別、標題及目前聊天室標題選擇器。

`panels` 分別定義 `message`、`album`、`album_item`、`note`、`comment`、`thread` 的容器、列、欄位、適用聊天室類型及附件儲存控制項。選擇器只接受實際觀察的 `automation_id`、`name`、`control_type`。欄位只能讀名稱、文字或 AutomationId，並用固定前後綴／行號擷取，不執行正規式或程式碼。訊息時間須為有時區的 ISO 8601。

這是給整合測試使用的來源介面，不是宣稱一般使用者已能免設定完成採集的安裝體驗。

官方依據：

- [Microsoft UI Automation providers](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-providersoverview)
- [LINE 聊天歷史差異](https://help.line.me/line/smartphone/sp?contentId=20023289&lang=zh-Hant)
- [LINE 相簿](https://help.line.me/line/smartphone/sp?contentId=20000248&lang=zh-Hant)
- [LINE 記事本](https://help.line.me/line/?contentId=20000246&lang=zh-Hant)
- [LINE 社群](https://help.line.me/line/smartphone/sp?contentId=20005375&lang=zh-Hant)

## 0.2.0 本機檢查與研究界線

`doctor` 及設定頁會顯示實際偵測的 LINE 檔案版本、簽章核對和控制項／操作模式數量。可另存的結果不包含視窗標題、AutomationId 值、聊天文字、帳號或本機路徑。這份結果能協助判斷介面是否可讀，不能替代穩定訊息識別與附件來源驗收。

其他本機讀取器的上游文件也指出 Qt UIA 可能不完整；其 Windows 資料庫讀取採用有範圍的程序金鑰定位，且不提供通用附件下載／伺服器封存。[上游說明](https://github.com/bensonmaxai/line-desktop-mcp/blob/main/docs/windows-extensions.md)。本專案尚未整合該來源，不把外部專案的實測結果當成自身驗收，也沒有悄悄加入程序記憶體掃描。

本版本維持既定安全界線；若 UIA 不提供必要資料，須先重新確認來源方案，再實作及實測。不能只填入看似合理的設定就宣稱所有 LINE 類型已支援。

0.3.0 已移植 Windows 原生執行檔與保存核心，使用 Windows 系統 SQLite 及相同 UIA 連線守則。這不會自動增加 LINE 可暴露的資料；相容性驗收仍未完成。正常開啟 EXE 不需要 Python 或 Node.js。

## 相容性證據的邊界

- Windows 10／11 x64 是目標平台，不代表每個 OS build、更新狀態、企業政策或系統 SQLite 版本均已驗證。CI 的 `windows-latest` 只代表當次 runner；缺少系統元件或必要 API 時停止該功能，不下載替代套件。
- 中／英／日的部分另存視窗標題與按鈕在白名單內，但語系字串覆蓋不等於真實 LINE 完整整合通過。其他本地化標題會保守拒絕。
- 閱讀頁以實際瀏覽器能力為準：附件驗證需要 `crypto.subtle.digest`，大於 64 MiB 的串流另存另需 `showSaveFilePicker`。缺能力時清楚提示，不降級成未驗證下載。不能僅依品牌承諾 Safari 一定失敗或 Chromium 系一定成功。iOS 檔案預覽與桌面 `file://` 頁面需分開驗收。
- 發布頁紀錄已實際執行的測試；未跑過的 OS／瀏覽器／儲存媒體組合標示未驗證。

官方能力說明：[SQLite trusted_schema](https://www.sqlite.org/pragma.html#pragma_trusted_schema)、[File System Access](https://developer.chrome.com/docs/capabilities/web-apis/file-system-access)、[Secure Contexts](https://www.w3.org/TR/secure-contexts/)。

## alpha.7 私人儲存位置

建立帶私人 ACL 的應用資料、收藏、匯入目的地與 ZIP 檔案時，只接受能驗證的本機位置；UNC／延伸 UNC 及映射網路磁碟會在建立前拒絕。這是避免本機 Windows 帳號識別與 SMB／NAS 登入識別不同而留下無法管理的資料，不是已完成所有 NAS 的實機相容測試。既有共用資料不會被自動接管、放寬權限或刪除。外部備份讀取仍保留原有安全檢查。

OneDrive 等同步位置與其他重解析點繼續保守拒絕；請把完整程式移到一般本機目錄。訊息提到 OneDrive 只是常見例子，不表示程式已辨識特定雲端服務。
