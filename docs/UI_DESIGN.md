# 聊藏本機訊息工作台

本輪以「保存完整訊息、看懂每個檔案的結果、停止後仍能閱讀」為主要任務。EXE 使用原生 Windows 控制項，離線閱讀頁使用隨程式攜帶的 HTML、CSS、JavaScript；不載入 CDN、遠端字體、外部 UI 框架或本機伺服器。使用者提供的聊天照片不納入公開來源或測試資料。

## 介面規則

- EXE 用固定導覽、工具列、分頁清單與底部工作狀態；日期、儲存選項和較少使用的管理操作分區，避免全部擠在同一屏。
- 離線頁保留「全部聊天室」總覽；個別對話、關聯圖、圖片影片與檔案使用明確的獨立入口。聊天室清單本身分頁，搜尋與篩選不改變已選取資料。
- 選取控制明確分為本頁、符合篩選的結果、清空全部。自動保存完成的範圍與下一次勾選狀態分開。
- 原始名稱優先；查不到名稱時標示待確認，不捏造人名，不以長識別碼充當名字。完整訊息正文不因介面清理而刪減。
- 目前工作顯示聊天室、實際檔名、可知的數量與步驟。保存結果區分原檔、僅預覽、待處理、未取得及失敗，並提供各項原因。
- 停止先保留已寫入的訊息及檔案，再建立可再次停止的閱讀快照。輕量快照不假稱已重新完成整檔雜湊檢查。
- 檔案另存保留可用的原名；缺名才採訊息時間與檔案種類。TXT 使用聊天室名稱，同名加短序號，內部識別只留給資料關聯與校驗清單。
- 色彩：文字 `#21312e`、主要操作 `#236b58`、內容白底、工作區 `#f5f7f5`。採系統無襯線、正文約 16px、次級資訊約 14px，間距以 8／16／24 為主。
- 搜尋及分頁即時回應；過渡只用短暫顏色或透明度變化，遵守減少動態效果設定，不以動畫掩蓋未知進度。

## 逐一閱讀的參考

以下於 2026-10-29 查看。只採用公開設計方法，不複製受限制元件或引入其執行環境。

| 來源 | 閱讀結果與本輪應用 |
| --- | --- |
| [Beautiful UI](https://www.beautifului.dev/) | 閱讀工作列、篩選表格與搜尋範例；用於逐檔進度、狀態數量與空狀態。 |
| [beUI](https://beui.dev/) | 閱讀元件與動效規範；頻繁操作快速回應，減少位移與彈跳。 |
| [Rare UI](https://www.rareui.com/) | 閱讀文件呈現與導覽範例；只借用名稱、預覽及操作分層的概念。 |
| [Transitions](https://transitions.dev/) | 閱讀轉場及減少動態規則；不使用 transition: all。 |
| [shadcn/ui](https://ui.shadcn.com/) | 閱讀資料表、分頁及選取模式；區分本頁選取與全部結果。 |
| [oil-ui](https://github.com/oil-oil/oil-ui) | 閱讀主 Skill、視覺語言及布局規範；一屏一個主要動作，先排內容層級。 |
| [Oil 展示站](https://ui.oiloil.org/) | 閱讀工作台與舊介面改版案例；使用者任務優先於裝飾。 |
| [UI UX Pro Max](https://github.com/nextlevelbuilder/ui-ux-pro-max-skill) | 閱讀主 Skill、UX 規則與清單；可讀性、鍵盤操作、大資料分頁及狀態保留。未執行其 Python 搜尋器。 |
| [Impeccable](https://github.com/pbakaus/impeccable) | 閱讀 Operate／Read、craft-floor、clarify、harden；採工具與閱讀介面的精確、克制及恢復操作。未安裝其啟動工具。 |
| [Awesome AI Tools for UI](https://github.com/maxbogo/awesome-ai-tools-for-ui) | 完整查看資源分類，作為設計工具索引；不把所有工具加入產品。 |
| [Google Stitch](https://stitch.withgoogle.com/) | 原頁為動態介面，補讀 Google 官方設計與 DESIGN.md 介紹；串起選取、保存、停止、結果流程。 |
| [Uizard](https://uizard.io/) | 閱讀 UX 與互動原型說明；以可操作流程驗收，不能只看靜態畫面。 |
| [Visily](https://www.visily.ai/) | 原頁需 JavaScript，補讀官方 Sections／Screens／Containers 說明；保留總覽、其餘功能獨立分區。 |
| [Figma](https://www.figma.com/) | 閱讀 Design Systems、Auto layout 與元件狀態；EXE 和 HTML 共用層級、間距與狀態語意。 |
| [墨刀](https://modao.cc/) | 閱讀互動觸發與動態元件說明；選取、篩選、停止與結果是持續存在的狀態。 |

驗收使用人工名稱與訊息，檢查長中文、不同視窗尺寸、200% 縮放、大量聊天室分頁、完整正文、逐檔狀態、停止後重開與完全離線下載。畫面證據及實際通過項目由本版發布核對紀錄記載。
