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
 const auto first=root/L"naver_line_first.edb",second=root/L"naver_line_second.edb",manual=root/L"manually_selected.edb";
 for(const auto& path:{first,second,manual}){ExclusiveFile file(path);file.write("synthetic UI-only candidate; not a LINE database");file.sync();}
 const auto firstHash=digest(first).hash,secondHash=digest(second).hash;
 auto candidate=[](const fs::path& path){LineDatabaseCandidate value;value.path=path;value.size=46;return value;};
 auto makeLocations=[&](std::initializer_list<fs::path> paths){LineDatabaseLocations value;value.defaultDirectory=root;for(const auto& path:paths)value.candidates.push_back(candidate(path));return value;};
 {
  LineDatabasePicker state;state.locations=makeLocations({});auto dialog=createLineDatabasePicker(state);
  requireUi(dialog&&ListView_GetItemCount(state.list)==0&&state.selected.empty(),"empty database discovery has no invented selection");
  requireUi(!IsWindowEnabled(state.connect)&&!state.done,"empty database picker cannot connect automatically");
  SendMessageW(dialog,WM_COMMAND,IDOK,0);requireUi(!state.done&&state.result.empty(),"enter without a database cannot accept an empty source");
  SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);requireUi(state.done&&state.result.empty(),"empty picker cancels without returning a path");DestroyWindow(dialog);
 }
 {
  LineDatabasePicker state;state.locations=makeLocations({first});auto dialog=createLineDatabasePicker(state);
  requireUi(state.selected==first&&getText(state.path)==first.wstring()&&IsWindowEnabled(state.connect),"single discovered database is displayed in full and ready for confirmation");
  requireUi(!state.done&&state.result.empty(),"single discovered database never connects without confirmation");
  requireUi((GetWindowLongPtrW(state.path,GWL_STYLE)&ES_READONLY)!=0,"source path is copyable display rather than an editable output folder");
  SendMessageW(dialog,WM_COMMAND,IDOK,0);requireUi(state.done&&state.result==first,"explicit confirmation returns the sole displayed database");DestroyWindow(dialog);
 }
 {
  LineDatabasePicker state;state.locations=makeLocations({first,second});auto dialog=createLineDatabasePicker(state);
  requireUi(ListView_GetItemCount(state.list)==2&&state.selected.empty()&&!IsWindowEnabled(state.connect),"multiple accounts require an explicit selection");
  ListView_SetItemState(state.list,1,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
  requireUi(state.selected==second&&getText(state.path)==second.wstring()&&IsWindowEnabled(state.connect),"selecting the second real list row updates the full source path");
  const auto selectedText=getText(state.path),summary=getText(state.summary);bool browsed=false;
  state.browse=[&](const fs::path& initial){browsed=initial==root;return fs::path{};};
  SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_BROWSE,0);
  requireUi(browsed&&state.selected==second&&getText(state.path)==selectedText&&getText(state.summary)==summary&&!state.done,"manual file cancellation preserves the chosen account and visible instructions");
  state.browse=[&](const fs::path&){return root/L"archive.sqlite3";};SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_BROWSE,0);
  requireUi(state.selected==second&&getText(state.path)==selectedText&&state.result.empty()&&!state.done,"collection SQLite file cannot replace the LINE source selection");
  state.browse=[&](const fs::path&){return manual;};SendMessageW(dialog,WM_COMMAND,ID_DB_PICK_BROWSE,0);
  requireUi(state.selected==manual&&getText(state.path)==manual.wstring()&&ListView_GetNextItem(state.list,-1,LVNI_SELECTED)==-1,"manual source replaces the visible selection without leaving a different account highlighted");
  requireUi(!state.done&&state.result.empty()&&IsWindowEnabled(state.connect),"manual selection still requires explicit connection confirmation");
  SendMessageW(dialog,WM_COMMAND,IDOK,0);requireUi(state.done&&state.result==manual,"confirming manual selection returns that exact source path");DestroyWindow(dialog);
 }
 for(UINT message:{UINT(WM_COMMAND),UINT(WM_CLOSE)}){
  LineDatabasePicker state;state.locations=makeLocations({first});auto dialog=createLineDatabasePicker(state);
  SendMessageW(dialog,message,message==WM_COMMAND?WPARAM(IDCANCEL):WPARAM(0),0);
  requireUi(state.done&&state.result.empty(),"cancel button and window close both discard the pending database choice");DestroyWindow(dialog);
 }
 const auto oldDpi=app.dpi;
 for(UINT dpi:{96u,192u}){
  app.dpi=dpi;rebuildFonts();LineDatabasePicker state;state.locations=makeLocations({first,second});auto dialog=createLineDatabasePicker(state);
  RECT client{};GetClientRect(dialog,&client);std::vector<RECT> controlBounds;
  for(int id:{ID_DB_PICK_LIST,ID_DB_PICK_PATH,ID_DB_PICK_BROWSE,IDOK,IDCANCEL}){RECT bounds{};auto control=GetDlgItem(dialog,id);requireUi(control&&GetWindowRect(control,&bounds),"database picker has a real source or action control");MapWindowPoints(nullptr,dialog,reinterpret_cast<POINT*>(&bounds),2);requireUi(bounds.left>=0&&bounds.top>=0&&bounds.right<=client.right&&bounds.bottom<=client.bottom&&bounds.right>bounds.left&&bounds.bottom>bounds.top,"database selection and actions remain inside the work-area-sized window");controlBounds.push_back(bounds);}
  for(size_t i=0;i<controlBounds.size();++i)for(size_t j=i+1;j<controlBounds.size();++j){RECT overlap{};requireUi(!IntersectRect(&overlap,&controlBounds[i],&controlBounds[j]),"database list, path and action buttons never overlap one another");}
  fontHeight(state.path,MulDiv(16,dpi,96));DestroyWindow(dialog);
 }
 app.dpi=oldDpi;rebuildFonts();
 requireUi(digest(first).hash==firstHash&&digest(second).hash==secondHash,"showing and changing database candidates never modifies source bytes");
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
  requireUi(app.dpi==dpi,"DPI state updates");fontHeight(GetDlgItem(window,ID_START),MulDiv(16,dpi,96));fontHeight(GetDlgItem(window,ID_INFO),MulDiv(13,dpi,96));
  RECT button{};requireUi(GetWindowRect(GetDlgItem(window,ID_START),&button),"button bounds readable");requireUi(button.right-button.left==MulDiv(200,dpi,96),"button width follows DPI");requireUi(button.bottom-button.top==MulDiv(40,dpi,96),"button height follows DPI");
  RECT actual{};GetWindowRect(window,&actual);auto work=workArea(window);requireUi(actual.left>=work.left&&actual.top>=work.top&&actual.right<=work.right&&actual.bottom<=work.bottom,"window stays within work area");
 }
 databasePickerUiChecks();
 SetWindowPos(window,nullptr,0,0,600,400,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);layout();SCROLLINFO info{sizeof(info),SIF_ALL};requireUi(GetScrollInfo(window,SB_VERT,&info)&&info.nMax>=819&&info.nPage<820,"small window has vertical scroll range");
 for(int cycle=0;cycle<4;cycle++){scrollView(false,SB_BOTTOM);scrollView(false,SB_TOP);requireUi(!app.layingOut,"scroll repaint exits layout guard");requireUi(GetUpdateRect(window,nullptr,FALSE)==0,"scroll batch redraw consumes parent invalidation");}
 scrollView(false,SB_BOTTOM);requireUi(app.scrollY>0,"scroll reaches lower controls");scrollView(false,SB_TOP);requireUi(app.scrollY==0,"scroll returns to top");
 SetFocus(GetDlgItem(window,ID_START));revealFocus();RECT client{},button{};GetClientRect(window,&client);GetWindowRect(GetDlgItem(window,ID_START),&button);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&button),2);requireUi(button.top>=0&&button.bottom<=client.bottom,"keyboard focus reveals lower control");
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
 ListView_SetItemState(app.historyList,0,LVIS_SELECTED,LVIS_SELECTED);auto archiveBytes=digest(good/L"archive.sqlite3").hash;historyCommand(ID_HISTORY_DELETE,[]{return false;});requireUi(!app.busy&&readText(prefs)==priorPreferences&&digest(good/L"archive.sqlite3").hash==archiveBytes,"cancel history deletion opens no Store and leaves data/preferences unchanged");
 {Db db(good/L"archive.sqlite3");db.execute("PRAGMA user_version=1");}auto beforeConsent=digest(good/L"archive.sqlite3").hash;auto denied=approveArchiveOpen(good,[]{return false;});requireUi(!denied&&digest(good/L"archive.sqlite3").hash==beforeConsent&&readText(prefs)==priorPreferences,"cancel schema upgrade leaves database and preferences untouched");{Db db(good/L"archive.sqlite3");db.execute("PRAGMA user_version=2");}

 // A failed preference write after publication must still report success and deliver the location.
 SetWindowTextW(app.folder,parent.c_str());bool attemptedRemember=false;auto committedMessage=finishPublishedImport(ImportResult{good,{},"synthetic reader cleanup warning"},[&](const fs::path& saved){attemptedRemember=true;requireUi(saved==good,"postcommit preference receives saved root");throw Error("injected preference write failure");});
 MSG location{};requireUi(PeekMessageW(&location,window,MSG_LOCATION,MSG_LOCATION,PM_REMOVE)!=0,"published import still posts location after preference failure");DispatchMessageW(&location);
 requireUi(attemptedRemember&&getText(app.folder)==good.wstring()&&readText(prefs)==priorPreferences,"postcommit warning preserves old preferences and displays saved root");requireUi(committedMessage.find("已匯入")!=std::string::npos&&committedMessage.find("偏好位置未能記住")!=std::string::npos&&committedMessage.find(utf8(good))!=std::string::npos,"postcommit failure stays success with explicit location and warning");
 requireUi(publishedZipMessage({})=="完整收藏副本已保存。","published ZIP without retained snapshot has plain success");
 requireUi(committedMessage.find("synthetic reader cleanup warning")!=std::string::npos,"published import includes reader cleanup warning without hiding location or preference notice");
 auto retainedZipSnapshot=good/L"backup-synthetic.sqlite3";auto zipMessage=publishedZipMessage(retainedZipSnapshot);requireUi(zipMessage.find("完整收藏副本已保存。")!=std::string::npos&&zipMessage.find("未能清除")!=std::string::npos&&zipMessage.find(utf8(retainedZipSnapshot))!=std::string::npos,"published ZIP cleanup failure preserves success and discloses exact retained private snapshot");
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
 lineDatabaseClicked([&]{pickedDatabase=true;return fs::path{};});
 requireUi(pickedDatabase&&app.source==nativeSource&&app.chats==connectedChats&&app.checked==connectedChecked&&app.report==freshReport&&!app.busy,"cancelling a new database selection preserves the current source and connection report");
 requireUi(getText(app.folder)==connectedFolder&&readText(prefs)==connectedPreferences,"cancelling a new database selection preserves collection location and preference bytes");
 activatePage(1);requireUi(IsWindowVisible(GetDlgItem(window,ID_MEDIA_IMPORT)),"original-file fill-in belongs to the collection page");taskControls(true);requireUi(!IsWindowEnabled(GetDlgItem(window,ID_MEDIA_IMPORT)),"attachment fill-in is disabled while collecting");taskControls(false);requireUi(IsWindowEnabled(GetDlgItem(window,ID_MEDIA_IMPORT)),"attachment fill-in is restored after a task");
 clearConnection();nativeSource.reset();taskControls(false);requireUi(IsWindowEnabled(GetDlgItem(window,ID_ALBUMS)),"disconnect restores other source content controls");
 auto originalConfig=app.config;requireUi(originalConfig.filename()==L"ChatAtlasData"&&originalConfig.parent_path()==executableDirectory(),"DPI changes preserve portable data location");
 DestroyWindow(window);CoUninitialize();std::cout<<checks<<" Win32 synthetic DPI checks passed; real multi-monitor and OS-version matrix remain separate.\n";return 0;
 }catch(const std::exception&e){if(app.worker.joinable()){app.control->stopped=true;app.control->paused=false;app.worker.join();}if(app.window&&IsWindow(app.window))DestroyWindow(app.window);std::cerr<<e.what()<<"\n";return 1;}}
#else
int main(){std::cerr<<"This test requires Windows.\n";return 2;}
#endif
