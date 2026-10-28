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
}
int main(){try{
 initializeDpi();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 INITCOMMONCONTROLSEX controls{sizeof controls,ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES|ICC_DATE_CLASSES};requireUi(InitCommonControlsEx(&controls),"common controls initialize");
 WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"ChatAtlasDpiRegression";requireUi(RegisterClassW(&wc)!=0,"test class registers");
 auto window=CreateWindowExW(0,wc.lpszClassName,L"ChatAtlas synthetic DPI test",WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_HSCROLL,0,0,940,880,nullptr,nullptr,wc.hInstance,nullptr);requireUi(window!=nullptr,"real application controls create");ShowWindow(window,SW_SHOWNOACTIVATE);
 for(UINT dpi:{96u,120u,144u,192u,96u}){
  RECT suggested{0,0,MulDiv(940,dpi,96),MulDiv(880,dpi,96)};
  SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),reinterpret_cast<LPARAM>(&suggested));
  requireUi(app.dpi==dpi,"DPI state updates");fontHeight(GetDlgItem(window,ID_START),MulDiv(16,dpi,96));fontHeight(GetDlgItem(window,ID_INFO),MulDiv(13,dpi,96));
  RECT button{};requireUi(GetWindowRect(GetDlgItem(window,ID_START),&button),"button bounds readable");requireUi(button.right-button.left==MulDiv(200,dpi,96),"button width follows DPI");requireUi(button.bottom-button.top==MulDiv(40,dpi,96),"button height follows DPI");
  RECT actual{};GetWindowRect(window,&actual);auto work=workArea(window);requireUi(actual.left>=work.left&&actual.top>=work.top&&actual.right<=work.right&&actual.bottom<=work.bottom,"window stays within work area");
 }
 SetWindowPos(window,nullptr,0,0,600,400,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);layout();SCROLLINFO info{sizeof(info),SIF_ALL};requireUi(GetScrollInfo(window,SB_VERT,&info)&&info.nMax>=819&&info.nPage<820,"small window has vertical scroll range");
 scrollView(false,SB_BOTTOM);requireUi(app.scrollY>0,"scroll reaches lower controls");scrollView(false,SB_TOP);requireUi(app.scrollY==0,"scroll returns to top");
 SetFocus(GetDlgItem(window,ID_START));revealFocus();RECT client{},button{};GetClientRect(window,&client);GetWindowRect(GetDlgItem(window,ID_START),&button);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&button),2);requireUi(button.top>=0&&button.bottom<=client.bottom,"keyboard focus reveals lower control");
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
 app.historyRoot=good;app.historyTotal=1;app.history=J::array({row});drawHistory();requireUi(ListView_GetItemCount(app.historyList)==1,"history list renders actual row");
 auto beforeChecked=app.checked;auto wrong=row;wrong["account"]="another-account";bool wrongRejected=false;try{applyHistory(wrong);}catch(const Error&){wrongRejected=true;}requireUi(wrongRejected&&app.checked==beforeChecked&&!app.busy,"wrong account history cannot alter selection or start capture");
 applyHistory(row);requireUi(!app.busy&&app.checked.count(chat["source_id"].get<std::string>()),"history applies known chat without automatic capture");requireUi(SendMessageW(GetDlgItem(window,ID_MESSAGE_MODE),CB_GETCURSEL,0,0)==1,"history restores skip text mode");requireUi(dateRange(ID_FROM,false)==row["options"]["since"].get<std::string>()&&dateRange(ID_UNTIL,true)==row["options"]["until"].get<std::string>(),"history UTC range roundtrips through Taiwan dates");requireUi(readText(prefs)==priorPreferences,"applying history does not commit preference before starting");
 auto selectorSnapshot=[&]{return J{{"folder",narrow(getText(app.folder))},{"mode",SendMessageW(GetDlgItem(window,ID_MESSAGE_MODE),CB_GETCURSEL,0,0)},{"from",dateRange(ID_FROM,false)},{"until",dateRange(ID_UNTIL,true)},{"messages",SendMessageW(GetDlgItem(window,ID_MESSAGES),BM_GETCHECK,0,0)},{"albums",SendMessageW(GetDlgItem(window,ID_ALBUMS),BM_GETCHECK,0,0)},{"notes",SendMessageW(GetDlgItem(window,ID_NOTES),BM_GETCHECK,0,0)},{"checked",app.checked}};};auto beforeFailedApply=selectorSnapshot();
 auto badOptions=row;badOptions["options"]["since"]="2026-10-26T17:00:00+00:00";bool badPeriod=false;try{applyHistory(badOptions);}catch(const Error&){badPeriod=true;}requireUi(badPeriod&&!app.busy&&selectorSnapshot()==beforeFailedApply,"failed history apply preserves every selector");
 ListView_SetItemState(app.historyList,0,LVIS_SELECTED,LVIS_SELECTED);auto archiveBytes=digest(good/L"archive.sqlite3").hash;historyCommand(ID_HISTORY_DELETE,[]{return false;});requireUi(!app.busy&&readText(prefs)==priorPreferences&&digest(good/L"archive.sqlite3").hash==archiveBytes,"cancel history deletion opens no Store and leaves data/preferences unchanged");
 {Db db(good/L"archive.sqlite3");db.execute("PRAGMA user_version=1");}auto beforeConsent=digest(good/L"archive.sqlite3").hash;auto denied=approveArchiveOpen(good,[]{return false;});requireUi(!denied&&digest(good/L"archive.sqlite3").hash==beforeConsent&&readText(prefs)==priorPreferences,"cancel schema upgrade leaves database and preferences untouched");{Db db(good/L"archive.sqlite3");db.execute("PRAGMA user_version=2");}

 // A failed preference write after publication must still report success and deliver the location.
 SetWindowTextW(app.folder,parent.c_str());bool attemptedRemember=false;auto committedMessage=finishPublishedImport(ImportResult{good,{}},[&](const fs::path& saved){attemptedRemember=true;requireUi(saved==good,"postcommit preference receives saved root");throw Error("injected preference write failure");});
 MSG location{};requireUi(PeekMessageW(&location,window,MSG_LOCATION,MSG_LOCATION,PM_REMOVE)!=0,"published import still posts location after preference failure");DispatchMessageW(&location);
 requireUi(attemptedRemember&&getText(app.folder)==good.wstring()&&readText(prefs)==priorPreferences,"postcommit warning preserves old preferences and displays saved root");requireUi(committedMessage.find("已匯入")!=std::string::npos&&committedMessage.find("偏好位置未能記住")!=std::string::npos&&committedMessage.find(utf8(good))!=std::string::npos,"postcommit failure stays success with explicit location and warning");
 auto originalConfig=app.config;requireUi(originalConfig.filename()==L"ChatAtlasData"&&originalConfig.parent_path()==executableDirectory(),"DPI changes preserve portable data location");
 DestroyWindow(window);CoUninitialize();std::cout<<checks<<" Win32 synthetic DPI checks passed; real multi-monitor and OS-version matrix remain separate.\n";return 0;
 }catch(const std::exception&e){if(app.window&&IsWindow(app.window))DestroyWindow(app.window);std::cerr<<e.what()<<"\n";return 1;}}
#else
int main(){std::cerr<<"This test requires Windows.\n";return 2;}
#endif
