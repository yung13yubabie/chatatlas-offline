# 歷史功能驗收與後續增補

本文件原有檢查表記錄早期功能審查，不是 alpha.5 的完整驗收結果。後續版本以 README、對應 Release 及 [alpha.5 修正紀錄](ALPHA5_REVIEW.md) 為準；不得沿用早期通過字樣替代新版本實測。

# Native application functional acceptance

Scope: alpha.2 native Windows controller (`native/src/app.cpp`). This is a bounded correctness and user-flow review, not a security certification or a claim that real LINE capture is complete.

## Corrections made

- Interactive controls participate in keyboard Tab traversal; labels remain non-focusable.
- Reconnecting clears the previous source and selected chats before the attempt. A failed attempt cannot silently continue using the previous connection.
- Loading a valid replacement connection profile invalidates the old connection and its selection. Cancelled or invalid imports leave the prior configuration untouched.
- Empty content selection and reversed date ranges are rejected before saving preferences or constructing an archive.
- Failed thread startup restores the busy state and controls rather than leaving the application stuck.
- Closing during work cancels a paused task and disables Pause/Stop, then waits for the worker. Completion does not open a new modal dialog while closing.
- Failed posting of a chat-list result releases its payload.
- Completion text explicitly counts unavailable or failed attachments rather than presenting partial attachment preservation as complete.
- Headless `--check` requires an existing archive, returns nonzero when issues are found, and writes errors to stderr without blocking on a message box. `--demo` and a successful `--check` retain their JSON stdout contract.

## Acceptance matrix

| Flow | Source review result | Executed evidence / limit |
|---|---|---|
| Normal launch | Native Win32 controls and Windows components; no installer invocation introduced | Windows x64 compilation passed; interactive GUI not run in this cloud workspace |
| Missing connection profile | Reports version/compatibility limitation and offers existing collection or synthetic example | Source reviewed; real LINE not accessed |
| Failed reconnect | Old source/list/selection cleared before new attempt | Portable contract passed; interactive retry pending |
| Profile selection cancelled / invalid | No new profile is written; valid replacement invalidates old source | Source and contract checks; real file-dialog interaction pending |
| Chat filtering and selection | Selection is keyed by source ID and retained across filter redraw; new connection resets it | Source reviewed; native interaction pending |
| Content/date preflight | Empty content and reversed range fail before archive construction; same-day range remains inclusive through next-day exclusive bound | Portable contract passed; Windows date-control interaction pending |
| Busy / repeated click | Atomic busy guard prevents duplicate start; action controls disabled; thread-start failure restores controls | Source and portable contract checks; stress interaction pending |
| Pause / resume / stop / close | Control cancellation remains cooperative; Close clears pause and awaits completion | Source and portable contract checks; live long-running UI interaction pending |
| Collection check / rebuild | Uses existing archive; rebuild does not invoke attachment download | Source reviewed; native core and Windows CI results must be checked for exact commit |
| Open reader | Opens an existing local index in the user's associated browser | Source reviewed; local HTML/browser tests are separately owned |
| ZIP / cancelled file selection | Cancel returns without write; target collision rejected by archive code; existing collection remains separate | Source reviewed; archive regression coverage is separately owned |
| Partial attachment result | Missing/failed attachments are included in completion text | Portable contract passed; real expired LINE attachment not tested |
| Headless `--check` | Missing archive does not create one; corruption yields exit code 1; startup/operation error yields 2, no modal | Portable contract passed; Windows isolated-runtime test separately verifies behavior |
| Real LINE end-to-end | Not validated | Requires an authorized Windows machine with a supported, logged-in LINE version and a verified UI profile; no selectors or compatibility results were invented |
| Multi-GB and long-duration collection | Not validated | Requires dedicated Windows workload and storage-space/cancellation tests |

## Verification record and boundaries

Locally executed for this review:
- `node native/tests/app_flow_contract.cjs`: passed.
- Zig 0.14.1 compilation of `native/src/app.cpp` for x86_64 Windows: passed.
- Scoped whitespace/diff check: passed.

The contract tests inspect source invariants; they are not a replacement for real UI execution. Windows hosted-runner synthetic checks and isolated-runtime smoke tests must be reported using their exact final commit. A normal hosted runner is not a freshly installed Windows machine. Full keyboard behavior, focus order at different DPI settings, small-window layout and native dialog interaction still require real Windows GUI verification.

No Python, runtime package installer, WebView runtime or remote UI dependency was added by this review. Existing privacy, source-directory and archive-integrity guards were not removed to make tests pass.

## alpha.5 本輪新增門檻

新增採集紀錄保留／刪除、同帳號聊天室選項再套用、文字省略／更新、缺檔與損毀補存、完整備份匯入、取消偏好不提交、暫停有效時鐘及停止不自動重建的回歸測試。離線頁必須呈現部分／未知來源範圍與待補原因。真實 LINE 完整採集及來源過期訊號仍未驗證；歷史清單或人工重抓測試不能當成真實來源已整合。
