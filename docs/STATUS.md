# 0.3.1-alpha.7 候選版狀態

目前正在核對與修正本機私人儲存邊界、GUI 捲動重繪、LINE 視窗診斷、資料目錄入口及 UTC+8 歷史顯示。精確提交 CI 與新版實際圖片尚待完成，不沿用前版成功紀錄。

本輪範圍與未測界線見 [ALPHA7_REVIEW.md](ALPHA7_REVIEW.md)。不修改 alpha.1～6 已發布標籤或資產。

## alpha.6 歷史狀態（不是本版驗收）


更新：2026-10-28。alpha.6 程式與文件正在整合，最終發布產物仍須重新驗收；是否已發布與最後驗收狀態，以對應 Release 頁為準。這是實驗預覽版，不是已完成的 LINE 備份工具。

## 本輪範圍

[附件路徑契約、ZIP 資產一致性與修補回歸](ALPHA6_REVIEW.md)採先查證、再修改的流程。繼續維持 EXE 旁 ChatAtlasData、無 AppData fallback、不改既有資料夾 ACL、不安裝套件、HTML 無外部 CDN。

本版的全部 CI、原生建置與封裝不執行 Python。舊 Python 來源只保留歷史參考，不是使用者啟動入口。

## 驗收規則

- 對最終 commit 執行 source checks、Windows 原生／合成測試及實際發布 EXE 測試；不可沿用前版 CI。
- 發布四個原始 CI 產物，核對 SHA-256 與 GitHub attestation，不本機重包後宣稱仍有原來源證明。
- 新版畫面需來自實際執行，不替舊圖改標版本。
- 通過、失敗、跳過與未測分別列出；windows-latest／Chromium CI 不等同全系統、全瀏覽器或真實 LINE 整合。

## 本機環境限制

本機暫存祖先有 Git 標記，收藏保護會拒絕在其中建立 Store；不移除或繞過保護。完整 Store／端到端封裝由正常隔離的 CI 暫存區驗證。本機可進行不依賴 Store 建立的來源契約、路徑、資料庫及交叉編譯。

## 本版程式驗收

程式來源 `81c9dbc41d085296a93ca2c3c78ef0768a447052` 的 [Source CI](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40713161892) 與 [Native CI](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40713448403) 各三個工作均成功。

| 驗收 | Linux | Windows |
| --- | --- | --- |
| 原生核心 | 132 | 127 |
| 純附件路徑契約 | 35 | 35 |
| 附件路徑整合 | 121 | 120 |
| ZIP 邊界 | 57 | 57 |
| Store 取消／發布 | 27 | 27 |
| 完整備份匯入 | 107 | 155 |
| 歷史／文字模式／格式升級 | 62 | 62 |
| 同程序 GUI／DPI／流程 | 不適用 | 91 |

資料庫型別、訊息值域、路徑、Windows ACL／別名、UIA 合成控制及離線 file:// 閱讀回歸亦通過。瀏覽器正常測試流程無外部請求，注入的外部資源被 CSP 拒絕；不能由此宣稱所有可能環境皆不連網。

Native 的 `exact-release-smoke` 工作 `122911277624` 使用實際封裝 EXE，驗證隔離開發環境下的人工 demo/check、GUI 啟動／關閉、可攜移動、唯讀位置拒絕，以及 `ATLAS_QA` 啟用的人工收藏採集紀錄動線；五張新版畫面均保留原始截圖。不是乾淨作業系統認證、實體多螢幕或真實 LINE 測試。

上述是程式凍結點的實測證據。文件／截圖整合後仍重跑最終來源的全部流程，Release 只使用該最終流程的四個原始資產。

## alpha.5 歷史驗收

[alpha.5 Release](https://github.com/yung13yubabie/chatatlas-offline/releases/tag/v0.3.1-alpha.5) 綁定來源 `5b9380c71137c0bc47eb4b39fe1db2af1e4c3189`；其資產、CI 與截圖僅屬該版本，不沿用作 alpha.6 驗收。

## alpha.4 歷史驗收

程式 commit `6a15c4b02282573a699871a79cb3c2ebe29d7cf5` 的 [Source CI](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40659395672)與 [Native CI](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40659516136)共六個工作全數成功，包括 58 項同程序 Win32 DPI／資料夾流程測試、Windows 127 項核心測試、32 項 ZIP 邊界、資料庫與實際路徑別名、PowerShell 延遲回焦，以及無額外語言執行環境的原生閱讀器瀏覽器回歸。

四張 alpha.4 實際截圖來自上述工作。加入文件與截圖後仍要對最終來源重新建置，不能把前述候選資產直接稱為最終來源包。

## 前版紀錄

alpha.3 的已發布 tag、來源及資產保持不變：

- [alpha.3 Release](https://github.com/yung13yubabie/chatatlas-offline/releases/tag/v0.3.1-alpha.3)
- [alpha.3 tag Source CI](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40652910732)
- [alpha.3 tag Native CI](https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40652910690)

以上成功紀錄只屬 alpha.3，不是本版驗收。

## 保留限制

真實 LINE 採集、可用版面設定、個人 Windows 環境、不同縮放／儲存媒體及多 GB 真實工作負載仍需另行實測。Authenticode 未簽章、CFG 未啟用。不宣稱沒有漏洞、無暗門或任何情況都不會外連。請保留原始資料與第二份副本，不要使用預覽版處理唯一一份重要聊天。
