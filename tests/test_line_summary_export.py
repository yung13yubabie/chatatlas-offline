"""Synthetic export contract tests; never import or read live LINE."""

from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools import export_line_summary as exporter


SINCE = "2026-10-01T00:00:00+08:00"
UNTIL = "2026-10-02T00:00:00+08:00"


def message(mid="m1", timestamp="2026-10-01T12:00:00.123456+08:00", **extra):
    return {"message_id": mid, "sent_at": timestamp, "type": "text",
            "sender": "人工測試作者", "content": "人工測試訊息", **extra}


def page(items, more=False, cursor=None, **extra):
    return {"items": items, "has_more": more, "next_cursor": cursor,
            "content_complete": True, **extra}


class CollectionTests(unittest.TestCase):
    def collect(self, client, **changes):
        values = dict(account_id="fixture-account", chat_id="fixture-chat",
                      chat_title="人工測試聊天室", chat_kind="community",
                      since=SINCE, until=UNTIL, limit=5000, history_client=client)
        values.update(changes)
        return exporter.collect_snapshot(**values)

    def test_fixed_scope_pages_preserve_unicode_fractional_time_and_thread_fields(self):
        original = [message(), message("m2", thread_id="thread", thread_root_message_id="root")]
        calls = []
        def client(**query):
            calls.append(query)
            return (page(original[:1], True, "opaque-not-to-be-decoded") if len(calls) == 1
                    else page(original[1:]))
        snapshot = self.collect(client)
        self.assertEqual(snapshot, {
            "format": "chatatlas.line-summary", "schema": 1, "account_id": "fixture-account",
            "chat": {"source_id": "fixture-chat", "title": "人工測試聊天室", "kind": "community"},
            "since": SINCE, "until": UNTIL, "local_range_complete": True, "messages": original})
        self.assertIs(snapshot["messages"][0], original[0])
        self.assertEqual(calls, [
            {"chat_id": "fixture-chat", "since": SINCE, "until": UNTIL, "limit": 500, "cursor": None},
            {"chat_id": "fixture-chat", "since": SINCE, "until": UNTIL, "limit": 500,
             "cursor": "opaque-not-to-be-decoded"}])

    def test_link_with_null_sender_and_no_content_is_preserved(self):
        item = message(type="link", sender=None, url="https://example.invalid/test")
        del item["content"]
        self.assertEqual(self.collect(lambda **_: page([item]))["messages"], [item])

    def test_completed_empty_local_range_is_allowed(self):
        self.assertEqual(self.collect(lambda **_: page([]))["messages"], [])

    def test_invalid_caller_fields_do_not_read_line(self):
        cases = [{"since": "2026-10-01T00:00:00"}, {"until": SINCE},
                 {"since": "2026-10-01T00:00:00.000001+08:00"},
                 {"since": "2026-10-01T00:00:00.0000001+08:00"},
                 {"limit": 0}, {"limit": True}, {"account_id": ""},
                 {"chat_id": ""}, {"chat_title": " "}, {"chat_kind": "other"},
                 {"account_id": "account\n"}, {"chat_id": "chat\x7f"},
                 {"chat_title": "中" * 1366}, {"chat_title": "\ud800"},
                 {"account_id": "\ud800"}]
        for changes in cases:
            with self.subTest(changes=changes):
                calls = []
                with self.assertRaises(exporter.ExportError):
                    self.collect(lambda **query: calls.append(query), **changes)
                self.assertEqual(calls, [])

    def test_date_grammar_and_years_match_the_native_importer(self):
        invalid = ["2026-10-01 00:00:00+08:00", "20261001T000000+0800",
                   "2026-10-01T00:00:00,123+08:00", "2026-10-01T00:00:00+08:00:01",
                   "2026-10-01T00:00:00+14:01", "2026-10-01T00:00:00-15:00",
                   "2026-10-01T00:00:00+08:60", "2026-10-01T00:00:00.1234567890Z",
                   "2026-10-01T00:00:00z", "2026-02-30T00:00:00Z",
                   "2026-10-01T24:00:00Z", "2026-10-01T00:00:60Z",
                   "1969-12-31T23:59:59Z", "3000-01-01T00:00:00Z",
                   "1970-01-01T00:00:00+00:01", "2999-12-31T23:59:59-00:01"]
        for value in invalid:
            with self.subTest(value=value):
                with self.assertRaises(exporter.ExportError):
                    exporter._date(value)
        for value in ["1970-01-01T00:00:00Z", "2999-12-31T23:59:59Z",
                      "2026-10-01T00:00:00.123456789+14:00",
                      "2026-10-01T00:00:00-14:00"]:
            with self.subTest(value=value):
                self.assertIsNotNone(exporter._date(value))

    def test_native_utf8_title_byte_bound(self):
        self.collect(lambda **_: page([]), chat_title="中" * 1365 + "a")
        with self.assertRaises(exporter.ExportError):
            self.collect(lambda **_: page([]), chat_title="中" * 1365 + "ab")

    def test_upstream_character_limit_and_native_identifier_control_rules(self):
        self.collect(lambda **_: page([]), account_id="😀" * 256)
        with self.assertRaises(exporter.ExportError):
            self.collect(lambda **_: page([]), chat_id="a" * 257)
        for control in ["\x00", "\x01", "\x1f", "\x7f"]:
            with self.subTest(control=control), self.assertRaises(exporter.ExportError):
                self.collect(lambda **_: page([]), account_id="account" + control)

    def test_bad_pages_fail_without_retry(self):
        cases = [None, {"items": [], "has_more": False},
                 page([message()], content_complete=False),
                 page([message()], blocked_reason="budget exhausted"),
                 page([], True, "cursor"), page([message()], True),
                 page([message()], False, "unexpected"),
                 page([message()], has_more=1),
                 page([message(), message()]), page([{"message_id": "m"}]),
                 page([message("later"), message("earlier")]),
                 page([message(timestamp=UNTIL)]), page([message(timestamp=SINCE[:-6])]),
                 page([message(thread_id="thread")])]
        for result in cases:
            with self.subTest(result=result):
                calls = []
                def client(**query):
                    calls.append(query)
                    return result
                with self.assertRaises(exporter.ExportError):
                    self.collect(client)
                self.assertEqual(len(calls), 1)

    def test_duplicate_ids_across_pages_are_rejected(self):
        pages = iter([page([message()], True, "first"), page([message()])])
        with self.assertRaises(exporter.ExportError):
            self.collect(lambda **_: next(pages))

    def test_repeated_cursor_is_rejected(self):
        pages = iter([page([message("m1")], True, "same"),
                      page([message("m2")], True, "same")])
        with self.assertRaises(exporter.ExportError):
            self.collect(lambda **_: next(pages))

    def test_limit_is_not_a_successful_partial_snapshot(self):
        calls = []
        def client(**query):
            calls.append(query)
            return page([message()], True, "more")
        with self.assertRaises(exporter.ExportError):
            self.collect(client, limit=1)
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0]["limit"], 1)

    def test_exact_limit_with_complete_final_page_succeeds(self):
        self.assertEqual(len(self.collect(lambda **_: page([message()]), limit=1)["messages"]), 1)

    def test_page_cannot_exceed_requested_count(self):
        with self.assertRaises(exporter.ExportError):
            self.collect(lambda **_: page([message("m1"), message("m2")]), limit=1)

    def test_page_byte_bound_uses_upstream_ascii_json_accounting(self):
        item = message(content="中" * 50000)
        with self.assertRaises(exporter.ExportError):
            self.collect(lambda **_: page([item]))

    def test_snapshot_budget_applies_across_multiple_valid_pages(self):
        pages = iter([page([message("m1", content="a" * 300)], True, "c1"),
                      page([message("m2", content="b" * 300)])])
        with patch.object(exporter, "MAX_SNAPSHOT_BYTES", 900):
            with self.assertRaises(exporter.ExportError):
                self.collect(lambda **_: next(pages))

    def test_backend_diagnostic_is_not_returned(self):
        def client(**query):
            raise RuntimeError("secret-key-and-chat-content")
        with self.assertRaises(exporter.ExportError) as caught:
            self.collect(client)
        self.assertNotIn("secret-key", str(caught.exception))


class OutputTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.snapshot = exporter.collect_snapshot("fixture-account", "fixture-chat", "人工測試", "group",
                                                   SINCE, UNTIL, history_client=lambda **_: page([message()]))

    def test_atomic_new_output_is_valid_json_and_private(self):
        target = self.root / "new.json"
        size = exporter.write_snapshot(target, self.snapshot)
        self.assertEqual(json.loads(target.read_text(encoding="utf-8")), self.snapshot)
        self.assertEqual(target.stat().st_size, size)
        if os.name != "nt":
            self.assertEqual(stat.S_IMODE(target.stat().st_mode), 0o600)
        self.assertEqual(list(self.root.iterdir()), [target])

    def test_existing_output_is_never_overwritten(self):
        target = self.root / "existing.json"
        target.write_bytes(b"keep")
        with self.assertRaises(exporter.ExportError):
            exporter.write_snapshot(target, self.snapshot)
        self.assertEqual(target.read_bytes(), b"keep")

    def test_publish_race_does_not_replace_another_file(self):
        target = self.root / "raced.json"
        original = exporter.output_destination
        calls = []
        def check(value):
            calls.append(value)
            result = original(value)
            if len(calls) == 2:
                target.write_bytes(b"other-writer")
            return result
        with patch.object(exporter, "output_destination", side_effect=check):
            with self.assertRaises(OSError):
                exporter.write_snapshot(target, self.snapshot)
        self.assertEqual(target.read_bytes(), b"other-writer")
        self.assertEqual(list(self.root.iterdir()), [target])

    def test_failed_write_leaves_no_partial_file(self):
        target = self.root / "failed.json"
        with patch.object(exporter.os, "fsync", side_effect=OSError("synthetic")):
            with self.assertRaises(OSError):
                exporter.write_snapshot(target, self.snapshot)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_successful_publish_remains_successful_when_temp_cleanup_fails(self):
        target = self.root / "saved.json"
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(exporter, "load_history_client", return_value=lambda **_: page([message()])), \
                patch.object(Path, "unlink", side_effect=OSError("private-path-and-backend-detail")), \
                redirect_stdout(stdout), redirect_stderr(stderr):
            status = exporter.main(self.arguments(target))
        self.assertEqual(status, 0)
        self.assertEqual(json.loads(target.read_text(encoding="utf-8"))["messages"], [message()])
        self.assertIn("local_range_complete=true", stdout.getvalue())
        self.assertEqual(stderr.getvalue(), "Snapshot published; its private temporary copy could not be removed.\n")
        self.assertNotIn("private-path-and-backend-detail", stderr.getvalue())

    def test_cleanup_failure_does_not_mask_the_original_write_failure(self):
        target = self.root / "failed.json"
        original = OSError("original-private-backend-error")
        stderr = io.StringIO()
        with patch.object(exporter.os, "fsync", side_effect=original), \
                patch.object(Path, "unlink", side_effect=OSError("different-private-cleanup-error")), \
                redirect_stderr(stderr):
            with self.assertRaises(OSError) as caught:
                exporter.write_snapshot(target, self.snapshot)
        self.assertIs(caught.exception, original)
        self.assertFalse(target.exists())
        self.assertEqual(stderr.getvalue(), "Temporary cleanup failed; a private partial file may remain.\n")
        self.assertNotIn("private-backend-error", stderr.getvalue())
        self.assertNotIn("private-cleanup-error", stderr.getvalue())

    def test_git_repository_and_worktree_destinations_are_rejected(self):
        for is_file in (False, True):
            with self.subTest(worktree=is_file):
                git = self.root / ".git"
                if is_file:
                    git.write_text("gitdir: elsewhere")
                else:
                    git.mkdir()
                with self.assertRaises(exporter.ExportError):
                    exporter.write_snapshot(self.root / "private.json", self.snapshot)
                if is_file:
                    git.unlink()
                else:
                    git.rmdir()

    @unittest.skipIf(os.name == "nt", "Symlink creation may require Windows developer privileges")
    def test_linked_destination_and_ancestors_are_rejected(self):
        linked = self.root / "link"
        linked.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(exporter.ExportError):
            exporter.write_snapshot(linked / "snapshot.json", self.snapshot)
        dangling = self.root / "dangling.json"
        dangling.symlink_to(self.root / "absent")
        with self.assertRaises(exporter.ExportError):
            exporter.write_snapshot(dangling, self.snapshot)

    def test_final_snapshot_is_independently_size_checked(self):
        with patch.object(exporter, "MAX_SNAPSHOT_BYTES", 10):
            with self.assertRaises(exporter.ExportError):
                exporter.write_snapshot(self.root / "large.json", self.snapshot)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_missing_output_parent_is_not_created(self):
        with self.assertRaises(exporter.ExportError):
            exporter.write_snapshot(self.root / "missing" / "snapshot.json", self.snapshot)
        self.assertEqual(list(self.root.iterdir()), [])

    def arguments(self, output):
        return ["--line-summary", "unused-trusted-checkout", "--account-id", "fixture-account",
                "--chat-id", "fixture-chat", "--chat-title", "private-chat-title",
                "--chat-kind", "group", "--since", SINCE, "--until", UNTIL,
                "--output", str(output)]

    def test_failed_cli_never_publishes_or_discloses_backend_content(self):
        target = self.root / "aborted.json"
        def client(**query):
            raise RuntimeError("private-chat-title secret-passphrase private-message-text")
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(exporter, "load_history_client", return_value=client), redirect_stdout(stdout), redirect_stderr(stderr):
            status = exporter.main(self.arguments(target))
        self.assertEqual(status, 1)
        self.assertEqual(stdout.getvalue(), "")
        for private in ("private-chat-title", "secret-passphrase", "private-message-text"):
            self.assertNotIn(private, stderr.getvalue())
        self.assertEqual(list(self.root.iterdir()), [])

    def test_successful_cli_prints_counts_only(self):
        target = self.root / "snapshot.json"
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(exporter, "load_history_client", return_value=lambda **_: page([message()])), redirect_stdout(stdout), redirect_stderr(stderr):
            status = exporter.main(self.arguments(target))
        self.assertEqual(status, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertRegex(stdout.getvalue(), r"^messages=1\nlocal_range_complete=true\nbytes=\d+\n$")

    def test_invalid_scope_or_destination_never_loads_upstream(self):
        target = self.root / "snapshot.json"
        target.write_bytes(b"existing")
        with patch.object(exporter, "load_history_client") as load, redirect_stderr(io.StringIO()):
            self.assertEqual(exporter.main(self.arguments(target)), 1)
        load.assert_not_called()

    def test_cli_prevalidates_the_same_strict_scope_before_loading_upstream(self):
        for flag, invalid in [("--since", "2026-10-01 00:00:00+08:00"),
                              ("--since", "1970-01-01T00:00:00+08:00"),
                              ("--account-id", "fixture\x7f"),
                              ("--chat-title", "中" * 1366)]:
            with self.subTest(flag=flag, invalid=invalid):
                argv = self.arguments(self.root / "snapshot.json")
                argv[argv.index(flag) + 1] = invalid
                with patch.object(exporter, "load_history_client") as load, redirect_stderr(io.StringIO()):
                    self.assertEqual(exporter.main(argv), 1)
                load.assert_not_called()
        self.assertEqual(list(self.root.iterdir()), [])

    def test_argument_errors_do_not_echo_private_values(self):
        stderr = io.StringIO()
        with redirect_stderr(stderr):
            self.assertEqual(exporter.main(["--unknown-private-chat-value"]), 1)
        self.assertNotIn("unknown-private-chat", stderr.getvalue())


class LoaderTests(unittest.TestCase):
    def test_existing_server_is_reused_without_resetting_its_budget(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            server_file = root / "line_mcp_server.py"
            server_file.write_text("raise AssertionError('do not reimport')")
            client = lambda **_: page([])
            existing = SimpleNamespace(__file__=str(server_file), line_get_history=client, used_messages=99)
            with patch.dict(sys.modules, {"line_mcp_server": existing}):
                before = list(sys.path)
                self.assertIs(exporter.load_history_client(root), client)
                self.assertIs(exporter.load_history_client(root), client)
                self.assertEqual(existing.used_messages, 99)
                self.assertEqual(sys.path, before)

    def test_different_loaded_checkout_is_not_replaced(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "line_mcp_server.py").write_text("raise AssertionError('do not import')")
            existing = SimpleNamespace(__file__=str(root / "other" / "line_mcp_server.py"))
            with patch.dict(sys.modules, {"line_mcp_server": existing}):
                with self.assertRaises(exporter.ExportError):
                    exporter.load_history_client(root)
                self.assertIs(sys.modules["line_mcp_server"], existing)


if __name__ == "__main__":
    unittest.main()
