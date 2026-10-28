import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from line_vault.collector import CaptureOptions, Collector
from line_vault.demo import SyntheticSource
from line_vault.errors import Cancelled, CapabilityError, DownloadUnavailable, IntegrityError, VaultError
from line_vault.exporter import DOWNLOAD_CHUNK, download_parts, export_html, make_zip
from line_vault.model import Attachment, Chat, Item
from line_vault.safeio import within
from line_vault.store import Store


class LifecycleTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name)
        self.root=self.base/'archive'
        self.store=Store(self.root)
        self.addCleanup(lambda: self.store.close())


    def seed(self):
        s=SyntheticSource()
        return Collector(self.store,s).run(s.list_chats())

    def test_store_retains_canonical_root(self):
        self.store.close()
        intermediate = self.base / "intermediate"
        intermediate.mkdir()
        alias = intermediate / ".." / self.root.name
        self.store = Store(alias)
        self.assertEqual(self.store.root, self.root.resolve(strict=True))
        self.seed()
        self.assertEqual(self.store.check(), [])
        export_html(self.store)
        self.assertEqual(self.store.check(), [])

    @unittest.skipUnless(os.name == "nt", "Windows 8.3 path alias regression")
    def test_windows_short_path_alias(self):
        import ctypes
        self.store.close()
        long_path = str(self.root.resolve(strict=True))
        short_path = ctypes.windll.kernel32.GetShortPathNameW
        short_path.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint]
        short_path.restype = ctypes.c_uint
        required = short_path(long_path, None, 0)
        self.assertGreater(required, 0)
        buffer = ctypes.create_unicode_buffer(required)
        self.assertGreater(short_path(long_path, buffer, required), 0)
        self.store = Store(Path(buffer.value))
        self.assertEqual(self.store.root, Path(long_path))
        self.seed()
        self.assertEqual(self.store.check(), [])
        export_html(self.store)
        make_zip(self.store, self.base / "short-path-backup.zip")
        self.assertEqual(self.store.check(), [])

    def test_second_process_cannot_publish_same_archive(self):
        code='from line_vault.store import Store;from pathlib import Path;Store(Path(__import__("sys").argv[1]))'
        p=subprocess.run([sys.executable,'-c',code,str(self.root)],capture_output=True,text=True,encoding='utf-8',
                         env=dict(os.environ,PYTHONIOENCODING='utf-8'))
        self.assertNotEqual(p.returncode,0)
        self.assertIn('正在使用中',p.stderr)

    def test_lock_is_released_after_close(self):
        self.store.close()
        self.store=Store(self.root)
        self.assertEqual(self.store.check(),[])

    def test_bad_database_releases_lock(self):
        folder=self.base/'bad';folder.mkdir();(folder/'archive.sqlite3').write_bytes(b'invalid sqlite')
        for _ in range(2):
            with self.assertRaisesRegex(IntegrityError,'資料庫無法讀取'):Store(folder)

    def test_interrupted_job_is_visible_after_reopen(self):
        self.seed()
        cid=self.store.db.execute('SELECT id FROM chats LIMIT 1').fetchone()[0]
        with self.store.transaction():self.store.db.execute("INSERT INTO jobs(chat_id,status,started_at) VALUES(?,'running','synthetic')",(cid,))
        self.store.close();self.store=Store(self.root)
        row=self.store.db.execute('SELECT status,finished_at FROM jobs ORDER BY id DESC LIMIT 1').fetchone()
        self.assertEqual(row['status'],'interrupted');self.assertTrue(row['finished_at'])

    def test_job_counts_are_per_chat(self):
        self.seed()
        rows=list(self.store.db.execute('SELECT counts FROM jobs ORDER BY id'))
        self.assertEqual([json.loads(r[0])['items'] for r in rows],[6,8])
        self.assertEqual([json.loads(r[0])['downloaded'] for r in rows],[2,0])

    def test_temporary_transfer_is_retried(self):
        class Temporary(SyntheticSource):
            def __init__(self):super().__init__();self.attempts=0
            def download(self,*args):
                self.attempts+=1
                if self.attempts==1:raise OSError('private diagnostic')
                return super().download(*args)
        s=Temporary();r=Collector(self.store,s).run(s.list_chats())
        self.assertEqual(r['failed'],0);self.assertEqual(s.attempts,3)

    def test_source_identity_failure_stops_without_retries(self):
        class Changed(SyntheticSource):
            def download(self,*args):raise CapabilityError('來源改變')
        with self.assertRaises(CapabilityError):Collector(self.store,Changed()).run(Changed().list_chats())
        self.assertEqual(self.store.db.execute('SELECT status FROM jobs').fetchone()[0],'failed')
        self.assertEqual(self.store.summary()['attachments'],{'pending':2})

    def test_expired_media_has_distinct_status(self):
        class Expired(SyntheticSource):
            def download(self,*args):raise DownloadUnavailable('private response')
        result=Collector(self.store,Expired()).run(Expired().list_chats())
        self.assertEqual(result['unavailable'],4)
        self.assertEqual(self.store.summary()['attachments'],{'unavailable':4})
        self.assertNotIn('private',str(list(self.store.db.execute('SELECT reason FROM attachments'))))

    def test_only_selected_content_is_collected(self):
        s=SyntheticSource();options=CaptureOptions(frozenset({'note','comment'}))
        Collector(self.store,s,options=options).run(s.list_chats())
        self.assertEqual(self.store.summary()['items'],4)
        self.assertEqual(self.store.summary()['attachments'],{})
        self.assertEqual(s.downloads,0)

    def test_related_content_cannot_be_split(self):
        with self.assertRaises(VaultError):CaptureOptions(frozenset({'album_item'}))

    def test_subchat_adds_parent_context_without_collecting_it(self):
        class Sub(SyntheticSource):
            def list_chats(self):return [Chat('c','Example community','community'),Chat('s','Example subchat','subchat','c')]
        s=Sub();Collector(self.store,s).run([s.list_chats()[1]])
        self.assertEqual(self.store.summary()['chats'],2)
        self.assertEqual(self.store.db.execute('SELECT count(DISTINCT chat_id) FROM items').fetchone()[0],1)
        self.assertEqual(self.store.check(),[])

    def test_missing_community_rejected_before_collection(self):
        class Sub(SyntheticSource):
            def list_chats(self):return [Chat('s','Example subchat','subchat','absent')]
        s=Sub()
        with self.assertRaises(IntegrityError):Collector(self.store,s).run(s.list_chats())
        self.assertEqual(self.store.summary()['items'],0)

    def test_orphan_contextual_item_rejected(self):
        with self.store.transaction():cid=self.store.put_chat('a',Chat('c','Example'))
        with self.assertRaises(IntegrityError):
            with self.store.transaction():self.store.put_items(cid,[Item('p','album_item')])

    def test_detached_raw_file_is_reported_and_retained(self):
        self.seed();path=within(self.root,'assets/unregistered.bin');path.write_bytes(b'synthetic')
        self.assertIn('unregistered_file',{p['code'] for p in self.store.check()})
        self.assertTrue(path.exists())
        dest=self.base/'copy.zip';make_zip(self.store,dest)
        import zipfile
        with zipfile.ZipFile(dest) as z:self.assertNotIn('assets/unregistered.bin',z.namelist())

    def test_damaged_relations_are_detected(self):
        self.seed()
        parent=self.store.db.execute("SELECT id FROM items WHERE kind='note' LIMIT 1").fetchone()[0]
        child=self.store.db.execute("SELECT id FROM items WHERE kind='album_item' LIMIT 1").fetchone()[0]
        with self.store.transaction():self.store.db.execute('UPDATE items SET parent_id=? WHERE id=?',(parent,child))
        self.assertIn('relations',{p['code'] for p in self.store.check()})

    def test_broken_database_cycle_is_reported(self):
        self.seed();iid=self.store.db.execute("SELECT id FROM items WHERE kind='comment' LIMIT 1").fetchone()[0]
        with self.store.transaction():self.store.db.execute('UPDATE items SET parent_id=id WHERE id=?',(iid,))
        self.assertIn('relations',{p['code'] for p in self.store.check()})

    def test_recalled_title_and_metadata_leave_current_content(self):
        self.seed();cid=self.store.db.execute('SELECT id FROM chats LIMIT 1').fetchone()[0]
        with self.store.transaction():self.store.put_items(cid,[Item('m1','message','old text',title='old title',metadata={'text':'old'},state='recalled')])
        row=self.store.db.execute("SELECT text,title,metadata FROM items WHERE chat_id=? AND source_id='m1'",(cid,)).fetchone()
        self.assertEqual(tuple(row),('','','{}'))

    def test_zip_refuses_corrupt_original(self):
        self.seed();p=self.store.db.execute('SELECT path FROM blobs LIMIT 1').fetchone()[0]
        within(self.root,p).write_bytes(b'corrupt')
        with self.assertRaises(IntegrityError):make_zip(self.store,self.base/'copy.zip')
        self.assertFalse((self.base/'copy.zip').exists());self.assertFalse((self.base/'copy.zip.partial').exists())

    def test_transport_parts_rebuild_when_damaged(self):
        self.seed();export_html(self.store)
        blob=self.store.db.execute('SELECT * FROM blobs LIMIT 1').fetchone()
        info=download_parts(self.store,blob);p=within(self.root,info['parts'][0]['path'])
        before=p.read_bytes();p.write_bytes(b'corrupt');download_parts(self.store,blob)
        self.assertEqual(p.read_bytes(),before)

    def test_empty_and_multichunk_transport_preserve_bytes(self):
        for size in [0,DOWNLOAD_CHUNK*2+7]:
            payload=(b'0123456789abcdef'*(size//16+1))[:size]
            sha=hashlib.sha256(payload).hexdigest();rel=f'assets/{sha[:2]}/{sha}.bin'
            path=within(self.root,rel);path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(payload)
            info=download_parts(self.store,{'path':rel,'sha256':sha,'size':size})
            import base64
            chunks=[]
            for part in info['parts']:
                script=within(self.root,part['path']).read_text(encoding="utf-8")
                args=json.loads('['+script.removeprefix('window.VaultBytes.add(').removesuffix(');\n')+']')
                chunks.append(base64.b64decode(args[2]))
            self.assertEqual(b''.join(chunks),payload)

    def test_transport_cache_cannot_include_unrelated_file(self):
        self.seed();export_html(self.store)
        blob=self.store.db.execute('SELECT * FROM blobs LIMIT 1').fetchone();info=download_parts(self.store,blob)
        info['parts'][0]['path']='connection.local.json'
        within(self.root,'downloads/'+blob['sha256']+'/info.json').write_text(json.dumps(info), encoding="utf-8")
        fixed=download_parts(self.store,blob)
        self.assertTrue(fixed['parts'][0]['path'].startswith('downloads/'))

    def test_copied_object_recovers_after_interrupted_database_commit(self):
        self.seed()
        with self.store.transaction():
            self.store.db.execute("UPDATE attachments SET blob_id=NULL,status='pending'")
            self.store.db.execute('DELETE FROM blobs')
        s=SyntheticSource();result=Collector(self.store,s).run(s.list_chats())
        self.assertEqual((s.downloads,result['reused']),(0,4))
        self.assertEqual(self.store.check(),[])

    def test_inconsistent_source_version_does_not_replace_old_metadata(self):
        with self.store.transaction():
            cid=self.store.put_chat('a',Chat('c','Example'))
            self.store.put_items(cid,[Item('m','message')])
            aid=self.store.put_attachment(cid,Attachment('f','m','file.bin',version='v1',expected_size=10))
        with self.assertRaises(IntegrityError):
            with self.store.transaction():self.store.put_attachment(cid,Attachment('f','m','file.bin',version='v1',expected_size=11))
        self.assertEqual(self.store.db.execute('SELECT expected_size FROM attachments WHERE id=?',(aid,)).fetchone()[0],10)

    def test_reader_check_distinguishes_rebuild_from_redownload(self):
        self.seed();export_html(self.store)
        blob=self.store.db.execute('SELECT * FROM blobs LIMIT 1').fetchone();info=download_parts(self.store,blob)
        within(self.root,info['parts'][0]['path']).write_bytes(b'corrupt')
        self.assertIn('reader_data',{p['code'] for p in self.store.check()})
        export_html(self.store)
        self.assertEqual(self.store.check(),[])

    def test_source_cannot_silently_drop_unknown_attachment_parent(self):
        class Bad(SyntheticSource):
            def read(self,*args):return [],[Attachment('a','absent','Example.bin')],'visible_only'
        with self.assertRaises(IntegrityError):Collector(self.store,Bad()).run(Bad().list_chats())

    def test_cancelled_publication_keeps_previous_reader_entry(self):
        self.seed();export_html(self.store);before=(self.root/'index.html').read_bytes()
        def stop():raise Cancelled('stopped')
        with self.assertRaises(Cancelled):export_html(self.store,stop)
        self.assertEqual((self.root/'index.html').read_bytes(),before)
        self.assertEqual(self.store.check(),[])

    def test_cancelled_zip_does_not_leave_a_misleading_completed_file(self):
        self.seed();destination=self.base/'cancelled.zip'
        def stop_when_writing():
            partial=destination.with_name(destination.name+'.partial')
            if partial.exists() and partial.stat().st_size:raise Cancelled('stopped')
        with self.assertRaises(Cancelled):make_zip(self.store,destination,checkpoint=stop_when_writing)
        self.assertFalse(destination.exists());self.assertFalse(destination.with_name(destination.name+'.partial').exists())
        self.assertEqual(self.store.check(),[])


if __name__=='__main__':unittest.main()
