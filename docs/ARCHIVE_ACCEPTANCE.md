# Native archive lifecycle acceptance

This is a developer verification record, not text for the reading interface. Fixtures are artificial; no personal LINE data is required. The lifecycle review covers the native implementation, not the retained historical Python sources.

## Confirmed correction

`Store::check()` returns `pending_attachment` for pending, failed, or unavailable originals and `preview_only` for thumbnails. `makeZip()` previously omitted these codes from the operation labelled a complete backup, so it could produce a ZIP without complete originals. The complete-backup gate now rejects both codes before publishing an output. The existing collection remains readable and unchanged; the message asks the user to save or repair missing files before retrying. No partial-backup feature was added.

## Verification coverage

| Flow | Evidence and acceptance |
| --- | --- |
| Initial collection | Existing native core tests collect all artificial rooms and validate database/file integrity. |
| Repeat update | Existing tests assert updated message text, no duplicate items and zero downloads for intact attachments. |
| Content deduplication | Existing tests share matching content across attachments without redundant downloads. |
| Missing/corrupt attachment | Existing corruption test plus new lifecycle test assert detection, repair and download of only the missing content. |
| Rejected source batch | New duplicate-source-ID batch test requires rejection without changing the prior item count and records a failed job. |
| Interrupted job | Existing reopen test changes a running job to interrupted; copied-but-unregistered content is reused. |
| Retry/unavailable/wrong bytes | Existing tests distinguish transient retries, expired originals and mismatched content; none is labelled successfully saved. |
| Parent relationships | Existing tests reject missing parents, wrong parent types, cycles and cross-account identity collisions. Artificial fixtures include albums, comments and message threads. |
| Recalled content | Existing tests remove body/title while retaining the relation to already saved files. |
| HTML failure | Existing tests verify early cancellation and invalid identities preserve the previous entrypoint. This does not claim atomic publication of every file under arbitrary disk failure. |
| Complete backup | Existing success and no-overwrite tests plus new tests reject pending, failed, unavailable and preview-only original attachments. |
| Cancelled backup | New test cancels after the partial ZIP is created and requires no published ZIP, no leftover partial and no leftover database snapshot. |
| Windows private storage | Separate `windows_permissions_test.cpp` covers private creation/reopen, unchanged existing parent permissions, broad child permissions and junction rejection. |

## Execution status and limits

The preceding native Linux CI at commit `e4d1cfcf1091830e2e86dcd0e5250113a0a86f83` passed the pre-existing core suite. The new lifecycle regressions must pass on the final commit's Linux and Windows CI before delivery. The local development environment deliberately triggers the source-directory storage guard, so a local full runtime pass is not claimed and that guard is not disabled.

This review does not prove real LINE UI compatibility, unavailable-file recovery from LINE servers, multi-GB performance or atomicity against every same-user path race. File-format support does not substitute for those runtime tests.
