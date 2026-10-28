import hashlib
import json
import tempfile
import unittest
import zipfile
from pathlib import Path

from line_vault.collector import Collector,Control
from line_vault.demo import SyntheticSource
from line_vault.errors import Cancelled,IntegrityError
from line_vault.exporter import export_html,make_zip,safe_json
from line_vault.model import Attachment,Chat,Item,timestamp
from line_vault.safeio import atomic_bytes,copy_verified,digest,safe_name,within
from line_vault.store import Store


class ArchiveCase(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base=Path(self.tmp.name);self.root=self.base/"私人封存 資料"
        self.store=Store(self.root)
        self.addCleanup(lambda: self.store.close())
    def seed(self):
        source=SyntheticSource()
        result=Collector(self.store,source).run(source.list_chats())
        return source,result

    def test_first_collection(self):
        s,r=self.seed();self.assertEqual(r["downloaded"],2);self.assertEqual(self.store.check(),[])
    def test_new_relation_reuses_exact_known_content(self):
        source,result=self.seed()
        self.assertEqual((source.downloads,result["reused"]),(2,2))
        self.assertEqual(self.store.db.execute("SELECT count(*) FROM blobs").fetchone()[0],2)
    def test_second_collection_has_zero_downloads(self):
        self.seed();s=SyntheticSource(2)
        r=Collector(self.store,s).run(s.list_chats())
        self.assertEqual((r["reused"],s.downloads),(4,0));self.assertEqual(self.store.summary()["items"],14)
    def test_latest_text_replaces_old(self):
        self.seed();s=SyntheticSource(2);Collector(self.store,s).run(s.list_chats())
        rows=self.store.db.execute("SELECT text FROM items WHERE source_id='m1'").fetchall()
        self.assertEqual(len(rows),2);self.assertTrue(all("第二次" in r[0] for r in rows))
    def test_unobserved_message_is_retained(self):
        self.seed();cid=self.store.db.execute("SELECT id FROM chats LIMIT 1").fetchone()[0]
        with self.store.transaction():self.store.put_items(cid,[Item("new","message","new")])
        self.assertIsNotNone(self.store.db.execute("SELECT 1 FROM items WHERE chat_id=? AND source_id='m1'",(cid,)).fetchone())
    def test_missing_file_is_redownloaded(self):
        self.seed();p=self.store.db.execute("SELECT path FROM blobs LIMIT 1").fetchone()[0];within(self.root,p).unlink()
        s=SyntheticSource();r=Collector(self.store,s).run(s.list_chats())
        self.assertGreater(r["downloaded"],0);self.assertEqual(self.store.check(),[])
    def test_corrupt_file_is_redownloaded(self):
        self.seed();p=self.store.db.execute("SELECT path FROM blobs LIMIT 1").fetchone()[0];within(self.root,p).write_bytes(b"bad")
        s=SyntheticSource();Collector(self.store,s).run(s.list_chats());self.assertEqual(self.store.check(),[])
    def test_same_name_different_content_not_merged(self):
        cid=self.add_chat()
        with self.store.transaction():
            self.store.put_items(cid,[Item("1","message"),Item("2","message")])
            one=self.store.put_attachment(cid,Attachment("a1","1","same.txt"))
            two=self.store.put_attachment(cid,Attachment("a2","2","same.txt"))
        self.assertNotEqual(one,two)
    def test_shared_media_keeps_each_message_relation(self):
        cid=self.add_chat()
        with self.store.transaction():
            self.store.put_items(cid,[Item("1","message"),Item("2","message")])
            one=self.store.put_attachment(cid,Attachment("same-object","1","same.txt"))
            two=self.store.put_attachment(cid,Attachment("same-object","2","same.txt"))
        self.assertNotEqual(one,two)
    def add_chat(self):
        with self.store.transaction():return self.store.put_chat("account",Chat("chat","name"))
    def test_accounts_do_not_mix(self):
        with self.store.transaction():
            one=self.store.put_chat("a",Chat("same","同名"));two=self.store.put_chat("b",Chat("same","同名"))
        self.assertNotEqual(one,two)
    def test_same_name_chats_do_not_mix(self):
        with self.store.transaction():
            one=self.store.put_chat("a",Chat("one","同名"));two=self.store.put_chat("a",Chat("two","同名"))
        self.assertNotEqual(one,two)
    def test_missing_parent_rejected_atomically(self):
        cid=self.add_chat()
        with self.assertRaises(IntegrityError):
            with self.store.transaction():self.store.put_items(cid,[Item("a","comment",parent_source_id="missing")])
        self.assertEqual(self.store.summary()["items"],0)
    def test_parent_type_checked(self):
        cid=self.add_chat()
        with self.assertRaises(IntegrityError):
            with self.store.transaction():self.store.put_items(cid,[Item("a","note"),Item("b","album_item",parent_source_id="a")])
    def test_hierarchy_cycle_rejected(self):
        cid=self.add_chat()
        with self.assertRaises(IntegrityError):
            with self.store.transaction():self.store.put_items(cid,[Item("a","comment",parent_source_id="b"),Item("b","comment",parent_source_id="a")])
    def test_parent_after_child_works(self):
        cid=self.add_chat()
        with self.store.transaction():self.store.put_items(cid,[Item("b","album_item",parent_source_id="a"),Item("a","album")])
        self.assertEqual(self.store.check(),[])
    def test_cross_chat_parent_rejected(self):
        cid=self.add_chat()
        with self.store.transaction():
            other=self.store.put_chat("account",Chat("other","other"));self.store.put_items(other,[Item("parent","album")])
        with self.assertRaises(IntegrityError):
            with self.store.transaction():self.store.put_items(cid,[Item("a","album_item",parent_source_id="parent")])
    def test_attachment_without_parent_rejected(self):
        cid=self.add_chat()
        with self.assertRaises(IntegrityError):
            with self.store.transaction():self.store.put_attachment(cid,Attachment("a","missing","f"))
    def test_recall_removes_text_but_retains_asset(self):
        self.seed();row=self.store.db.execute("SELECT chat_id FROM items WHERE source_id='m2' LIMIT 1").fetchone();cid=row[0]
        with self.store.transaction():self.store.put_items(cid,[Item("m2","message","must disappear",state="recalled")])
        self.assertEqual(self.store.db.execute("SELECT text FROM items WHERE chat_id=? AND source_id='m2'",(cid,)).fetchone()[0],"")
        self.assertEqual(self.store.db.execute("SELECT saved_before FROM attachments WHERE chat_id=? AND source_id='file1'",(cid,)).fetchone()[0],1)
        self.assertEqual(self.store.check(),[])
    def test_empty_source_does_not_delete(self):
        self.seed();before=self.store.summary()
        cid=self.store.db.execute("SELECT id FROM chats LIMIT 1").fetchone()[0]
        with self.store.transaction():self.store.put_items(cid,[])
        self.assertEqual(before,self.store.summary())
    def test_duplicate_source_id_rejected(self):
        cid=self.add_chat()
        with self.assertRaises(IntegrityError):
            with self.store.transaction():self.store.put_items(cid,[Item("x","message"),Item("x","message")])
    def test_source_id_cannot_change_kind(self):
        cid=self.add_chat()
        with self.store.transaction():self.store.put_items(cid,[Item("x","message","old")])
        with self.assertRaises(IntegrityError):
            with self.store.transaction():self.store.put_items(cid,[Item("x","album")])
    def test_unrelated_database_not_modified(self):
        import sqlite3
        folder=self.base/"unrelated";folder.mkdir()
        c=sqlite3.connect(folder/"archive.sqlite3");c.execute("CREATE TABLE private_data(value)");c.commit();c.close()
        before=(folder/"archive.sqlite3").read_bytes()
        with self.assertRaises(IntegrityError):Store(folder)
        self.assertEqual((folder/"archive.sqlite3").read_bytes(),before)
    def test_attachment_hash_change_preserves_previous_file_relation(self):
        cid=self.add_chat()
        with self.store.transaction():
            self.store.put_items(cid,[Item("m","message")])
            a=self.store.put_attachment(cid,Attachment("file","m","same.txt",expected_sha256="1"*64))
            b=self.store.put_attachment(cid,Attachment("file","m","same.txt",expected_sha256="2"*64))
        self.assertNotEqual(a,b)
        self.assertEqual(self.store.db.execute("SELECT saved_before FROM attachments WHERE id=?",(a,)).fetchone()[0],1)
    def test_cancel_preserves_database(self):
        c=Control();c.stop()
        with self.assertRaises(Cancelled):Collector(self.store,SyntheticSource(),c).run(SyntheticSource().list_chats())
        self.assertEqual(self.store.check(),[])
    def test_failed_source_marks_job_failed(self):
        class Broken(SyntheticSource):
            def read(self,*args):raise IntegrityError("safe failure")
        with self.assertRaises(IntegrityError):Collector(self.store,Broken()).run(Broken().list_chats())
        self.assertEqual(self.store.db.execute("SELECT status FROM jobs").fetchone()[0],"failed")
    def test_failed_download_is_not_saved(self):
        class Broken(SyntheticSource):
            def download(self,*args):raise OSError("secret path")
        Collector(self.store,Broken()).run(Broken().list_chats())
        self.assertEqual(self.store.summary()["attachments"],{"failed":4})
        self.assertNotIn("secret",str([tuple(r) for r in self.store.db.execute("SELECT reason FROM attachments")]))
    def test_export_is_offline_and_escapes_script(self):
        cid=self.add_chat()
        with self.store.transaction():self.store.put_items(cid,[Item("x","message","</script><img src=https://example.invalid onerror=alert(1)>")])
        p=export_html(self.store);content=p.read_text(encoding="utf-8")
        self.assertIn("connect-src 'none'",content)
        chunks=list((self.root/"reader").rglob("*.js"));self.assertTrue(chunks)
        self.assertNotIn("</script>",chunks[0].read_text(encoding="utf-8"))
        self.assertIn("\\u003c/script>",chunks[0].read_text(encoding="utf-8"))
    def test_old_reader_generation_removed(self):
        self.seed();export_html(self.store);first=next((self.root/"reader").iterdir());export_html(self.store)
        self.assertFalse(first.exists());self.assertEqual(len(list((self.root/"reader").iterdir())),1)
    def test_export_hides_missing_download(self):
        self.seed();r=self.store.db.execute("SELECT path FROM blobs LIMIT 1").fetchone();within(self.root,r[0]).unlink();export_html(self.store)
        payload="".join(p.read_text(encoding="utf-8") for p in (self.root/"reader").rglob("*.js"))
        self.assertIn('"status":"failed"',payload)
    def test_archive_zip_contains_only_portable_files(self):
        self.seed();export_html(self.store);(self.root/"connection.local.json").write_text("private", encoding="utf-8")
        dest=self.base/"copy.zip";make_zip(self.store,dest)
        with zipfile.ZipFile(dest) as z:
            self.assertIn("index.html",z.namelist());self.assertIn("archive.sqlite3",z.namelist());self.assertNotIn("connection.local.json",z.namelist())
    def test_existing_zip_not_overwritten(self):
        dest=self.base/"copy.zip";dest.write_bytes(b"old")
        with self.assertRaises(IntegrityError):make_zip(self.store,dest)
        self.assertEqual(dest.read_bytes(),b"old")
    def test_zip_inside_archive_rejected(self):
        with self.assertRaises(IntegrityError):make_zip(self.store,self.root/"copy.zip")
    def test_selected_zip_fails_on_missing_attachment(self):
        self.seed()
        with self.assertRaises(IntegrityError):make_zip(self.store,self.base/"x.zip",["missing"])
        self.assertFalse((self.base/"x.zip").exists())
    def test_foreign_keys_enforced(self):
        import sqlite3
        with self.assertRaises(sqlite3.IntegrityError):
            with self.store.transaction():self.store.db.execute("DELETE FROM chats WHERE id='missing'");self.store.db.execute("INSERT INTO jobs(chat_id,status,started_at) VALUES('missing','x','x')")


class SafeIOCase(unittest.TestCase):
    def test_names(self):
        for name in ["../bad.exe","CON","LPT1.txt","a:b","a\\b","\x00"]:
            fixed=safe_name(name);self.assertNotIn("/",fixed);self.assertNotIn("\\",fixed);self.assertNotIn(":",fixed)
        self.assertEqual(safe_name("CON"),"_CON")
    def test_traversal(self):
        with tempfile.TemporaryDirectory() as d:
            for value in ["../x","/tmp/x","a\\b","C:/x"]:
                with self.assertRaises(IntegrityError):within(Path(d),value)
    def test_symlink_escape(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            try:(root/"link").symlink_to(root.parent,target_is_directory=True)
            except OSError:self.skipTest("此 Windows 帳號未提供建立連結權限；程式執行不要求提權。")
            with self.assertRaises(IntegrityError):within(root,"link/file")
    def test_copy_checks_size(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);src=root/"src";src.write_bytes(b"abc")
            with self.assertRaises(IntegrityError):copy_verified(src,root,"out",expected_size=4)
            self.assertFalse((root/"out").exists())
    def test_copy_checks_hash(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);src=root/"src";src.write_bytes(b"abc")
            with self.assertRaises(IntegrityError):copy_verified(src,root,"out",expected_hash="0"*64)
    def test_known_zero_byte_file(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);src=root/"src";src.write_bytes(b"")
            sha,size=copy_verified(src,root,"out",expected_size=0)
            self.assertEqual((sha,size),(hashlib.sha256(b"").hexdigest(),0))
    def test_time_zone_is_required(self):
        with self.assertRaises(ValueError):timestamp("2026-01-01T00:00:00")
        self.assertEqual(timestamp("2026-01-01T08:00:00+08:00"),"2026-01-01T00:00:00.000+00:00")
    def test_json_escape_preserves_data(self):
        data={"value":"</script>\u2028&"};self.assertEqual(json.loads(safe_json(data)),data)


if __name__=="__main__":unittest.main()
