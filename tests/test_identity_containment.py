"""Synthetic regression tests for tampered local archive identifiers."""
import tempfile
import unittest
from pathlib import Path

from line_vault.errors import IntegrityError
from line_vault.exporter import export_html, make_zip
from line_vault.model import Attachment, Chat, Item
from line_vault.safeio import atomic_bytes, digest
from line_vault.store import Store
from line_vault.transport import prepare_download


class IdentityContainmentCase(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "archive"
        self.store = Store(self.root)
        self.addCleanup(lambda: self.store.close())
        with self.store.transaction():
            self.cid = self.store.put_chat("synthetic-regression", Chat("room", "Artificial test"))
            self.store.put_items(self.cid, [Item("message", "message", "Artificial text")])
            self.aid = self.store.put_attachment(self.cid, Attachment("file", "message", "artificial.bin"))
        payload = self.root / "assets" / "artificial.bin"
        atomic_bytes(payload, b"Artificial regression bytes")
        sha, size = digest(payload)
        with self.store.transaction():
            self.store.save_blob(self.aid, "assets/artificial.bin", sha, size)
        export_html(self.store)


    def assert_rejected_without_output(self):
        before = sorted(str(p.relative_to(self.base)) for p in self.base.rglob("*"))
        entrypoint = (self.root / "index.html").read_bytes()
        self.assertTrue(any(p["code"] == "identity" for p in self.store.check(check_reader=False)))
        with self.assertRaises(IntegrityError):
            export_html(self.store)
        self.assertEqual((self.root / "index.html").read_bytes(), entrypoint)
        self.assertEqual(sorted(str(p.relative_to(self.base)) for p in self.base.rglob("*")), before)
        with self.assertRaises(IntegrityError):
            make_zip(self.store, self.base / "rejected.zip")
        self.assertFalse((self.base / "rejected.zip").exists())
        self.assertFalse((self.base / "rejected.zip.partial").exists())

    def test_tampered_chat_path_identifiers_rejected_before_writes(self):
        # Update foreign-key references too: a database can be relationally valid
        # while its stored primary key is unsafe as an output filename.
        for value in ("../../../outside-synthetic", str(self.base / "absolute-synthetic"),
                      "C:/outside-synthetic", "..\\outside-synthetic", "bad\x00name",
                      "A" * 64, "0" * 64, ""):
            with self.subTest(value=value):
                self.store.db.execute("PRAGMA foreign_keys=OFF")
                self.store.db.execute("UPDATE chats SET id=?", (value,))
                for table in ("items", "attachments", "jobs"):
                    self.store.db.execute("UPDATE " + table + " SET chat_id=?", (value,))
                self.store.db.commit()
                self.store.db.execute("PRAGMA foreign_keys=ON")
                self.assert_rejected_without_output()

    def test_all_stored_id_and_hash_fields_are_checked(self):
        columns = {"items": ("id", "chat_id", "parent_id"),
                   "attachments": ("id", "chat_id", "item_id", "blob_id", "expected_hash"),
                   "blobs": ("id", "sha256")}
        for table, names in columns.items():
            for name in names:
                with self.subTest(table=table, column=name):
                    self.store.db.execute("PRAGMA foreign_keys=OFF")
                    before = self.store.db.execute("SELECT " + name + " FROM " + table).fetchone()[0]
                    self.store.db.execute("UPDATE " + table + " SET " + name + "=?", ("../synthetic",))
                    self.store.db.commit()
                    self.assert_rejected_without_output()
                    self.store.db.execute("UPDATE " + table + " SET " + name + "=?", (before,))
                    self.store.db.commit()
                    self.store.db.execute("PRAGMA foreign_keys=ON")

    def test_nullable_primary_key_rejected(self):
        self.store.db.execute("PRAGMA foreign_keys=OFF")
        self.store.db.execute("UPDATE chats SET id=NULL")
        self.store.db.commit()
        self.assert_rejected_without_output()

    def test_hash_case_and_source_identity_mismatch_rejected(self):
        self.store.db.execute("UPDATE blobs SET sha256=upper(sha256)")
        self.store.db.commit()
        self.assert_rejected_without_output()

    def test_download_entry_rejects_malformed_hash_before_writes(self):
        blob = dict(self.store.db.execute("SELECT * FROM blobs").fetchone())
        before = sorted(str(p.relative_to(self.base)) for p in self.base.rglob("*"))
        for value in ("../outside-synthetic", "0" * 63, "A" * 64, None):
            with self.subTest(value=value), self.assertRaises(IntegrityError):
                prepare_download(self.root, {**blob, "sha256": value})
        self.assertEqual(sorted(str(p.relative_to(self.base)) for p in self.base.rglob("*")), before)

    def test_reader_symlink_escape_rejected(self):
        import shutil
        shutil.rmtree(self.root / "reader")
        outside = self.base / "outside"
        outside.mkdir()
        try:
            (self.root / "reader").symlink_to(outside, target_is_directory=True)
        except OSError:
            self.skipTest("Current account cannot create symbolic links")
        entrypoint = (self.root / "index.html").read_bytes()
        with self.assertRaises(IntegrityError):
            export_html(self.store)
        self.assertEqual(list(outside.iterdir()), [])
        self.assertEqual((self.root / "index.html").read_bytes(), entrypoint)

    def test_download_symlink_escape_rejected(self):
        import shutil
        sha = self.store.db.execute("SELECT sha256 FROM blobs").fetchone()[0]
        shutil.rmtree(self.root / "downloads" / sha)
        outside = self.base / "outside"
        outside.mkdir()
        try:
            (self.root / "downloads" / sha).symlink_to(outside, target_is_directory=True)
        except OSError:
            self.skipTest("Current account cannot create symbolic links")
        with self.assertRaises(IntegrityError):
            export_html(self.store)
        self.assertEqual(list(outside.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
