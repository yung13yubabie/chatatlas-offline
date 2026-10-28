# 聊藏原生開發

版本以根目錄 VERSION 與對應 Release 為準。原生 Windows EXE 不啟動 Python 或 Node.js；原生建置與打包也不需 Python。

## Windows x64 建置

使用 Zig 0.14.1 與 Node.js，從 Repo 根目錄執行：

```sh
node tools/fetch_zig.mjs
# Linux：ATLAS_ZIG 指向 .toolchains/zig-x86_64-linux-0.14.1/zig
# Windows：ATLAS_ZIG 指向 .toolchains/zig-x86_64-windows-0.14.1/zig.exe
node native/build.mjs
```

下載腳本只接受官方固定 SHA-256 的 Linux / Windows x64 工具鏈；不是 app 執行時下載。
若 Zig 不在 PATH，設定 `ATLAS_ZIG`。建置拒絕其他 Zig 版本，輸出 `native/out/ChatAtlas.exe`
及包含全部編譯來源雜湊的 `build-provenance.json`。Authenticode 仍未簽章。

## 原生測試與封裝

Linux 需要 GNU C++20、make、系統 SQLite：

```sh
make -C native test build/atlas-cli
node tools/graph_model_test.cjs
node native/tests/windows_bridge_contract.cjs
node tools/package_test.mjs
node tools/check_public_tree.mjs
node tools/package_release.mjs --output dist
```

Windows 的合成 ACL / 核心測試：`node tools/windows_native_tests.mjs`；
PowerShell 合成測試：`powershell -File native/tests/windows_bridge_test.ps1`。
這些不開啟 LINE、不讀真實聊天，也不能取代 GUI 實機驗收。

打包只接受經檢查的 Git index 來源與相符的 EXE 來源雜湊，人工範例由原生 CLI 生成；
輸出資料夾需為新目標，不覆寫既有產物。ZIP 每一項會在寫出前解壓比對。
暫存收藏不能放在 Git 樹內。若環境暫存根目錄被標成 Git 資料夾，換正常非 Git
私人測試環境；不可刪除或停用保護。

## 發布及限制

`windows-build.yml` 可手動或由 alpha tag 觸發；先通過 Windows 合成測試，再建置、打包及
簽發 GitHub build provenance attestation。僅該建置 job 取得短效 OIDC 與 attestation 寫入權限，
沒有 Repo contents 寫入權限，不會自行建立 Release。

必須下載 CI 產物的原始位元組發布，不能在本機重新打包後宣稱繼承 CI attestation。
下載後可用 `gh attestation verify <檔案> --repo yung13yubabie/chatatlas-offline` 驗證。
attestation 是建置來源證明，不是 Authenticode 簽章或安全認證。

`-fstack-protector-strong` 已加入原生編譯。CFG 尚未啟用：Zig 0.14.1 接受 `-mguard=cf`
但測試產物未設 GUARD_CF，而 `--guard-cf` 與 `/guard:cf` linker 參數皆被拒絕。
不會僅因編譯旗標存在就聲稱有 CFG。

nlohmann/json 3.12.0 標頭授權在 `vendor/`，執行環境告示見根目錄 `licenses/`。
舊 Python 實作及測試僅保留歷史參考；全部 CI、原生建置及發布流程均不執行它們。
