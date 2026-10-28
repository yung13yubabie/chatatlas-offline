import unittest
import codecs
from pathlib import Path

from line_vault.errors import CapabilityError
from line_vault.windows.source import WindowsSource,single,validate_profile


class WorkerEncodingTests(unittest.TestCase):
    def test_worker_encoding_matches_embedded_payload(self):
        root = Path(__file__).resolve().parents[1]
        raw = (root / "src/line_vault/windows/uia_worker.ps1").read_bytes()
        self.assertTrue(raw.startswith(codecs.BOM_UTF8))
        text = raw.decode("utf-8-sig")
        self.assertTrue(text.startswith("$ErrorActionPreference"))
        self.assertNotIn("\ufeff", text)
        self.assertTrue(any(ord(char) > 127 for char in text))
        bundle = (root / "native/src/bundle.cpp").read_text(encoding="utf-8")
        embedded = bundle.split('const char*worker_script=R"ATLAS(', 1)[1].split(')ATLAS";', 1)[0]
        self.assertEqual(embedded, text)


class ProfileTests(unittest.TestCase):
    def test_empty_profile_is_not_a_live_source(self):
        with self.assertRaises(CapabilityError):validate_profile({})
    def test_partial_profile_is_not_full_capture(self):
        p={"schema":1,"account":{"selector":{"name":"synthetic"},"expected_value":"synthetic"},
           "chats":{"id":{"property":"automation_id"},"row":{"control_type":"ListItem"}},"panels":{}}
        with self.assertRaises(CapabilityError):validate_profile(p)
    def test_ambiguous_source_rejected(self):
        with self.assertRaises(CapabilityError):single([{},{}])
    def test_absent_source_rejected(self):
        with self.assertRaises(CapabilityError):single([])
    def test_literal_prefix_not_regex(self):
        s=WindowsSource.__new__(WindowsSource)
        value=s.field({"name":"prefix_123_end"},{"property":"name","prefix":"prefix_","suffix":"_end"})
        self.assertEqual(value,"123")
    def test_regular_expression_is_not_executed(self):
        s=WindowsSource.__new__(WindowsSource)
        with self.assertRaises(CapabilityError):s.field({"name":"aaaaaaaaaaaaaaaa!"},{"property":"name","pattern":"(a+)+$"})
    def test_runtime_id_not_persistent_identity(self):
        s=WindowsSource.__new__(WindowsSource)
        with self.assertRaises(CapabilityError):s.field({"runtime_id":"x"},{"property":"runtime_id"})
    def test_bad_field_prefix_rejected(self):
        s=WindowsSource.__new__(WindowsSource)
        with self.assertRaises(CapabilityError):s.field({"name":"unrelated"},{"prefix":"expected_"})


class LocalBindingTests(unittest.TestCase):
    def profile(self):
        panels={kind:{'container':{'automation_id':kind+'-container'},'row':{'automation_id':kind+'-row'},
                       'fields':{'source_id':{'property':'automation_id'}}} for kind in ['message','album','album_item','note','comment','thread']}
        panels['album_item']['fields']['parent_source_id']={'property':'name'}
        panels['comment']['fields']['parent_source_id']={'property':'name'}
        return {'schema':1,'account':{'selector':{'automation_id':'account'},'expected_value':'synthetic-account-check','local_id':'fixture-account'},
                'chats':{'container':{'automation_id':'chats'},'row':{'automation_id':'chat-row'},'id':{'property':'name','line':0},
                         'title':{'property':'name','line':1},'kind':'group','selected_title':{'automation_id':'selected-title'},
                         'selected_id':{'selector':{'automation_id':'selected-id'},'property':'text'}},'panels':panels}

    def bridge(self):
        class FakeBridge:
            def __init__(self):self.calls=[];self.closed=False;self.selected='fixture-chat';self.account='synthetic-account-check'
            def call(self,action,**values):
                self.calls.append((action,values))
                if action=='connect':return {'version':'fixture-only','publisher_verified':True}
                if action=='find':
                    label=values['selector'].get('automation_id')
                    names={'account':self.account,'chats':'Chat list','chat-row':'fixture-chat\nExample chat',
                           'selected-title':'Example chat','selected-id':self.selected,'message-container':'Messages','message-row':'Example text','thread-container':'Threads'}
                    if label not in names:return []
                    return [{'token':label,'automation_id':label,'name':names[label]}]
                if action=='text':
                    key=values['token']
                    return {'account':self.account,'selected-title':'Example chat','selected-id':self.selected}.get(key,'')
                if action in {'select','activate'}:return True
                raise AssertionError('Unexpected operation '+action)
            def close(self):self.closed=True
        return FakeBridge()

    def test_valid_local_binding_is_accepted(self):
        self.assertEqual(validate_profile(self.profile())['schema'],1)

    def test_capture_only_opens_selected_content(self):
        from line_vault.collector import Control
        b=self.bridge();s=WindowsSource(self.profile(),b);s.set_kinds({'message','thread'})
        items,media,coverage=s.read(s.list_chats()[0],None,None,Control())
        self.assertEqual(len(items),1);self.assertEqual(coverage,'visible_only')
        labels=[v['selector'].get('automation_id') for a,v in b.calls if a=='find']
        self.assertNotIn('album-container',labels);self.assertNotIn('note-container',labels)
        self.assertLess([a for a,_ in b.calls].index('activate'),[a for a,_ in b.calls].index('select'))

    def test_failed_initial_account_check_closes_worker(self):
        b=self.bridge();b.account='different-account'
        with self.assertRaises(CapabilityError):WindowsSource(self.profile(),b)
        self.assertTrue(b.closed)

    def test_same_title_different_chat_identity_is_rejected(self):
        b=self.bridge();s=WindowsSource(self.profile(),b);chat=s.list_chats()[0];b.selected='other-chat'
        with self.assertRaises(CapabilityError):s.assert_chat(chat)

    def test_profile_rejects_unknown_data_fields(self):
        p=self.profile();p['panels']['message']['fields']['execute']='anything'
        with self.assertRaises(CapabilityError):validate_profile(p)

    def test_profile_rejects_code_and_regex(self):
        p=self.profile();p['chats']['id']['pattern']='(a+)+$'
        with self.assertRaises(CapabilityError):validate_profile(p)

    def test_profile_rejects_missing_context_parent(self):
        p=self.profile();del p['panels']['comment']['fields']['parent_source_id']
        with self.assertRaises(CapabilityError):validate_profile(p)

    def test_profile_rejects_invalid_scroll_types(self):
        p=self.profile();p['panels']['message']['scroll']='false'
        with self.assertRaises(CapabilityError):validate_profile(p)

    def test_profile_rejects_nondictionary_fields(self):
        p=self.profile();p['panels']['message']['fields']=[]
        with self.assertRaises(CapabilityError):validate_profile(p)


if __name__=="__main__":unittest.main()
