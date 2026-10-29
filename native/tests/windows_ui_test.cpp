// Same-process messages exercise real Win32 controls without changing OS DPI.
// Do not send a pointer-bearing WM_DPICHANGED to another process from a script.
#define wWinMain atlasApplicationMain
#include "../src/app.cpp"
#undef wWinMain
#include <iostream>
#ifdef _WIN32
namespace {
int checks=0;
void requireUi(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
void fontHeight(HWND control,int expected){auto font=reinterpret_cast<HFONT>(SendMessageW(control,WM_GETFONT,0,0));LOGFONTW value{};requireUi(font&&GetObjectW(font,sizeof(value),&value)==sizeof(value),"control font exists");requireUi(value.lfHeight==-expected,"font follows DPI");}
void databasePickerUiChecks(){
 auto root=privateDir(app.config/L"DatabasePickerFixture");
 const auto first=root/L"naver_line_first.edb",second=root/L"naver_line_second.edb",manual=root/L"人工手動選取.DB";
 for(const auto& path:{first,second,manual}){ExclusiveFile file(path);file.write("synthetic UI-only candidate; not a LINE database");file.sync();}
 const auto firstHash=digest(first).hash,secondHash=digest(second).hash;
 auto buffer=[](std::initializer_list<std::wstring> words){std::vector<wchar_t> out;for(const auto& word:words){out.insert(out.end(),word.begin(),word.end());out.push_back(L'\0');}out.push_back(L'\0');return out;};
 auto rejected=[&](const std::vector<wchar_t>& encoded){bool failed=false;try{parseLineDatabaseFileSelection(encoded);}catch(const Error&){failed=true;}requireUi(failed,"malformed or oversized native multi-file buffer is rejected");};
 requireUi(parseLineDatabaseFileSelection(buffer({first.wstring()}))==std::vector<fs::path>{first},"single-file native selection preserves the complete path");
 requireUi(parseLineDatabaseFileSelection(buffer({root.wstring(),first.filename().wstring(),manual.filename().wstring()}))==std::vector<fs::path>({first,manual}),"native multi-file buffer preserves every Unicode edb and db selection");
 auto longDirectory=root;for(int depth=0;depth<6;++depth)longDirectory/=std::wstring(180,L'界');
 requireUi(parseLineDatabaseFileSelection(buffer({longDirectory.wstring(),L"one.EDB",L"two.db"}))==std::vector<fs::path>({longDirectory/L"one.EDB",longDirectory/L"two.db"}),"long Unicode directory and mixed-case suffixes are parsed without truncation");
 requireUi(parseLineDatabaseFileSelection(buffer({root.wstring(),L"same.edb",L"same.edb"})).size()==1,"duplicate paths in a native file response do not double-select a source");
 rejected({L'C',L':',L'\\',L'x'});rejected({L'C',L':',L'\\',L'x',L'\0'});
 rejected(buffer({root.wstring(),L"..\\outside.edb"}));rejected(buffer({root.wstring(),L"archive.sqlite3"}));
 rejected(buffer({L"relative.edb"}));rejected(buffer({root.wstring(),first.wstring()}));
 {
  std::vector<fs::path> paths;for(size_t index=0;index<64;++index)paths.push_back(root/(L"source-"+std::to_wstring(index)+L".edb"));
  requireUi(validateLineDatabaseSelection(paths)==paths,"all sixty-four explicit source paths survive selection validation");
  paths.push_back(root/L"one-too-many.db");bool failed=false;try{validateLineDatabaseSelection(paths);}catch(const Error&){failed=true;}requireUi(failed,"more than sixty-four explicit sources are refused rather than truncated");
  rejected(std::vector<wchar_t>(maxLineDatabaseSelectionChars+1,L'\0'));
 }
 auto candidate=[](const fs::path& path){LineDatabaseCandidate value;value.path=path;value.size=46;return value;};
 auto makeLocations=[&](std::initializer_list<fs::path> paths){LineDatabaseLocations value;value.defaultDirectory=root;for(const auto& path:paths)value.candidates.push_back(candidate(path));return value;};
 {
  LineDatabasePicker state;state.locations=makeLocations({});auto dialog=createLineDatabasePicker(state);
  requireUi(dialog&&ListView_GetItemCount(state.list)==0&&state.selected.empty(),"empty database discovery has no invented selection");
  requireUi(!IsWindowEnabled(state.connect)&&!state.done,"empty database picker cannot connect automatically");
  SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_ALL,0);requireUi(state.selected.empty(),"select all on an empty candidate list selects no files");
  SendMessageW(dialog,WM_COMMAND,IDOK,0);requireUi(!state.done&&state.result.empty(),"enter without a database cannot accept an empty source");
  SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);requireUi(state.done&&state.result.empty(),"empty picker cancels without returning a path");DestroyWindow(dialog);
 }
 {
  LineDatabasePicker state;state.locations=makeLocations({first});auto dialog=createLineDatabasePicker(state);
  requireUi(state.selected==std::vector<fs::path>{first}&&getText(state.path)==first.wstring()&&IsWindowEnabled(state.connect),"single discovered database is checked and displayed in full for confirmation");
  requireUi(!state.done&&state.result.empty(),"single discovered database never connects without confirmation");
  requireUi((GetWindowLongPtrW(state.path,GWL_STYLE)&ES_READONLY)!=0,"source path is copyable display rather than an editable output folder");
  SendMessageW(dialog,WM_COMMAND,IDOK,0);requireUi(state.done&&state.result==std::vector<fs::path>{first},"explicit confirmation returns the sole displayed database");DestroyWindow(dialog);
 }
 {
  LineDatabasePicker state;state.locations=makeLocations({first,second});auto dialog=createLineDatabasePicker(state);
  requireUi(ListView_GetItemCount(state.list)==2&&state.selected.empty()&&!IsWindowEnabled(state.connect),"multiple candidates are initially unchecked");
  requireUi((ListView_GetExtendedListViewStyle(state.list)&LVS_EX_CHECKBOXES)!=0&&(GetWindowLongPtrW(state.list,GWL_STYLE)&LVS_SINGLESEL)==0,"real source list supports checkboxes and Ctrl or Shift multi-row selection");
  ListView_SetItemState(state.list,1,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
  requireUi(state.selected.empty()&&getText(state.path)==second.wstring(),"highlighting a row previews its complete path without silently checking it");
  ListView_SetCheckState(state.list,1,TRUE);requireUi(state.selected==std::vector<fs::path>{second}&&IsWindowEnabled(state.connect),"checking the second row selects that exact source");
  SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_ALL,0);requireUi(state.selected==std::vector<fs::path>({first,second})&&getText(state.count).find(L"2")!=std::wstring::npos,"select all checks every source and updates the visible count");
  SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_CLEAR,0);requireUi(state.selected.empty()&&!IsWindowEnabled(state.connect),"clear unchecks every source and disables empty connection");
  ListView_SetItemState(state.list,0,LVIS_SELECTED,LVIS_SELECTED);SendMessageW(state.list,WM_KEYDOWN,VK_SPACE,0);
  requireUi(state.selected==std::vector<fs::path>({first,second}),"space checks all highlighted Ctrl or Shift rows together");
  SendMessageW(state.list,WM_KEYDOWN,VK_SPACE,0);requireUi(state.selected.empty(),"space can also clear all highlighted rows together");
  ListView_SetCheckState(state.list,0,TRUE);ListView_SetCheckState(state.list,1,TRUE);
  const auto selectedText=getText(state.path),summary=getText(state.summary);const auto selectedBefore=state.selected;bool browsed=false;
  state.browse=[&](const fs::path& initial){browsed=initial==root;return std::vector<fs::path>{};};SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_BROWSE,0);
  requireUi(browsed&&state.selected==selectedBefore&&getText(state.path)==selectedText&&getText(state.summary)==summary&&!state.done,"manual file cancellation preserves every checked source and visible instructions");
  state.browse=[&](const fs::path&){return std::vector<fs::path>{manual,root/L"archive.sqlite3"};};SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_BROWSE,0);
  requireUi(state.selected==selectedBefore&&getText(state.path)==selectedText&&state.result.empty()&&ListView_GetItemCount(state.list)==2&&!state.done,"a mixed invalid group cannot partially replace the earlier selection");
  state.browse=[&](const fs::path&){return std::vector<fs::path>{manual,first};};SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_BROWSE,0);
  requireUi(state.selected==std::vector<fs::path>({manual,first})&&getText(state.path)==manual.wstring()&&ListView_GetItemCount(state.list)==2,"manual multi-file selection replaces the list with every selected source");
  requireUi(!state.done&&state.result.empty()&&IsWindowEnabled(state.connect),"manual selection still requires explicit connection confirmation");
  SendMessageW(dialog,WM_COMMAND,IDOK,0);requireUi(state.done&&state.result==std::vector<fs::path>({manual,first}),"confirming multiple files returns every file rather than only the first one");DestroyWindow(dialog);
 }
 for(UINT message:{UINT(WM_COMMAND),UINT(WM_CLOSE)}){
  LineDatabasePicker state;state.locations=makeLocations({first,second});auto dialog=createLineDatabasePicker(state);SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_ALL,0);
  SendMessageW(dialog,message,message==WM_COMMAND?WPARAM(IDCANCEL):WPARAM(0),0);
  requireUi(state.done&&state.result.empty(),"cancel button and window close discard the pending batch even after all were checked");DestroyWindow(dialog);
 }
 const auto oldDpi=app.dpi;
 for(UINT dpi:{96u,192u}){
  app.dpi=dpi;rebuildFonts();LineDatabasePicker state;state.locations=makeLocations({first,second});auto dialog=createLineDatabasePicker(state);
  RECT client{};GetClientRect(dialog,&client);std::vector<RECT> controlBounds;
  for(int id:{ID_DB_PICK_LIST,ID_DB_PICK_PATH,ID_DB_PICK_BROWSE,ID_DB_PICK_ALL,ID_DB_PICK_CLEAR,ID_DB_PICK_COUNT,IDOK,IDCANCEL}){RECT bounds{};auto control=GetDlgItem(dialog,id);requireUi(control&&GetWindowRect(control,&bounds),"database picker has a real source or action control");MapWindowPoints(nullptr,dialog,reinterpret_cast<POINT*>(&bounds),2);requireUi(bounds.left>=0&&bounds.top>=0&&bounds.right<=client.right&&bounds.bottom<=client.bottom&&bounds.right>bounds.left&&bounds.bottom>bounds.top,"batch source selection and all actions remain inside the work-area-sized window");controlBounds.push_back(bounds);}
  for(size_t i=0;i<controlBounds.size();++i)for(size_t j=i+1;j<controlBounds.size();++j){RECT overlap{};requireUi(!IntersectRect(&overlap,&controlBounds[i],&controlBounds[j]),"batch source list, path, count and action buttons never overlap one another");}
  fontHeight(state.path,MulDiv(16,dpi,96));DestroyWindow(dialog);
 }
 app.dpi=oldDpi;rebuildFonts();requireUi(digest(first).hash==firstHash&&digest(second).hash==secondHash,"showing and changing database candidates never modifies source bytes");
}
Result finishUiTask(HWND window){
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);std::unique_ptr<Result> result;
 while(!result){
  if(std::chrono::steady_clock::now()>deadline){app.control->stopped=true;app.control->paused=false;app.preserveControl->stopped=true;app.preserveControl->paused=false;if(app.worker.joinable())app.worker.join();app.busy=false;taskControls(false);throw std::runtime_error("synthetic automatic capture exceeded thirty seconds");}
  MSG message{};
  if(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
   if(message.hwnd==window&&message.message==MSG_DONE)result.reset(reinterpret_cast<Result*>(message.lParam));
   else {TranslateMessage(&message);DispatchMessageW(&message);}
  }else MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);
 }
 auto copy=*result;SendMessageW(window,MSG_DONE,0,reinterpret_cast<LPARAM>(result.release()));return copy;
}
std::wstring cellText(HWND list,int row,int column){wchar_t value[2048]{};ListView_GetItemText(list,row,column,value,2048);return value;}
void workspaceSelectionUiChecks(HWND window){
 auto previousSource=app.source;auto previousChats=app.chats;auto previousChecked=app.checked;
 app.source=std::make_shared<DemoSource>();app.chats=J::array();app.checked.clear();app.chatOffset=0;
 for(int index=0;index<61;++index)app.chats.push_back({{"source_id","room-"+std::to_string(index)},{"title",std::string(index<40?"跑步社 ":"工作室 ")+std::to_string(index)},{"kind",index%2?"direct":"group"},{"source_label",index<30?"來源 1 · internal-source-name":"來源 2 · internal-source-name"}});
 refreshChatSources();for(int id:{ID_CHAT_TYPE,ID_CHAT_SOURCE,ID_CHAT_PAGE_SIZE})SendMessageW(GetDlgItem(window,id),CB_SETCURSEL,0,0);SetWindowTextW(app.search,L"");drawChats();
 requireUi(ListView_GetItemCount(app.list)==25&&app.filtered.size()==61&&app.checked.empty(),"chat list starts on a bounded unchecked page");
 command(ID_SELECT_PAGE);command(ID_CHAT_NEXT);requireUi(app.checked.size()==25&&app.chatOffset==25&&!ListView_GetCheckState(app.list,0),"next page retains earlier selection without checking new rows");
 command(ID_SELECT_PAGE);requireUi(app.checked.size()==50,"select this page adds the next page to stable selection");command(ID_CLEAR_PAGE);command(ID_CHAT_PREV);requireUi(app.checked.size()==25&&ListView_GetCheckState(app.list,0),"clear this page preserves checked rows on another page");
 SendMessageW(GetDlgItem(window,ID_CHAT_TYPE),CB_SETCURSEL,2,0);command(ID_CHAT_TYPE);requireUi(app.filtered.size()==31,"chat type filter distinguishes groups from direct conversations");
 SendMessageW(GetDlgItem(window,ID_CHAT_SOURCE),CB_SETCURSEL,2,0);command(ID_CHAT_SOURCE);requireUi(app.filtered.size()==16,"source filter combines with chat type filter");
 SetWindowTextW(app.search,L"工作室");requireUi(app.filtered.size()==11&&ListView_GetItemCount(app.list)==11,"name search combines with type and source filters");command(ID_CLEAR_ALL);requireUi(app.checked.empty()&&!IsWindowEnabled(GetDlgItem(window,ID_START)),"clear all also clears selected rows hidden by a filter");
 command(ID_SELECT_FILTERED);requireUi(app.checked.size()==11&&ListView_GetCheckState(app.list,0),"select filtered results checks every matching chat");
 SetWindowTextW(app.search,L"");for(int id:{ID_CHAT_TYPE,ID_CHAT_SOURCE})SendMessageW(GetDlgItem(window,id),CB_SETCURSEL,0,0);SendMessageW(GetDlgItem(window,ID_CHAT_PAGE_SIZE),CB_SETCURSEL,1,0);command(ID_CHAT_PAGE_SIZE);requireUi(ListView_GetItemCount(app.list)==50&&app.checked.size()==11,"page-size change preserves cross-page selection");command(ID_CHAT_NEXT);requireUi(ListView_GetItemCount(app.list)==11&&app.chatOffset==50,"the final chat page is reachable");
 app.chats=J::array({{{"source_id","safe-test-id"},{"title","u01234567890123456789012345678901"},{"kind","direct"}}});app.chatOffset=0;refreshChatSources();drawChats();requireUi(cellText(app.list,0,0)==L"名稱待確認","unknown identity is displayed as a human status instead of a raw identifier");
 app.source=previousSource;app.chats=previousChats;app.checked=previousChecked;app.chatOffset=0;for(int id:{ID_CHAT_TYPE,ID_CHAT_SOURCE,ID_CHAT_PAGE_SIZE})SendMessageW(GetDlgItem(window,id),CB_SETCURSEL,0,0);refreshChatSources();drawChats();
}
void workspaceBoundsUiChecks(){
 for(int page=0;page<5;++page){activatePage(page);std::vector<RECT> bounds;for(const auto& [control,owner]:app.children){if(owner>=0&&owner!=page)continue;auto found=app.bases.find(control);if(found==app.bases.end())continue;auto b=found->second;if(b.right<=b.left||b.bottom<=b.top)continue;requireUi(b.left>=0&&b.top>=0&&b.right<=std::max(880L,LONG(app.bases[app.tabs].right+24)),"all workspace controls stay inside the logical scroll canvas");for(const auto& earlier:bounds){RECT overlap{};requireUi(!IntersectRect(&overlap,&b,&earlier),"visible header, list, settings and footer controls do not overlap");}bounds.push_back(b);}}
 activatePage(0);
}
void resultTableViewportUiChecks(HWND window){
 auto previousDpi=app.dpi;RECT previousWindow{};GetWindowRect(window,&previousWindow);auto previousPage=app.page;auto previousReport=app.fileReport;auto previousProgress=app.showProgress;auto previousOffset=app.fileOffset;
 RECT compact{0,0,1024,720};SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(96,96),reinterpret_cast<LPARAM>(&compact));app.scrollX=app.scrollY=0;activatePage(1);app.showProgress=false;app.fileOffset=0;J rows=J::array();
 for(int index=0;index<5;++index)rows.push_back({{"chat_title","人工檔案表測試"},{"name","路線照片 "+std::to_string(index+1)+".jpg"},{"kind","image"},{"status","saved"},{"reason","原檔已保存，可離線開啟。"}});
 app.fileReport={{"files",rows},{"total",rows.size()},{"limit",50}};drawFileRows();layout();RECT table{};GetWindowRect(app.fileList,&table);requireUi(table.bottom-table.top>=140,"720-pixel window reserves a readable file table before allocating result notes");
 // A resized reason column adds the horizontal scrollbar seen in the real
 // report. Its height must not hide the first three complete file rows.
 ListView_SetColumnWidth(app.fileList,4,420);UpdateWindow(app.fileList);SCROLLINFO horizontal{sizeof(horizontal),SIF_ALL};requireUi(GetScrollInfo(app.fileList,SB_HORZ,&horizontal)&&horizontal.nMax>=static_cast<int>(horizontal.nPage),"file table viewport test includes an actual horizontal scrollbar");RECT client{},header{};GetClientRect(app.fileList,&client);GetWindowRect(ListView_GetHeader(app.fileList),&header);MapWindowPoints(nullptr,app.fileList,reinterpret_cast<POINT*>(&header),2);
 requireUi(ListView_GetItemCount(app.fileList)==5&&ListView_GetCountPerPage(app.fileList)>=3,"compact saved-result table has capacity for at least three complete rows");for(int index=0;index<3;++index){RECT row{};requireUi(ListView_GetItemRect(app.fileList,index,&row,LVIR_BOUNDS)&&row.top>=header.bottom&&row.bottom<=client.bottom&&row.bottom>row.top,"first three file rows remain fully below the header and above the horizontal scrollbar");}
 app.fileReport=std::move(previousReport);app.showProgress=previousProgress;app.fileOffset=previousOffset;SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(previousDpi,previousDpi),reinterpret_cast<LPARAM>(&previousWindow));activatePage(previousPage);drawFileRows();layout();
}
class StopAfterSavedFileSource:public DemoSource{
 public:std::atomic<bool> waiting{false};
 fs::path download(const J& chat,const J& file,const fs::path& destination,Control& control)override{if(downloads){waiting=true;for(;;)control.wait(20);}return DemoSource::download(chat,file,destination,control);}
};
void savedResultUiChecks(HWND window){
 resetCaptureResults();CaptureProgress event;event.phase="file";event.chat_id="synthetic-chat";event.chat_title="跑步小組";event.chat_index=1;event.chat_total=1;event.file_id="synthetic-file";event.file_name="路線照片.jpg";event.kind="image";event.variant="full";event.status="pending";event.file_index=1;event.file_total=1;
 SendMessageW(window,MSG_PROGRESS,0,reinterpret_cast<LPARAM>(new CaptureProgress(event)));requireUi(ListView_GetItemCount(app.fileList)==1&&cellText(app.fileList,0,1)==L"路線照片.jpg"&&cellText(app.fileList,0,3)==L"正在保存","typed file progress identifies the exact current file");
 event.phase="terminal";event.status="saved";event.reason="原檔已保存。";SendMessageW(window,MSG_PROGRESS,0,reinterpret_cast<LPARAM>(new CaptureProgress(event)));requireUi(ListView_GetItemCount(app.fileList)==1&&cellText(app.fileList,0,3)==L"原檔已保存"&&cellText(app.fileList,0,4)==L"原檔已保存。","terminal progress updates its existing row with the saved state and reason");
 auto source=std::make_shared<StopAfterSavedFileSource>();app.source=source;app.chats=source->chats();app.checked={app.chats[0]["source_id"].get<std::string>()};refreshChatSources();taskControls(false);activatePage(4);requireUi(IsWindowVisible(GetDlgItem(window,ID_ALBUMS))&&IsWindowEnabled(GetDlgItem(window,ID_ALBUMS)),"synthetic Stop source exposes its supported content settings");auto root=app.config/L"StoppedCaptureResult";SetWindowTextW(app.folder,root.c_str());
 for(int id:{ID_MESSAGES,ID_ALBUMS,ID_NOTES})SendMessageW(GetDlgItem(window,id),BM_SETCHECK,BST_CHECKED,0);for(int id:{ID_FROM,ID_UNTIL})SendMessageW(GetDlgItem(window,id),DTM_SETSYSTEMTIME,GDT_NONE,0);SendMessageW(GetDlgItem(window,ID_MESSAGE_MODE),CB_SETCURSEL,0,0);collectClicked([](const fs::path&){return true;});
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);while(!source->waiting){requireUi(std::chrono::steady_clock::now()<deadline,"capture reaches the second synthetic download within thirty seconds");MSG message{};if(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);if(!app.busy&&!source->waiting)throw std::runtime_error("synthetic Stop capture finished before its second download: "+narrow(getText(app.taskNote)));}else MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);}
 command(ID_STOP);auto stopped=finishUiTask(window);requireUi(stopped.stopped&&!stopped.error&&app.control->stopped&&app.page==1&&getText(app.taskNote).find(L"已保留")!=std::wstring::npos,"Stop retains its cancellation flag and leaves a persistent result workspace");
 requireUi(fs::is_regular_file(root/L"index.html"),"Stop publishes a readable partial result instead of losing the completed work");auto html=readText(root/L"index.html");auto marker=html.find("id=\"manifest\"");auto begin=html.find('>',marker),end=html.find("</script>",begin);requireUi(marker!=std::string::npos&&begin!=std::string::npos&&end!=std::string::npos,"partial reader has its offline manifest");auto manifest=J::parse(html.substr(begin+1,end-begin-1));requireUi(manifest.value("direct_file_downloads",false)&&manifest["downloads"].empty(),"Stop snapshot uses saved local originals without rebuilding base64 download parts");
 {Store saved(root);requireUi(saved.summary()["items"].get<int>()>0&&saved.db.rows("SELECT id FROM blobs").size()==1,"messages and the first completed original survive Stop");auto issues=saved.check();for(const auto& issue:issues)requireUi(issue["code"]!="file"&&issue["code"]!="reader_data","partial reader preserves valid original bytes and needs no generated download parts");}
 requireUi(!app.fileReport["files"].empty()&&ListView_GetItemCount(app.fileList)>=1,"Stop leaves per-file availability rows visible");fileReportClicked(true);auto checked=finishUiTask(window);requireUi(!checked.error&&app.fileVerifyContents&&getText(app.taskNote).find(L"路線色調參考.png")!=std::wstring::npos,"full inspection keeps concrete file names and reasons in the persistent result");
 // Hold the snapshot at its cancellation checkpoint so the second click tests
 // the real busy preservation phase even on a fast machine with tiny files.
 auto readerHash=digest(root/L"index.html").hash;startTask([root]{Store saved(root);app.control->stopped=true;app.preserveControl->paused=true;return preserveStoppedCapture(saved,root);},true);
 deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);while(!app.preserving){requireUi(std::chrono::steady_clock::now()<deadline,"reader preservation reaches its paused checkpoint within thirty seconds");MSG message{};if(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);if(!app.busy)throw std::runtime_error("synthetic preservation ended before Stop: "+narrow(getText(app.taskNote)));}else MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);}
 requireUi(app.busy&&IsWindowVisible(GetDlgItem(window,ID_STOP))&&IsWindowEnabled(GetDlgItem(window,ID_STOP)),"second Stop is an enabled action during reader preservation");SendMessageW(window,WM_COMMAND,ID_STOP,0);requireUi(app.control->stopped&&app.preserveControl->stopped&&!app.preserveControl->paused,"a second Stop targets reader preservation without resetting original cancellation");auto stoppedSnapshot=finishUiTask(window);requireUi(stoppedSnapshot.stopped&&!stoppedSnapshot.error&&getText(app.taskNote).find(L"閱讀頁整理也已停止")!=std::wstring::npos&&digest(root/L"index.html").hash==readerHash,"stopping the snapshot retains the previous readable result and a persistent explanation");
}
void automaticCaptureUiChecks(HWND window){
 auto parent=privateDir(app.config/L"AutoCaptureParent"),destination=parent/L"ChatAtlas-Collection";
 auto first=std::make_shared<DemoSource>(),second=std::make_shared<DemoSource>();second->account="synthetic-local-account";
 auto source=std::make_shared<LineBatchSource>(std::vector<std::shared_ptr<Source>>{first,second});
 const auto originalChats=source->chats();LineDatabaseLocations locations;
 LineDatabaseCandidate one,two;one.path=app.config/L"auto-first.edb";two.path=app.config/L"auto-second.db";locations.candidates={one,two};
 size_t connections=0;bool exactFiles=false;std::vector<fs::path> opened;
 LineBatchConnector connect=[&](const std::vector<fs::path>& files,Control& control,std::function<void(std::string)>){
  control.check();++connections;exactFiles=files==std::vector<fs::path>({one.path,two.path});LineBatchOpenResult result;result.source=source;result.entries={{one.path,"ready",""},{two.path,"ready",""}};return result;
 };
 activatePage(4);SetWindowTextW(app.folder,parent.c_str());SYSTEMTIME captureStart{};captureStart.wYear=1990;captureStart.wMonth=1;captureStart.wDay=1;SendMessageW(GetDlgItem(window,ID_FROM),DTM_SETSYSTEMTIME,GDT_VALID,reinterpret_cast<LPARAM>(&captureStart));SendMessageW(GetDlgItem(window,ID_UNTIL),DTM_SETSYSTEMTIME,GDT_NONE,0);SendMessageW(GetDlgItem(window,ID_MESSAGE_MODE),CB_SETCURSEL,1,0);activatePage(0);
 const auto capturedSince=dateRange(ID_FROM,false);requireUi(!capturedSince.empty(),"automatic capture fixture starts with an explicit date and skip-text option");
 autoCaptureClicked([&]{return locations;},connect,[&](const fs::path& page){opened.push_back(page);});
 requireUi(app.busy&&!IsWindowEnabled(GetDlgItem(window,ID_AUTO_CAPTURE)),"automatic capture starts a real worker and disables repeat activation");
 const auto completion=finishUiTask(window);
 requireUi(!completion.error&&!app.busy&&IsWindowEnabled(GetDlgItem(window,ID_AUTO_CAPTURE)),"automatic capture completes without a modal test prompt and restores the action control");
 requireUi(connections==1&&exactFiles,"automatic capture connects the complete discovered batch once");
 requireUi(app.source==source&&app.chats==originalChats&&app.checked.empty(),"automatic capture publishes every routed source chat without silently checking the processed batch");
 requireUi(dateRange(ID_FROM,false)==capturedSince&&dateRange(ID_UNTIL,true).empty()&&SendMessageW(GetDlgItem(window,ID_MESSAGE_MODE),CB_GETCURSEL,0,0)==1,"automatic connection publication preserves the captured date range and skip-text selection");
 requireUi(folderPath()==destination&&fs::is_regular_file(destination/L"archive.sqlite3")&&fs::is_regular_file(destination/L"index.html"),"one automatic click creates a dedicated collection and the offline reader");
 requireUi(opened==std::vector<fs::path>{destination/L"index.html"},"automatic capture opens the published reader through the injected callback without launching a browser in the test");
 {
  Store saved(destination);auto chats=saved.db.rows("SELECT account,count(*) AS n FROM chats GROUP BY account ORDER BY account");
  requireUi(chats.size()==2&&chats[0]["account"]=="synthetic-local-account"&&chats[1]["account"]=="synthetic-native-account"&&chats[0]["n"]==3&&chats[1]["n"]==3,"automatic capture stores original per-database accounts rather than a mixed batch namespace");
  requireUi(saved.summary()["items"].get<int>()>0&&saved.check().empty(),"automatic capture actually saves consistent messages and attachment bytes");
  const auto jobs=saved.history(100);requireUi(jobs.size()==originalChats.size(),"automatic batch records a completed capture job for every original chat");
  for(const auto& job:jobs)requireUi(job["options"]["since"]==capturedSince&&job["options"]["until"]==""&&job["options"]["message_mode"]=="skip_text","automatic capture job options retain the requested date and skip-text mode after source publication");
 }
 std::vector<fs::path> generations;for(const auto& entry:fs::directory_iterator(destination/L"text-exports"))if(entry.is_directory())generations.push_back(entry.path());
 requireUi(generations.size()==1,"automatic capture publishes exactly one completed text export generation");
 auto manifest=J::parse(readText(generations[0]/L"export-complete.json"));requireUi(manifest.value("complete",false)&&manifest["chats"].size()==originalChats.size(),"automatic capture writes a complete TXT manifest for every collected chat");
 for(const auto& chat:manifest["chats"]){auto txt=within(generations[0],chat["file"].get<std::string>());requireUi(digest(txt).hash==chat["sha256"].get<std::string>()&&readText(txt).find("聊藏對話紀錄")!=std::string::npos,"automatic capture publishes readable verified text instead of a success-only label");}
 const auto previousSource=app.source;const auto previousChats=app.chats;const auto preferences=readText(app.config/L"preferences.local.json");
 auto emptyParent=privateDir(app.config/L"AutoCaptureMissing");activatePage(0);SetWindowTextW(app.folder,emptyParent.c_str());bool connectedEmpty=false;
 autoCaptureClicked([]{return LineDatabaseLocations{};},[&](const std::vector<fs::path>&,Control&,std::function<void(std::string)>){connectedEmpty=true;return LineBatchOpenResult{};},[](const fs::path&){});
 auto missing=finishUiTask(window);requireUi(missing.error&&!connectedEmpty&&!fs::exists(emptyParent/L"ChatAtlas-Collection"),"missing automatic candidates report failure before connecting or creating a collection");
 requireUi(app.source==previousSource&&app.chats==previousChats&&readText(app.config/L"preferences.local.json")==preferences,"failed automatic discovery preserves the previous source and collection preferences");
 auto cancelledParent=privateDir(app.config/L"AutoCaptureCancelled");activatePage(0);SetWindowTextW(app.folder,cancelledParent.c_str());
 autoCaptureClicked([&]{return locations;},[&](const std::vector<fs::path>&,Control& control,std::function<void(std::string)>){control.stopped=true;LineBatchOpenResult result;result.source=source;return result;},[](const fs::path&){});
 auto cancelled=finishUiTask(window);requireUi(!fs::exists(cancelledParent/L"ChatAtlas-Collection")&&cancelled.message.find("已停止")!=std::string::npos&&app.source==previousSource,"stopping automatic capture before source publication creates no output or replacement connection");
 bool busyDiscovered=false;app.busy=true;autoCaptureClicked([&]{busyDiscovered=true;return locations;},connect,[](const fs::path&){});app.busy=false;
 requireUi(!busyDiscovered&&connections==1,"repeat automatic action while busy never starts another discovery or connector");
}
void completionPageUiChecks(HWND window,const fs::path& root){
 auto usableFocus=[] {auto focused=GetFocus();return focused&&app.bases.count(focused)&&IsWindowVisible(focused)&&IsWindowEnabled(focused);};
 activatePage(2);SetFocus(GetDlgItem(window,ID_UIA_CONNECT));auto source=std::make_shared<DemoSource>();startTask([source]{publishLineConnection(source,source->chats());return "人工來源已連接，等待選取聊天室。";},true);auto connected=finishUiTask(window);
 requireUi(!connected.error&&app.page==0&&TabCtrl_GetCurSel(app.tabs)==0&&IsWindowVisible(app.list)&&app.checked.empty()&&usableFocus(),"manual connection completion shows unchecked chats and leaves usable keyboard focus");
 activatePage(2);SetFocus(GetDlgItem(window,ID_PROBE));startTask([]{refreshProbeReport([](Control* control){control->check();return J{{"version","synthetic-page-check"}};});return "人工連線檢查已完成。";},true);auto probed=finishUiTask(window);
 requireUi(!probed.error&&app.page==2&&TabCtrl_GetCurSel(app.tabs)==2&&IsWindowVisible(GetDlgItem(window,ID_PROBE))&&usableFocus(),"probe completion retains the connection-check workspace and usable focus");
 activatePage(3);SetWindowTextW(app.folder,root.c_str());SetFocus(GetDlgItem(window,ID_HISTORY_REFRESH));historyCommand(ID_HISTORY_REFRESH);auto history=finishUiTask(window);
 requireUi(!history.error&&app.page==3&&TabCtrl_GetCurSel(app.tabs)==3&&IsWindowVisible(app.historyList)&&ListView_GetItemCount(app.historyList)>0&&usableFocus(),"history worker completion keeps populated history and keyboard focus visible");
 activatePage(0);fileReportClicked(false);auto files=finishUiTask(window);
 requireUi(!files.error&&app.page==1&&TabCtrl_GetCurSel(app.tabs)==1&&IsWindowVisible(app.fileList)&&ListView_GetItemCount(app.fileList)>0&&usableFocus(),"file inspection completion shows its saved-result workspace and usable focus");
}
J savedReaderManifest(const fs::path& page){auto html=readText(page);auto marker=html.find("id=\"manifest\"");auto begin=html.find('>',marker),end=html.find("</script>",begin);if(marker==std::string::npos||begin==std::string::npos||end==std::string::npos)throw std::runtime_error("saved reader has no manifest");return J::parse(html.substr(begin+1,end-begin-1));}
class RebuildFixtureSource:public Source{
 std::string bytes=std::string(8*1024*1024,'R'),hash=sha(bytes);
 public:int downloads=0;RebuildFixtureSource(){account="synthetic-native-account";}
 J chats()override{return J::array({{{"source_id","rebuild-room"},{"title","已保存檔案測試"},{"kind","group"}}});}
 Batch read(const J&,const std::set<std::string>&,const std::string&,const std::string&,Control& control)override{control.check();Batch batch;batch.coverage="source_range";batch.items=J::array({{{"source_id","saved-file-message"},{"kind","message"},{"text","已保存的合成原檔"},{"sender_id","synthetic-sender"},{"sender_name","人工來源"},{"sent_at","2026-10-24T09:00:00+08:00"},{"ordinal",0}}});batch.attachments=J::array({{{"source_id","saved-original"},{"item_source_id","saved-file-message"},{"name","已保存的原始資料.bin"},{"kind","file"},{"expected_size",bytes.size()},{"expected_sha256",hash}}});return batch;}
 fs::path download(const J&,const J&,const fs::path& destination,Control& control)override{control.check();++downloads;auto file=destination/L"payload.bin";atomicText(file,bytes);return file;}
};
void rebuildCommandUiChecks(HWND window){
 auto root=app.config/L"RebuildSavedReader";RebuildFixtureSource source;fs::path original;Digest before;
 {Store saved(root);Control control;collect(saved,source,source.chats(),control,{"message","thread"});exportHtml(saved,{},nullptr,false);auto blob=saved.db.rows("SELECT * FROM blobs")[0];original=within(root,blob["path"].get<std::string>());before=digest(original);}
 auto direct=savedReaderManifest(root/L"index.html");requireUi(direct.value("direct_file_downloads",false)&&direct["downloads"].empty(),"rebuild command fixture begins with a direct local-file snapshot");auto directHash=digest(root/L"index.html").hash;
 clearConnection();taskControls(false);activatePage(1);SetWindowTextW(app.folder,root.c_str());auto previousOpener=app.readerOpener;std::vector<fs::path> opened;app.readerOpener=[&](const fs::path& page){opened.push_back(page);};
 requireUi(!app.source&&IsWindowVisible(GetDlgItem(window,ID_REBUILD))&&IsWindowEnabled(GetDlgItem(window,ID_REBUILD)),"saved-reader upgrade is available with no connected LINE source");SendMessageW(window,WM_COMMAND,ID_REBUILD,0);auto rebuilt=finishUiTask(window);auto full=savedReaderManifest(root/L"index.html");
 requireUi(!rebuilt.error&&!rebuilt.stopped&&!full.value("direct_file_downloads",true)&&!full["downloads"].empty()&&digest(root/L"index.html").hash!=directHash,"real rebuild command atomically upgrades a direct snapshot to complete download data");requireUi(!app.source&&source.downloads==1&&digest(original).hash==before.hash&&digest(original).size==before.size&&opened==std::vector<fs::path>{root/L"index.html"},"reader rebuild reuses saved originals and opens the completed reader without reconnecting or downloading");
 {Store saved(root);requireUi(saved.check().empty(),"upgraded reader has valid original bytes and complete offline download parts");}
 auto completeHash=digest(root/L"index.html").hash;SendMessageW(window,WM_COMMAND,ID_REBUILD,0);SendMessageW(window,WM_COMMAND,ID_PAUSE,0);requireUi(app.busy&&app.control->paused&&IsWindowEnabled(GetDlgItem(window,ID_STOP)),"full reader regeneration exposes its real Pause and Stop controls");SendMessageW(window,WM_COMMAND,ID_STOP,0);auto cancelled=finishUiTask(window);
 requireUi(cancelled.stopped&&!cancelled.error&&digest(root/L"index.html").hash==completeHash&&!savedReaderManifest(root/L"index.html").value("direct_file_downloads",true)&&!savedReaderManifest(root/L"index.html")["downloads"].empty(),"cancelling full regeneration preserves the previous complete reader instead of removing or downgrading it");requireUi(!app.source&&source.downloads==1&&digest(original).hash==before.hash,"cancelled reader regeneration preserves saved originals without fetching again");app.readerOpener=std::move(previousOpener);
}

}
int main(){try{
 initializeDpi();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 INITCOMMONCONTROLSEX controls{sizeof controls,ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES|ICC_DATE_CLASSES};requireUi(InitCommonControlsEx(&controls),"common controls initialize");
 WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"ChatAtlasDpiRegression";requireUi(RegisterClassW(&wc)!=0,"test class registers");
 auto window=CreateWindowExW(0,wc.lpszClassName,L"ChatAtlas synthetic DPI test",mainWindowStyle,0,0,940,880,nullptr,nullptr,wc.hInstance,nullptr);requireUi(window!=nullptr,"real application controls create");ShowWindow(window,SW_SHOWNOACTIVATE);
 requireUi((GetWindowLongPtrW(window,GWL_STYLE)&WS_CLIPCHILDREN)!=0,"parent painting excludes child control areas");
 for(UINT dpi:{96u,120u,144u,192u,96u}){
  RECT suggested{0,0,MulDiv(940,dpi,96),MulDiv(880,dpi,96)};
  SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),reinterpret_cast<LPARAM>(&suggested));
  requireUi(app.dpi==dpi,"DPI state updates");fontHeight(GetDlgItem(window,ID_START),MulDiv(16,dpi,96));fontHeight(GetDlgItem(window,ID_INFO),MulDiv(14,dpi,96));
  RECT button{};requireUi(GetWindowRect(GetDlgItem(window,ID_START),&button),"button bounds readable");requireUi(button.right-button.left==MulDiv(200,dpi,96),"button width follows DPI");requireUi(button.bottom-button.top==MulDiv(36,dpi,96),"button height follows DPI");
  RECT actual{};GetWindowRect(window,&actual);auto work=workArea(window);requireUi(actual.left>=work.left&&actual.top>=work.top&&actual.right<=work.right&&actual.bottom<=work.bottom,"window stays within work area");workspaceBoundsUiChecks();
 }
 resultTableViewportUiChecks(window);workspaceSelectionUiChecks(window);databasePickerUiChecks();
 SetWindowPos(window,nullptr,0,0,600,400,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);layout();SCROLLINFO info{sizeof(info),SIF_ALL};requireUi(GetScrollInfo(window,SB_VERT,&info)&&info.nMax>=679&&info.nPage<680,"small window has vertical scroll range");
 for(int cycle=0;cycle<4;cycle++){scrollView(false,SB_BOTTOM);scrollView(false,SB_TOP);requireUi(!app.layingOut,"scroll repaint exits layout guard");requireUi(GetUpdateRect(window,nullptr,FALSE)==0,"scroll batch redraw consumes parent invalidation");}
 scrollView(false,SB_BOTTOM);requireUi(app.scrollY>0,"scroll reaches lower controls");scrollView(false,SB_TOP);requireUi(app.scrollY==0,"scroll returns to top");
 // At this size the normal footer uses the emergency scrolling canvas. Start
 // is intentionally disabled until a chat is selected; use an enabled action.
 auto focusTarget=GetDlgItem(window,ID_BROWSE);RECT client{},button{};GetClientRect(window,&client);GetWindowRect(focusTarget,&button);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&button),2);
 requireUi(app.page==0&&IsWindowVisible(focusTarget)&&IsWindowEnabled(focusTarget)&&button.bottom>client.bottom,"enabled lower action starts outside the compact viewport");
 SetFocus(focusTarget);requireUi(GetFocus()==focusTarget,"keyboard focus reaches the enabled lower action");revealFocus();GetClientRect(window,&client);GetWindowRect(focusTarget,&button);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&button),2);requireUi(button.left>=0&&button.right<=client.right&&button.top>=0&&button.bottom<=client.bottom,"keyboard focus reveals lower control");
 activatePage(1);requireUi(IsWindowVisible(GetDlgItem(window,ID_OPEN_DATA)),"program data button belongs to collection page");
 bool explored=false;auto preferenceBeforeExplore=fs::exists(app.config/L"preferences.local.json");openProgramData([&](const fs::path& path){explored=path==app.config;return intptr_t(33);});requireUi(explored&&fs::exists(app.config/L"preferences.local.json")==preferenceBeforeExplore,"data folder opener uses config without writing preferences");
 bool exploreFailed=false;try{openProgramData([](const fs::path&){return intptr_t(5);});}catch(const Error& e){exploreFailed=std::string(e.what()).find("無法開啟程式資料夾")!=std::string::npos;}requireUi(exploreFailed,"shell explorer failures are surfaced");
 for(int page:{0,1,2,3,1}){activatePage(page);requireUi(IsWindowVisible(GetDlgItem(window,ID_OPEN_DATA))==(page==1),"data button visibility follows active tab");requireUi(GetUpdateRect(window,nullptr,FALSE)==0,"tab switch redraws exposed parent surface");}
 taskControls(true);requireUi(!IsWindowEnabled(GetDlgItem(window,ID_OPEN_DATA)),"data folder management disabled while collecting");taskControls(false);requireUi(IsWindowEnabled(GetDlgItem(window,ID_OPEN_DATA)),"data folder management restored after task");activatePage(0);
 auto parent=privateDir(app.config/L"Parent");
 requireAclVolume(parent/L"not-created"/L"nested");requireUi(!fs::exists(parent/L"not-created"),"volume check does not create missing paths");
 auto cancelled=newCollectionPath(parent,[](const fs::path&){return false;});requireUi(!cancelled&&!fs::exists(parent/L"ChatAtlas-Collection"),"cancel creates no collection");
 auto accepted=newCollectionPath(parent,[](const fs::path&){return true;});requireUi(accepted&&accepted->parent_path()==parent&&fs::is_directory(*accepted),"accept creates dedicated child");requirePrivateDirectory(*accepted);
 bool refusedCollision=false;try{newCollectionPath(parent,[](const fs::path& target){privateDir(target);return true;});}catch(const Error&){refusedCollision=true;}requireUi(refusedCollision,"racing existing child is never adopted");
 auto existing=privateDir(app.config/L"Existing");{ExclusiveFile file(existing/L"archive.sqlite3");file.write("fixture untouched");file.sync();}bool asked=false;auto preserved=newCollectionPath(existing,[&](const fs::path&){asked=true;return true;});requireUi(preserved&&*preserved==existing&&!asked&&readText(existing/L"archive.sqlite3")=="fixture untouched","existing archive path and content remain unchanged");

 // Exercise the actual collect button path, not only newCollectionPath.
 auto prefs=app.config/L"preferences.local.json";requireUi(!fs::exists(prefs),"startup does not precreate preferences");
 app.source=std::make_shared<DemoSource>();app.chats=app.source->chats();app.checked={app.chats[0]["source_id"].get<std::string>()};
 SetWindowTextW(app.folder,parent.c_str());collectClicked([](const fs::path&){return false;});requireUi(!app.busy&&!fs::exists(prefs),"cancel full collect flow leaves absent preferences absent");
 rememberArchive(parent);auto priorPreferences=readText(prefs);SetWindowTextW(app.folder,existing.c_str());requireUi(folderPath()==existing&&readText(prefs)==priorPreferences,"reading edited folder never commits preference");
 SetWindowTextW(app.folder,parent.c_str());collectClicked([](const fs::path&){return false;});requireUi(!app.busy&&readText(prefs)==priorPreferences,"cancel full collect flow preserves existing preferences");
 auto invalid=app.config/L"NotAFolder";{ExclusiveFile file(invalid);file.write("untouched");file.sync();}SetWindowTextW(app.folder,invalid.c_str());bool invalidRejected=false;try{collectClicked([](const fs::path&){return true;});}catch(const Error&){invalidRejected=true;}catch(const fs::filesystem_error&){invalidRejected=true;}requireUi(invalidRejected&&!app.busy&&readText(prefs)==priorPreferences,"failed collection selection leaves preferences unchanged");
 auto good=app.config/L"PreferenceSuccess";{auto store=openRememberedArchive(good);requireUi(savedArchive(J::parse(readText(prefs)))==good,"successful Store opening commits final preference");}
 priorPreferences=readText(prefs);bool invalidOpen=false;try{auto store=openRememberedArchive(existing);}catch(...){invalidOpen=true;}requireUi(invalidOpen&&readText(prefs)==priorPreferences,"failed Store opening leaves last good preference unchanged");
 auto chat=app.chats[0];J row={{"id",1},{"account",app.source->account},{"source_id",chat["source_id"]},{"chat_title",chat["title"]},{"chat_kind",chat["kind"]},{"parent_source_id",chat.value("parent_source_id",J(nullptr))},{"status","complete"},{"started_at","2026-10-28T00:00:00.000+00:00"},{"counts",{{"items",1},{"reused",2}}},{"options",{{"version",1},{"kinds",J::array({"message","thread"})},{"since","2026-10-26T16:00:00.000+00:00"},{"until","2026-10-28T16:00:00.000+00:00"},{"message_mode","skip_text"}}}};
 for(auto [input,expected]:std::vector<std::pair<std::string,std::string>>{
 {"1970-01-01T00:00:00Z","1970-01-01 08:00:00"},
 {"2999-12-31T23:59:59Z","3000-01-01 07:59:59"},
 {"1970-01-01T00:00:00+14:00","時間無法辨識"},
 {"2999-12-31T23:59:59-14:00","時間無法辨識"},
 {"2026-10-28T00:00:00.000+00:00","2026-10-28 08:00:00"},
 {"2026-12-31T20:01:02Z","2027-01-01 04:01:02"},
 {"2024-02-28T20:00:00Z","2024-02-29 04:00:00"},
 {"2026-10-28T08:01:02+08:00","2026-10-28 08:01:02"},
 {"2026-10-28T23:01:02-04:00","2026-10-29 11:01:02"},
 {"2026-10-28T23:59:59.123456789Z","2026-10-29 07:59:59"},
 {"2026-10-28T00:00:00","時間無法辨識"},{"2026-02-30T00:00:00Z","時間無法辨識"},{"malformed","時間無法辨識"},{"","未記錄"}})requireUi(historyStarted(input)==expected,"history timestamp is explicit Taiwan display or marked invalid");
 app.historyRoot=good;app.historyTotal=1;app.history=J::array({row});drawHistory();requireUi(ListView_GetItemCount(app.historyList)==1,"history list renders actual row");wchar_t displayedTime[64]{};ListView_GetItemText(app.historyList,0,0,displayedTime,64);requireUi(std::wstring(displayedTime)==L"2026-10-28 08:00:00","actual history control displays UTC+8");requireUi(app.history[0]["started_at"]==row["started_at"],"rendering history does not mutate stored UTC value");
 auto beforeChecked=app.checked;auto wrong=row;wrong["account"]="another-account";bool wrongRejected=false;try{applyHistory(wrong);}catch(const Error&){wrongRejected=true;}requireUi(wrongRejected&&app.checked==beforeChecked&&!app.busy,"wrong account history cannot alter selection or start capture");
 applyHistory(row);requireUi(!app.busy&&app.checked.count(chat["source_id"].get<std::string>()),"history applies known chat without automatic capture");requireUi(SendMessageW(GetDlgItem(window,ID_MESSAGE_MODE),CB_GETCURSEL,0,0)==1,"history restores skip text mode");requireUi(dateRange(ID_FROM,false)==row["options"]["since"].get<std::string>()&&dateRange(ID_UNTIL,true)==row["options"]["until"].get<std::string>(),"history UTC range roundtrips through Taiwan dates");requireUi(readText(prefs)==priorPreferences,"applying history does not commit preference before starting");
 auto selectorSnapshot=[&]{return J{{"folder",narrow(getText(app.folder))},{"mode",SendMessageW(GetDlgItem(window,ID_MESSAGE_MODE),CB_GETCURSEL,0,0)},{"from",dateRange(ID_FROM,false)},{"until",dateRange(ID_UNTIL,true)},{"messages",SendMessageW(GetDlgItem(window,ID_MESSAGES),BM_GETCHECK,0,0)},{"albums",SendMessageW(GetDlgItem(window,ID_ALBUMS),BM_GETCHECK,0,0)},{"notes",SendMessageW(GetDlgItem(window,ID_NOTES),BM_GETCHECK,0,0)},{"checked",app.checked}};};auto beforeFailedApply=selectorSnapshot();
 auto badOptions=row;badOptions["options"]["since"]="2026-10-26T17:00:00+00:00";bool badPeriod=false;try{applyHistory(badOptions);}catch(const Error&){badPeriod=true;}requireUi(badPeriod&&!app.busy&&selectorSnapshot()==beforeFailedApply,"failed history apply preserves every selector");
 activatePage(3);requireUi(IsWindowVisible(app.historyList),"history deletion fixture selects a visible retained job");ListView_SetItemState(app.historyList,0,LVIS_SELECTED,LVIS_SELECTED);auto archiveBytes=digest(good/L"archive.sqlite3").hash;historyCommand(ID_HISTORY_DELETE,[]{return false;});requireUi(!app.busy&&readText(prefs)==priorPreferences&&digest(good/L"archive.sqlite3").hash==archiveBytes,"cancel history deletion opens no Store and leaves data/preferences unchanged");
 {Db db(good/L"archive.sqlite3");db.execute("PRAGMA user_version=1");}auto beforeConsent=digest(good/L"archive.sqlite3").hash;auto denied=approveArchiveOpen(good,[]{return false;});requireUi(!denied&&digest(good/L"archive.sqlite3").hash==beforeConsent&&readText(prefs)==priorPreferences,"cancel schema upgrade leaves database and preferences untouched");{Db db(good/L"archive.sqlite3");db.execute("PRAGMA user_version=2");}

 // A failed preference write after publication must still report success and deliver the location.
 SetWindowTextW(app.folder,parent.c_str());bool attemptedRemember=false;auto committedMessage=finishPublishedImport(ImportResult{good,{},"synthetic reader cleanup warning"},[&](const fs::path& saved){attemptedRemember=true;requireUi(saved==good,"postcommit preference receives saved root");throw Error("injected preference write failure");});
 MSG location{};requireUi(PeekMessageW(&location,window,MSG_LOCATION,MSG_LOCATION,PM_REMOVE)!=0,"published import still posts location after preference failure");DispatchMessageW(&location);
 requireUi(attemptedRemember&&getText(app.folder)==good.wstring()&&readText(prefs)==priorPreferences,"postcommit warning preserves old preferences and displays saved root");requireUi(committedMessage.find("已匯入")!=std::string::npos&&committedMessage.find("偏好位置未能記住")!=std::string::npos&&committedMessage.find(utf8(good))!=std::string::npos,"postcommit failure stays success with explicit location and warning");
 requireUi(publishedZipMessage({})=="完整聊天紀錄副本已保存。","published ZIP without retained snapshot has plain success");
 requireUi(committedMessage.find("synthetic reader cleanup warning")!=std::string::npos,"published import includes reader cleanup warning without hiding location or preference notice");
 auto retainedZipSnapshot=good/L"backup-synthetic.sqlite3";auto zipMessage=publishedZipMessage(retainedZipSnapshot);requireUi(zipMessage.find("完整聊天紀錄副本已保存。")!=std::string::npos&&zipMessage.find("未能清除")!=std::string::npos&&zipMessage.find(utf8(retainedZipSnapshot))!=std::string::npos,"published ZIP cleanup failure preserves success and discloses exact retained private snapshot");
 // Commit a synthetic legacy upgrade, then inject only ancillary preference failure.
 auto migrationRoot=app.config/L"PreferenceFailureUpgrade";{Store fixture(migrationRoot);fixture.db.execute("PRAGMA user_version=1");}
 {bool attempted=false;auto opened=openRememberedArchive(migrationRoot,{},true,[&](const fs::path&){attempted=true;throw Error("synthetic preference failure");});requireUi(attempted&&readText(prefs)==priorPreferences,"post-upgrade preference failure preserves old preference bytes");requireUi(opened->db.rows("PRAGMA user_version")[0].begin().value()==2&&!opened->upgradeSnapshot.empty()&&fs::is_regular_file(opened->upgradeSnapshot),"preference failure does not undo committed upgrade or hide snapshot");auto notice=storeNotice(opened);requireUi(notice.find("偏好位置未能記住")!=std::string::npos&&notice.find(utf8(opened->upgradeSnapshot))!=std::string::npos,"opened archive reports preference warning together with upgrade snapshot");requireUi(opened->check({},false).empty(),"authorized integrity operation continues despite preference failure");}
 auto deletionRoot=app.config/L"HistoryRefreshFailure";{Store fixture(deletionRoot);DemoSource source;Control control;collect(fixture,source,source.chats(),control);auto row=fixture.history(1)[0];auto items=fixture.db.rows("SELECT * FROM items ORDER BY id"),blobs=fixture.db.rows("SELECT * FROM blobs ORDER BY id"),state=fixture.db.rows("SELECT * FROM capture_state ORDER BY chat_id");auto count=fixture.historyCount();bool refreshed=false;auto message=deleteHistoryAndRefresh(fixture,row["id"].get<int64_t>(),[&]{refreshed=true;throw Error("synthetic refresh failure");});requireUi(refreshed&&fixture.historyCount()==count-1,"refresh failure does not undo confirmed history deletion");requireUi(fixture.db.rows("SELECT * FROM items ORDER BY id")==items&&fixture.db.rows("SELECT * FROM blobs ORDER BY id")==blobs&&fixture.db.rows("SELECT * FROM capture_state ORDER BY chat_id")==state,"history refresh failure preserves content, dedupe and latest state");requireUi(message.find("已刪除")!=std::string::npos&&message.find("列表未能更新")!=std::string::npos,"committed deletion stays success with refresh warning");bool refreshOnFailure=false;try{deleteHistoryAndRefresh(fixture,row["id"].get<int64_t>(),[&]{refreshOnFailure=true;});requireUi(false,"missing history row must fail");}catch(const Error&){}requireUi(!refreshOnFailure,"failed deletion never reports a successful refresh");}
 Control afterPublication;afterPublication.stopped=true;bool browserOpened=false;auto savedPage=good/L"index.html";auto stoppedNotice=openPublishedReader(savedPage,afterPublication,[&](const fs::path&){browserOpened=true;});requireUi(!browserOpened&&stoppedNotice.find(utf8(savedPage))!=std::string::npos,"stop after reader publication skips browser and retains visible saved path");afterPublication.stopped=false;auto normalNotice=openPublishedReader(savedPage,afterPublication,[&](const fs::path& path){browserOpened=path==savedPage;});requireUi(browserOpened&&normalNotice.empty(),"normal published reader still opens without extra stop notice");
 // A failed or cancelled re-probe cannot export an earlier successful report.
 app.control=std::make_shared<Control>();const J earlierReport={{"version","synthetic-old"},{"publisher_verified",true}};
 refreshProbeReport([&](Control* control){requireUi(control==app.control.get(),"probe receives current cancellation control");return earlierReport;});requireUi(app.report==earlierReport,"successful report remains available for normal export");
 auto priorProbeBytes=safeJson(app.report);
 for(const char* reason:{"missing","ambiguous","bridge-failure"}){app.report=earlierReport;bool rejected=false;try{refreshProbeReport([&](Control*)->J{requireUi(app.report.is_null(),"old report is invalidated before re-probe work");throw CapabilityError(reason);});}catch(const CapabilityError&){rejected=true;}requireUi(rejected&&app.report.is_null(),"failed probe cannot expose stale successful report");}
 app.report=earlierReport;bool cancelledProbe=false;try{refreshProbeReport([](Control*)->J{throw Cancelled();});}catch(const Cancelled&){cancelledProbe=true;}requireUi(cancelledProbe&&app.report.is_null(),"cancelled probe invalidates earlier report");bool exportBlocked=false;try{command(ID_REPORT);}catch(const Error& e){exportBlocked=std::string(e.what()).find("請先執行連線檢查")!=std::string::npos;}requireUi(exportBlocked,"report command refuses export before opening a save dialog after cancelled probe");
 app.report=earlierReport;cancelledProbe=false;try{refreshProbeReport([&](Control* control)->J{control->stopped=true;return earlierReport;});}catch(const Cancelled&){cancelledProbe=true;}requireUi(cancelledProbe&&app.report.is_null(),"stop after inspection but before publication discards result");
 bool inspectedAfterStop=false;app.report=earlierReport;try{refreshProbeReport([&](Control*)->J{inspectedAfterStop=true;return earlierReport;});}catch(const Cancelled&){}requireUi(!inspectedAfterStop&&app.report.is_null(),"already stopped probe clears old report without inspecting");
 app.control=std::make_shared<Control>();const J freshReport={{"version","synthetic-new"},{"publisher_verified",true}};refreshProbeReport([&](Control*){return freshReport;});requireUi(app.report==freshReport&&safeJson(app.report)!=priorProbeBytes,"successful re-probe replaces old data and normal export serialization works");
 // Native connection results configure actual controls without reading a live process.
 auto linePath=app.config/L"synthetic-native-line.edb";const std::string lineKey="0123456789aBCDef0123456789aBCDef";
 {LineCipher writer(linePath,lineKey,nullptr,true);writer.rows("CREATE TABLE _chat(_id TEXT PRIMARY KEY,_lastUpdatedTime INTEGER)");writer.rows("CREATE TABLE _message(_id TEXT PRIMARY KEY,_chatId TEXT,_from TEXT,_createdTime INTEGER,_text TEXT,_contentType INTEGER,_contentMetadata TEXT)");writer.rows("INSERT INTO _chat VALUES('synthetic-chat',1790784000000)");writer.rows("INSERT INTO _message VALUES('synthetic-message','synthetic-chat','synthetic-person',1790784000000,'人工原生連線訊息',0,'{}')");}
 auto nativeSource=std::make_shared<LineDatabaseSource>(linePath,lineKey);auto result=new ConnectionResult{nativeSource,nativeSource->chats()};SendMessageW(window,MSG_CHATS,0,reinterpret_cast<LPARAM>(result));taskControls(false);
 requireUi(app.source==nativeSource&&app.chats.size()==1&&app.checked.empty(),"native database connection lists chats without silently selecting all of them");
 requireUi(SendMessageW(GetDlgItem(window,ID_MESSAGES),BM_GETCHECK,0,0)==BST_CHECKED&&!IsWindowEnabled(GetDlgItem(window,ID_ALBUMS))&&!IsWindowEnabled(GetDlgItem(window,ID_NOTES)),"native source enables messages and attachments while unsupported albums and notes stay disabled");
 requireUi(dateRange(ID_FROM,false).empty()&&dateRange(ID_UNTIL,true).empty(),"new native database clears previous source date selections");
 app.checked={"synthetic-chat"};drawChats();auto lineParent=privateDir(app.config/L"NativeImportParent");SetWindowTextW(app.folder,lineParent.c_str());bool askedLine=false;collectClicked([&](const fs::path& proposed){askedLine=proposed==lineParent/L"ChatAtlas-Collection";return false;});requireUi(askedLine&&!app.busy&&!fs::exists(lineParent/L"ChatAtlas-Collection"),"native collection cancellation creates no archive or background worker");
 app.report=freshReport;auto connectedChats=app.chats;auto connectedChecked=app.checked;const auto connectedFolder=getText(app.folder);const auto connectedPreferences=readText(prefs);bool pickedDatabase=false;
 lineDatabaseClicked([&]{pickedDatabase=true;return std::vector<fs::path>{};});
 requireUi(pickedDatabase&&app.source==nativeSource&&app.chats==connectedChats&&app.checked==connectedChecked&&app.report==freshReport&&!app.busy,"cancelling a new database selection preserves the current source and connection report");
 requireUi(getText(app.folder)==connectedFolder&&readText(prefs)==connectedPreferences,"cancelling a new database selection preserves collection location and preference bytes");
 activatePage(1);requireUi(IsWindowVisible(GetDlgItem(window,ID_MEDIA_IMPORT)),"original-file fill-in belongs to the collection page");taskControls(true);requireUi(!IsWindowEnabled(GetDlgItem(window,ID_MEDIA_IMPORT)),"attachment fill-in is disabled while collecting");taskControls(false);requireUi(IsWindowEnabled(GetDlgItem(window,ID_MEDIA_IMPORT)),"attachment fill-in is restored after a task");
 automaticCaptureUiChecks(window);savedResultUiChecks(window);completionPageUiChecks(window,app.config/L"StoppedCaptureResult");rebuildCommandUiChecks(window);
 clearConnection();nativeSource.reset();taskControls(false);requireUi(IsWindowEnabled(GetDlgItem(window,ID_ALBUMS)),"disconnect restores other source content controls");
 auto originalConfig=app.config;requireUi(originalConfig.filename()==L"ChatAtlasData"&&originalConfig.parent_path()==executableDirectory(),"DPI changes preserve portable data location");
 DestroyWindow(window);CoUninitialize();std::cout<<checks<<" Win32 synthetic DPI checks passed; real multi-monitor and OS-version matrix remain separate.\n";return 0;
 }catch(const std::exception&e){if(app.worker.joinable()){app.control->stopped=true;app.control->paused=false;app.worker.join();}if(app.window&&IsWindow(app.window))DestroyWindow(app.window);std::cerr<<e.what()<<"\n";return 1;}}
#else
int main(){std::cerr<<"This test requires Windows.\n";return 2;}
#endif
