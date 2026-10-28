# 0.3.1-alpha.2 候選版狀態

更新：2026-10-28。實驗預覽版，不是已完成的 LINE 備份工具。

## 此次可驗證範圍

- 原生 EXE、全部 CI、建置與打包流程不使用 Python；Node.js 僅為開發、測試與建置工具。
- Windows 私人 ACL、檔案控制代碼驗證、LINE 程序與另存對話框驗證的修改需以本版 Windows 合成測試結果為準。
- 原生 build 加入 strong stack protector；CFG 仍未啟用，Authenticode 仍未簽章。
- Actions 原本已釘選完整 commit SHA；本版保留釘選並新增 tag 建置、原生打包與受限權限的 build attestation。
- GitHub attestation 必須由實際成功執行的 workflow 產生並驗證，不能只靠本文件或本機 JSON 宣稱存在。
- 發布是否通過驗收，以 Release 頁所列精確 commit 的 CI 結果為準；必須同時通過來源、Windows 合成測試與實際發布 EXE 測試，不能沿用前版結果。

## 本機可確認與限制

Node 封裝回歸測試已通過（ZIP 往返、CRC、非法路徑拒絕、完整 SHA 釘選與無 Python 原生管線）。
目前工作環境的暫存根目錄屬 Git 樹，原生收藏保護會拒絕合成 Store 建立；
因此本機端到端封裝與完整核心 Store 測試未完成，應由正常隔離的 CI 暫存區驗證，
不會移除保護繞過。

## 前版已完成紀錄

alpha.1 的 source checks 六個 job 及 native build 已通過，舊版文件所記錄的初期環境阻擋
不代表最終 CI 仍失敗。這只適用於 commit `4ef4e7af5d40553fad42d0a7caf29e2affa575a0`：

- https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40619562124
- https://github.com/yung13yubabie/chatatlas-offline/actions/runs/40619848176

## 未完成驗收

CI 包含原生視窗啟動、必要控制項與正常關閉測試；它不等於使用者電腦上的完整互動驗收。
個人 Windows 環境、完整 LINE 自動採集、版面設定與多 GB 附件仍需另外實機驗收。
本次不是深入獨立安全稽核，未宣稱沒有暗門、沒有漏洞或任何情況都不會外連。
檔案雜湊、重編譯比對、PE 匯入表及 CSP 各自只有有限證明範圍。

保留原始資料及第二份副本，不應用預覽版處理唯一一份重要聊天。
