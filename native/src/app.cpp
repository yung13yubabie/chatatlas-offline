#include "core.hpp"
#include "windows_source.hpp"
#include "import.hpp"
#include "line_database.hpp"
#include "line_cipher.hpp"
#include "line_process.hpp"
#include "line_media.hpp"
#include "line_locations.hpp"
#include "line_batch.hpp"
#include "text_export.hpp"
#ifdef _WIN32
#include "file.hpp"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <cctype>
#include <memory>
#include <mutex>
using namespace atlas;
namespace {
constexpr UINT MSG_DONE=WM_APP+1,MSG_STATUS=WM_APP+2,MSG_CHATS=WM_APP+3,MSG_HISTORY=WM_APP+4,MSG_LOCATION=WM_APP+5,MSG_PROGRESS=WM_APP+6,MSG_FILE_REPORT=WM_APP+7,MSG_PRESERVING=WM_APP+8;constexpr int ID_TABS=100,ID_CONNECT=101,ID_DEMO=102,ID_LIST=103,ID_SEARCH=104,ID_FOLDER=105,ID_BROWSE=106,ID_START=107,ID_PAUSE=108,ID_STOP=109,ID_OPEN=110,ID_CHECK=111,ID_REBUILD=112,ID_BACKUP=113,ID_PROBE=114,ID_PROFILE=115,ID_REPORT=116,ID_MESSAGES=117,ID_ALBUMS=118,ID_NOTES=119,ID_FROM=120,ID_UNTIL=121,ID_INFO=122,ID_MESSAGE_MODE=123,ID_HISTORY=124,ID_HISTORY_REFRESH=125,ID_HISTORY_APPLY=126,ID_HISTORY_DELETE=127,ID_IMPORT=128,ID_HISTORY_PREV=129,ID_HISTORY_NEXT=130,ID_HISTORY_PAGE=131,ID_OPEN_DATA=132,ID_LINE_DATABASE=133,ID_MEDIA_IMPORT=135,ID_UIA_CONNECT=136,ID_AUTO_CAPTURE=137,ID_CHAT_TYPE=138,ID_CHAT_SOURCE=139,ID_CHAT_PAGE_SIZE=140,ID_CHAT_PREV=141,ID_CHAT_NEXT=142,ID_CHAT_PAGE=143,ID_SELECT_PAGE=144,ID_CLEAR_PAGE=145,ID_SELECT_FILTERED=146,ID_CLEAR_ALL=147,ID_SELECTION_COUNT=148,ID_FILES=149,ID_FILE_STATE=150,ID_FILE_SEARCH=151,ID_FILE_PREV=152,ID_FILE_NEXT=153,ID_FILE_PAGE=154,ID_TASK_NOTE=155,ID_VIEW_RESULT=156,ID_SETTINGS=157,ID_HEADER=158,ID_SUBTITLE=159,ID_MAIN_HINT=160,ID_FOLDER_LABEL=161,ID_SETTINGS_TITLE=162,ID_RANGE_LABEL=163,ID_RANGE_TO=164,ID_TIME_HINT=165,ID_MODE_LABEL=166,ID_MODE_HINT=167,ID_CONTENT_LABEL=168,ID_RESULT_TITLE=169,ID_RESULT_HINT=170,ID_HISTORY_TITLE=171,ID_HISTORY_HINT=172,ID_HISTORY_NOTE=173,ID_PROBE_TITLE=174,ID_PROBE_HINT=175,ID_PROBE_NOTE=176,ID_IMPORT_HINT=177,ID_DATA_HINT=178,ID_FILE_REFRESH=179;
constexpr DWORD mainWindowStyle=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_SIZEBOX|WS_VSCROLL|WS_HSCROLL|WS_CLIPCHILDREN;
// Only the UI thread publishes a successfully initialized connection.
struct ConnectionResult{std::shared_ptr<Source> source;J chats;bool preserveOptions=false;};
struct HistoryResult{fs::path root;J rows;size_t offset=0;int64_t total=0;};
struct Result{std::string message;bool error=false,stopped=false;};
struct FileReportResult{fs::path root;J data;std::string state="all",query;};
fs::path executableDirectory(){
 std::vector<wchar_t> path(32768);const DWORD count=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
 if(!count||count>=path.size())throw Error("無法確認程式所在位置，請將完整程式包移到可寫入的私人資料夾。");
 return parentDirectoryPath(fs::path(std::wstring(path.data(),count)));
}
void requireAclVolume(const fs::path& p){
 requireLocalPrivatePath(p);
 std::vector<wchar_t> volume(32768);DWORD flags=0;
 if(!GetVolumePathNameW(p.c_str(),volume.data(),static_cast<DWORD>(volume.size()))||!GetVolumeInformationW(volume.data(),nullptr,0,nullptr,nullptr,&flags,nullptr,0))throw Error("無法確認保存磁碟的權限支援，請使用一般本機位置。");
 if(!(flags&FILE_PERSISTENT_ACLS))throw Error("這個磁碟不支援私人存取權限。請將程式與聊天紀錄放在支援權限的本機磁碟，不要使用 FAT32 或 exFAT 作為工作聊天紀錄。");
}
fs::path portableConfig(){
 fs::path probe;bool owned=false;
 try{
  requireAclVolume(executableDirectory());
  auto root=privateDir(executableDirectory()/L"ChatAtlasData");requirePrivateDirectory(root);
  probe=root/(L".write-check-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
  {ExclusiveFile file(probe);owned=true;file.sync();}
  if(!fs::remove(probe))throw Error("寫入檢查尚未完成。");owned=false;return root;
 }catch(const Error& e){if(owned){std::error_code ec;fs::remove(probe,ec);}throw Error(std::string(e.what())+"\n請將完整程式資料夾移到可寫入、未同步的一般本機位置，再重新開啟；若桌面或文件已開啟 OneDrive 同步，請改用其他位置。不會改用其他位置保存。");}
 catch(...){if(owned){std::error_code ec;fs::remove(probe,ec);}throw Error("程式旁的 ChatAtlasData 無法安全寫入。請檢查磁碟空間與資料夾權限；不會改用其他位置保存。");}
}
J archivePreference(const fs::path& selected){
 auto absolute=absoluteNormalizedPath(selected);auto relative=relativeToDirectory(absolute,executableDirectory());
 J value={{"last_archive",utf8(absolute)}};if(relative){auto name=utf8(*relative);std::replace(name.begin(),name.end(),'\\','/');value["last_archive"]=name;value["archive_base"]="portable";}return value;
}
fs::path savedArchive(const J& preference){
 auto name=preference.at("last_archive").get<std::string>();if(preference.value("archive_base","")=="portable"){if(name==".")return executableDirectory();return within(executableDirectory(),name);}
 auto selected=path8(name);if(!selected.is_absolute())throw Error("聊天紀錄位置不正確。");return selected;
}
struct App{
 HWND window=nullptr,tabs=nullptr,status=nullptr,list=nullptr,folder=nullptr,search=nullptr,historyList=nullptr,fileList=nullptr,taskNote=nullptr;
 HFONT font=nullptr,titleFont=nullptr,smallFont=nullptr;UINT dpi=96;int scrollX=0,scrollY=0;bool layingOut=false;
 HBRUSH background=nullptr;std::vector<std::pair<HWND,int>>children;std::map<HWND,RECT>bases;std::map<HWND,int>fontRoles;
 std::thread worker;std::atomic<bool>busy{false},preserving{false};std::atomic<bool>closing{false};bool drawing=false,taskError=false,showProgress=false,fileVerifyContents=false;
 std::set<std::string>checked;std::shared_ptr<Source>source;
 std::shared_ptr<Control>control=std::make_shared<Control>(),preserveControl=std::make_shared<Control>();
 J chats=J::array(),filtered=J::array(),shown=J::array(),history=J::array(),report,fileReport=J::object(),progressRows=J::array();
 std::map<std::string,size_t>progressIndex;fs::path config,historyRoot,fileRoot;
 std::vector<std::string> sourceOptions;size_t historyOffset=0,chatOffset=0,fileOffset=0;int64_t historyTotal=0;int page=0,completionPage=0;size_t chatPageSize=25;
};App app;int scaled(int logical){return MulDiv(logical,static_cast<int>(app.dpi),96);}
UINT windowDpi(HWND w){using Fn=UINT(WINAPI*)(HWND);auto fn=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));if(fn){auto dpi=fn(w);if(dpi)return dpi;}HDC dc=GetDC(w);auto dpi=dc?GetDeviceCaps(dc,LOGPIXELSX):96;if(dc)ReleaseDC(w,dc);return dpi>0?static_cast<UINT>(dpi):96;}
void initializeDpi(){using Fn=BOOL(WINAPI*)(HANDLE);auto fn=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext"));if(fn&&fn(reinterpret_cast<HANDLE>(-4)))return; // Manifest may have already selected PMv2/PMv1.
 if(fn&&GetLastError()==ERROR_ACCESS_DENIED)return;
 if(fn&&fn(reinterpret_cast<HANDLE>(-3)))return;
 SetProcessDPIAware();
}
RECT workArea(HWND w){MONITORINFO info{sizeof(info)};if(GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST),&info))return info.rcWork;RECT area{};SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);return area;}
void setRole(HWND w,int role){app.fontRoles[w]=role;SendMessageW(w,WM_SETFONT,reinterpret_cast<WPARAM>(role==1?app.titleFont:role==2?app.smallFont:app.font),TRUE);}
void rebuildFonts(){auto make=[](int size,int weight){return CreateFontW(-scaled(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft JhengHei UI");};HFONT normal=make(16,FW_NORMAL),title=make(26,FW_SEMIBOLD),small=make(14,FW_NORMAL);if(!normal||!title||!small){if(normal)DeleteObject(normal);if(title)DeleteObject(title);if(small)DeleteObject(small);throw Error("無法建立適合目前螢幕的字型。");}auto old=app.font,oldTitle=app.titleFont,oldSmall=app.smallFont;app.font=normal;app.titleFont=title;app.smallFont=small;for(auto [w,role]:app.fontRoles)setRole(w,role);if(old)DeleteObject(old);if(oldTitle)DeleteObject(oldTitle);if(oldSmall)DeleteObject(oldSmall);}
void layout(){
 if(!app.window||app.bases.empty()||app.layingOut)return;app.layingOut=true;
 RECT r{};GetClientRect(app.window,&r);const int W=std::max(880,MulDiv(int(r.right),96,int(app.dpi))),H=std::max(680,MulDiv(int(r.bottom),96,int(app.dpi)));
 const int width=scaled(W),height=scaled(H),m=24,cw=W-48;
 app.scrollX=std::clamp(app.scrollX,0,std::max(0,width-int(r.right)));app.scrollY=std::clamp(app.scrollY,0,std::max(0,height-int(r.bottom)));
 SCROLLINFO horizontal{sizeof(SCROLLINFO),SIF_RANGE|SIF_PAGE|SIF_POS|SIF_DISABLENOSCROLL,0,width-1,static_cast<UINT>(std::max(0L,r.right)),app.scrollX,0};
 SCROLLINFO vertical{sizeof(SCROLLINFO),SIF_RANGE|SIF_PAGE|SIF_POS|SIF_DISABLENOSCROLL,0,height-1,static_cast<UINT>(std::max(0L,r.bottom)),app.scrollY,0};
 SetScrollInfo(app.window,SB_HORZ,&horizontal,TRUE);SetScrollInfo(app.window,SB_VERT,&vertical,TRUE);
 auto place=[&](int id,int x,int y,int w,int h){auto c=GetDlgItem(app.window,id);if(c)app.bases[c]=RECT{x,y,x+w,y+h};};
 place(ID_HEADER,m,18,360,36);place(ID_SUBTITLE,m,60,cw-180,24);place(ID_INFO,W-180,22,156,30);place(ID_TABS,m,98,cw,36);
 place(ID_AUTO_CAPTURE,m,152,160,36);place(ID_CONNECT,196,152,224,36);place(ID_DEMO,432,152,148,36);place(ID_SETTINGS,W-168,152,144,36);
 place(ID_MAIN_HINT,m,198,cw,24);const int searchWidth=cw-454;
 place(ID_SEARCH,m,234,searchWidth,32);place(ID_CHAT_TYPE,m+searchWidth+10,234,130,32);place(ID_CHAT_SOURCE,m+searchWidth+150,234,204,32);place(ID_CHAT_PAGE_SIZE,W-112,234,88,32);
 place(ID_SELECT_PAGE,m,278,112,30);place(ID_CLEAR_PAGE,146,278,112,30);place(ID_SELECT_FILTERED,268,278,152,30);place(ID_CLEAR_ALL,430,278,136,30);place(ID_SELECTION_COUNT,582,283,W-606,26);
 place(ID_LIST,m,320,cw,std::max(80,H-528));place(ID_CHAT_PREV,m,H-196,112,28);place(ID_CHAT_NEXT,148,H-196,112,28);place(ID_CHAT_PAGE,276,H-191,W-300,24);
 place(ID_FOLDER_LABEL,m,H-150,100,24);place(ID_FOLDER,132,H-156,W-300,32);place(ID_BROWSE,W-156,H-156,132,32);
 place(ID_START,m,H-108,200,36);place(ID_PAUSE,236,H-108,96,36);place(ID_STOP,344,H-108,144,36);place(ID_VIEW_RESULT,500,H-108,184,36);
 if(app.status)app.bases[app.status]=RECT{m,H-62,W-m,H-14};
 // Saved messages and file results are a separate workspace, with fixed actions and page controls.
 place(ID_RESULT_TITLE,m,152,cw,28);place(ID_RESULT_HINT,m,188,cw,24);
 place(ID_OPEN,m,224,156,34);place(ID_CHECK,192,224,156,34);place(ID_REBUILD,360,224,156,34);place(ID_BACKUP,528,224,176,34);place(ID_IMPORT,716,224,std::max(140,W-740),34);
 place(ID_FILE_REFRESH,m,270,112,32);place(ID_MEDIA_IMPORT,148,270,180,32);place(ID_OPEN_DATA,340,270,160,32);place(ID_FILE_STATE,512,270,164,32);place(ID_FILE_SEARCH,688,270,std::max(144,W-712),32);
 place(ID_FILES,m,314,cw,std::max(44,H-626));place(ID_FILE_PREV,m,H-300,112,28);place(ID_FILE_NEXT,148,H-300,112,28);place(ID_FILE_PAGE,276,H-295,W-300,24);
 place(ID_TASK_NOTE,m,H-256,cw,88);place(ID_IMPORT_HINT,0,0,0,0);place(ID_DATA_HINT,0,0,0,0);
 place(ID_HISTORY_TITLE,m,152,cw,28);place(ID_HISTORY_HINT,m,188,cw,40);place(ID_HISTORY,m,236,cw,std::max(100,H-496));
 place(ID_HISTORY_REFRESH,m,H-248,156,34);place(ID_HISTORY_APPLY,192,H-248,212,34);place(ID_HISTORY_DELETE,416,H-248,176,34);place(ID_HISTORY_PREV,m,H-202,112,28);place(ID_HISTORY_NEXT,148,H-202,112,28);place(ID_HISTORY_PAGE,276,H-197,W-300,24);place(ID_HISTORY_NOTE,0,0,0,0);
 place(ID_PROBE_TITLE,m,152,cw,28);place(ID_PROBE_HINT,m,192,cw,44);place(ID_PROBE,m,252,216,36);place(ID_REPORT,252,252,216,36);place(ID_PROFILE,m,308,216,36);place(ID_UIA_CONNECT,252,308,264,36);place(ID_PROBE_NOTE,m,370,cw,100);
 place(ID_SETTINGS_TITLE,m,152,cw,30);place(ID_CONTENT_LABEL,m,204,cw,24);place(ID_MESSAGES,m,238,224,28);place(ID_ALBUMS,260,238,144,28);place(ID_NOTES,420,238,240,28);
 place(ID_RANGE_LABEL,m,294,104,28);place(ID_FROM,140,288,180,32);place(ID_RANGE_TO,332,294,24,24);place(ID_UNTIL,366,288,180,32);place(ID_TIME_HINT,564,294,W-588,26);
 place(ID_MODE_LABEL,m,350,104,28);place(ID_MESSAGE_MODE,140,344,std::min(440,W-188),32);place(ID_MODE_HINT,m,398,cw,70);
 for(auto& [w,b]:app.bases){wchar_t type[32]{};GetClassNameW(w,type,32);const int h=wcscmp(type,L"ComboBox")==0?scaled(220):scaled(b.bottom-b.top);MoveWindow(w,scaled(b.left)-app.scrollX,scaled(b.top)-app.scrollY,scaled(b.right-b.left),h,FALSE);}
 if(app.list){ListView_SetColumnWidth(app.list,0,scaled(std::max(280,cw-350)));ListView_SetColumnWidth(app.list,1,scaled(120));ListView_SetColumnWidth(app.list,2,scaled(210));}
 if(app.historyList)for(auto [index,ww]:std::vector<std::pair<int,int>>{{0,190},{1,std::max(180,cw-620)},{2,120},{3,290}})ListView_SetColumnWidth(app.historyList,index,scaled(ww));
 if(app.fileList)for(auto [index,ww]:std::vector<std::pair<int,int>>{{0,180},{1,std::max(220,cw-610)},{2,100},{3,120},{4,200}})ListView_SetColumnWidth(app.fileList,index,scaled(ww));
 RedrawWindow(app.window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);app.layingOut=false;
}
void scrollView(bool horizontal,int request){SCROLLINFO info{sizeof(SCROLLINFO),SIF_ALL};GetScrollInfo(app.window,horizontal?SB_HORZ:SB_VERT,&info);int& position=horizontal?app.scrollX:app.scrollY;switch(request){case SB_LINEUP:position-=scaled(36);break;case SB_LINEDOWN:position+=scaled(36);break;case SB_PAGEUP:position-=static_cast<int>(info.nPage);break;case SB_PAGEDOWN:position+=static_cast<int>(info.nPage);break;case SB_THUMBTRACK:case SB_THUMBPOSITION:position=info.nTrackPos;break;case SB_TOP:position=0;break;case SB_BOTTOM:position=info.nMax;break;default:return;}layout();}
void revealFocus(){HWND focused=GetFocus();while(focused&&focused!=app.window&&!app.bases.count(focused))focused=GetParent(focused);auto found=app.bases.find(focused);if(found==app.bases.end()||!IsWindowVisible(focused))return;RECT client{};GetClientRect(app.window,&client);auto b=found->second;int left=scaled(b.left),right=scaled(b.right),top=scaled(b.top),bottom=scaled(b.bottom);if(left<app.scrollX)app.scrollX=left;else if(right>app.scrollX+client.right)app.scrollX=right-client.right;if(top<app.scrollY)app.scrollY=top;else if(bottom>app.scrollY+client.bottom)app.scrollY=bottom-client.bottom;layout();}
std::wstring getText(HWND w){int n=GetWindowTextLengthW(w);std::wstring s(n+1,L'\0');GetWindowTextW(w,s.data(),n+1);s.resize(n);return s;}
void setStatus(std::string s){SetWindowTextW(app.status,wide(s).c_str());}
void postStatus(std::string s){auto p=new std::wstring(wide(s));if(!PostMessageW(app.window,MSG_STATUS,0,reinterpret_cast<LPARAM>(p)))delete p;}
HWND child(const wchar_t*type,const wchar_t*label,DWORD style,int id,int page,int x,int y,int w,int h){HWND c=CreateWindowExW(0,type,label,WS_CHILD|WS_VISIBLE|style|(wcscmp(type,L"STATIC")?WS_TABSTOP:0),x,y,w,h,app.window,reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),GetModuleHandleW(nullptr),nullptr);setRole(c,0);app.children.push_back({c,page});app.bases[c]=RECT{x,y,x+w,y+h};return c;}
void activatePage(int p){app.page=p;for(auto[c,page]:app.children)ShowWindow(c,page<0||page==p?SW_SHOW:SW_HIDE);RedrawWindow(app.window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);}
bool nativeDatabaseConnection(const std::shared_ptr<Source>& source){return bool(std::dynamic_pointer_cast<LineDatabaseSource>(source))||bool(std::dynamic_pointer_cast<LineBatchSource>(source));}
void drawChats();void drawFileRows();
void taskControls(bool busy,bool stoppable=false){for(int id:{ID_CONNECT,ID_DEMO,ID_START,ID_BROWSE,ID_PROFILE,ID_PROBE,ID_REPORT,ID_OPEN,ID_CHECK,ID_REBUILD,ID_BACKUP,ID_LIST,ID_SEARCH,ID_FOLDER,ID_MESSAGES,ID_ALBUMS,ID_NOTES,ID_FROM,ID_UNTIL,ID_MESSAGE_MODE,ID_HISTORY_REFRESH,ID_HISTORY_APPLY,ID_HISTORY_DELETE,ID_HISTORY,ID_IMPORT,ID_HISTORY_PREV,ID_HISTORY_NEXT,ID_OPEN_DATA,ID_LINE_DATABASE,ID_MEDIA_IMPORT,ID_UIA_CONNECT,ID_AUTO_CAPTURE,ID_CHAT_TYPE,ID_CHAT_SOURCE,ID_CHAT_PAGE_SIZE,ID_CHAT_PREV,ID_CHAT_NEXT,ID_SELECT_PAGE,ID_CLEAR_PAGE,ID_SELECT_FILTERED,ID_CLEAR_ALL,ID_SETTINGS,ID_FILE_REFRESH,ID_VIEW_RESULT})EnableWindow(GetDlgItem(app.window,id),!busy);if(nativeDatabaseConnection(app.source)){for(int id:{ID_ALBUMS,ID_NOTES})EnableWindow(GetDlgItem(app.window,id),FALSE);}for(int id:{ID_FILE_STATE,ID_FILE_SEARCH})EnableWindow(GetDlgItem(app.window,id),!busy||app.showProgress);EnableWindow(GetDlgItem(app.window,ID_PAUSE),busy&&stoppable);EnableWindow(GetDlgItem(app.window,ID_STOP),busy&&stoppable);if(!busy){EnableWindow(GetDlgItem(app.window,ID_START),!app.checked.empty());drawChats();drawFileRows();}}
void startTask(std::function<std::string()>fn,bool stoppable=false){if(app.busy.exchange(true))return;try{app.control=std::make_shared<Control>();app.preserveControl=std::make_shared<Control>();app.preserving=false;app.completionPage=app.page;taskControls(true,stoppable);setStatus("正在處理，請稍候…");app.worker=std::thread([fn=std::move(fn)]{CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);auto r=new Result;try{r->message=fn();r->stopped=app.control->stopped.load();}catch(const Cancelled&e){r->message=std::string(e.what())+" 已保存的訊息與原檔仍保留。";r->stopped=true;}catch(const Error&e){r->message=e.what();r->error=true;}catch(...){r->message="操作尚未完成，已保存的內容仍會保留。請檢查保存位置或重新連接 LINE。";r->error=true;}CoUninitialize();if(!PostMessageW(app.window,MSG_DONE,0,reinterpret_cast<LPARAM>(r)))delete r;});}catch(...){app.busy=false;taskControls(false);throw;}}
fs::path folderPath(bool existing=false){auto raw=getText(app.folder);if(raw.empty())throw Error("請先選擇聊天紀錄資料夾。");fs::path p(raw);if(existing&&!fs::is_regular_file(p/L"archive.sqlite3"))throw Error("這個資料夾還沒有可開啟的聊天紀錄。");return p;}
void rememberArchive(const fs::path& root){atomicText(app.config/L"preferences.local.json",safeJson(archivePreference(root)));}
struct OpenedArchive{std::unique_ptr<Store> store;std::string preferenceWarning;Store& operator*()const{return *store;}Store* operator->()const{return store.get();}};
OpenedArchive openRememberedArchive(const fs::path& root,Tick tick={},bool allowUpgrade=false,const std::function<void(const fs::path&)>& remember=rememberArchive){OpenedArchive opened{std::make_unique<Store>(root,tick,allowUpgrade),{}};try{remember(root);}catch(...){opened.preferenceWarning="偏好位置未能記住，聊天紀錄仍已開啟；下次請重新選取："+utf8(root)+"\n";}return opened;}
bool confirmArchiveUpgrade(){auto message=L"這份聊天紀錄使用舊格式。開啟管理前需要升級為新版格式。\n\n升級前會先在私人聊天紀錄內建立獨立的資料庫還原副本，保留原有對話與檔案。升級後不能再用 alpha.4 或更舊版本管理這份聊天紀錄。\n\n是否先建立還原副本並升級？";return MessageBoxW(app.window,message,L"確認升級聊天紀錄格式",MB_OKCANCEL|MB_ICONWARNING)==IDOK;}
std::optional<bool> approveArchiveOpen(const fs::path& root,const std::function<bool()>& confirm=confirmArchiveUpgrade){if(archiveSchemaVersion(root)!=1)return false;if(!confirm())return std::nullopt;return true;}
std::string storeNotice(const Store& store){auto notice=store.db.protectionNotice();if(!store.upgradeSnapshot.empty())notice+="聊天紀錄已升級，升級前的資料庫還原副本："+utf8(store.upgradeSnapshot)+"\n";return notice;}
std::string storeNotice(const OpenedArchive& opened){return storeNotice(*opened)+opened.preferenceWarning;}
fs::path chooseFolder(const wchar_t* title){IFileDialog*dialog=nullptr;if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_IFileDialog,reinterpret_cast<void**>(&dialog))))throw Error("無法開啟資料夾選擇畫面。");struct Release{IFileDialog* p;~Release(){p->Release();}} release{dialog};DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR);dialog->SetTitle(title);fs::path selected;if(SUCCEEDED(dialog->Show(app.window))){IShellItem*item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){selected=path;CoTaskMemFree(path);}item->Release();}}return selected;}
void browse(){if(app.busy)return;auto path=chooseFolder(app.page==1||app.page==3?L"選擇已有的聊天紀錄資料夾":L"選擇保存位置（新聊天紀錄會建立私人子資料夾）");if(!path.empty())SetWindowTextW(app.folder,path.c_str());}
fs::path chooseFile(bool save,const wchar_t*title,const wchar_t*filter,const wchar_t*name,const fs::path& initialDirectory={},HWND owner=nullptr){wchar_t path[32768]{};if(name)wcsncpy_s(path,name,_TRUNCATE);OPENFILENAMEW of{};of.lStructSize=sizeof of;of.hwndOwner=owner?owner:app.window;of.lpstrFile=path;of.nMaxFile=32768;of.lpstrTitle=title;of.lpstrFilter=filter;of.lpstrInitialDir=initialDirectory.empty()?nullptr:initialDirectory.c_str();of.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_DONTADDTORECENT|(save?0:OFN_FILEMUSTEXIST);if(save?GetSaveFileNameW(&of):GetOpenFileNameW(&of))return fs::path(path);return {};}
void openPage(const fs::path&p){auto result=reinterpret_cast<intptr_t>(ShellExecuteW(nullptr,L"open",p.c_str(),nullptr,parentDirectoryPath(p).c_str(),SW_SHOWNORMAL));if(result<=32)throw Error("無法開啟閱讀頁面，請在聊天紀錄資料夾按兩下 index.html。");}
intptr_t exploreFolder(const fs::path& path){return reinterpret_cast<intptr_t>(ShellExecuteW(app.window,L"explore",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL));}
void openProgramData(const std::function<intptr_t(const fs::path&)>& explore=exploreFolder){noLinks(app.config);if(!fs::is_directory(app.config))throw Error("程式資料夾已不存在，請重新開啟程式。");if(explore(app.config)<=32)throw Error("無法開啟程式資料夾，請在 ChatAtlas.exe 旁開啟 ChatAtlasData。");}
std::string openPublishedReader(const fs::path& page,const Control& control,const std::function<void(const fs::path&)>& open=openPage){if(!control.stopped.load()){open(page);return {};}return "\n閱讀頁面已保存；已依停止要求略過自動開啟："+utf8(page)+"\n";}
bool displayIdentifier(const std::string& value){
 if(value.size()<20)return false;size_t start=(value[0]=='u'||value[0]=='c'||value[0]=='r'||value[0]=='s')?1:0;
 return std::all_of(value.begin()+start,value.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');});
}
std::string chatDisplayName(const J& chat){auto value=chat.value("title","");return value.empty()||displayIdentifier(value)?"名稱待確認":value;}
std::string chatSourceLabel(const J& chat){auto label=chat.value("source_label","");if(label.empty())return "目前來源";auto divider=label.find(" · ");return divider==std::string::npos?label:label.substr(0,divider);}
void refreshChatSources(){
 auto combo=GetDlgItem(app.window,ID_CHAT_SOURCE);if(!combo)return;auto previous=getText(combo);app.sourceOptions.clear();std::set<std::string> values;
 for(const auto& chat:app.chats)values.insert(chatSourceLabel(chat));SendMessageW(combo,CB_RESETCONTENT,0,0);SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"全部來源"));
 int selected=0;for(const auto& value:values){app.sourceOptions.push_back(value);auto label=wide(value);auto row=SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));if(label==previous)selected=static_cast<int>(row);}SendMessageW(combo,CB_SETCURSEL,selected,0);
}
std::string selectedChatKind(){auto index=SendMessageW(GetDlgItem(app.window,ID_CHAT_TYPE),CB_GETCURSEL,0,0);return index==1?"direct":index==2?"group":index==3?"community":index==4?"subchat":"";}
void drawChats(){
 if(!app.list)return;app.drawing=true;ListView_DeleteAllItems(app.list);app.filtered=J::array();app.shown=J::array();
 auto query=narrow(getText(app.search)),kind=selectedChatKind();auto sourceIndex=SendMessageW(GetDlgItem(app.window,ID_CHAT_SOURCE),CB_GETCURSEL,0,0);
 auto selectedSource=sourceIndex>0&&static_cast<size_t>(sourceIndex)<=app.sourceOptions.size()?app.sourceOptions[static_cast<size_t>(sourceIndex-1)]:std::string();
 for(const auto& chat:app.chats){auto name=chatDisplayName(chat),source=chatSourceLabel(chat);if(!kind.empty()&&chat.value("kind","")!=kind)continue;if(!selectedSource.empty()&&source!=selectedSource)continue;if(!query.empty()&&name.find(query)==std::string::npos&&source.find(query)==std::string::npos)continue;app.filtered.push_back(chat);}
 auto sizeIndex=SendMessageW(GetDlgItem(app.window,ID_CHAT_PAGE_SIZE),CB_GETCURSEL,0,0);app.chatPageSize=sizeIndex==1?50:sizeIndex==2?100:25;
 if(app.chatOffset>=app.filtered.size())app.chatOffset=app.filtered.empty()?0:((app.filtered.size()-1)/app.chatPageSize)*app.chatPageSize;
 const auto until=std::min(app.filtered.size(),app.chatOffset+app.chatPageSize);
 for(size_t index=app.chatOffset;index<until;++index){const auto& chat=app.filtered[index];app.shown.push_back(chat);auto title=wide(chatDisplayName(chat));LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=ListView_GetItemCount(app.list);item.pszText=title.data();int row=ListView_InsertItem(app.list,&item);auto type=chat.value("kind","group");auto label=type=="direct"?L"個人對話":type=="community"?L"社群":type=="subchat"?L"社群聊天室":L"群組";ListView_SetItemText(app.list,row,1,const_cast<wchar_t*>(label));auto source=wide(chatSourceLabel(chat));ListView_SetItemText(app.list,row,2,source.data());ListView_SetCheckState(app.list,row,app.checked.count(chat.at("source_id").get<std::string>())?TRUE:FALSE);}
 const auto pages=std::max<size_t>(1,(app.filtered.size()+app.chatPageSize-1)/app.chatPageSize);auto pageText=wide("第 "+std::to_string(app.chatOffset/app.chatPageSize+1)+" / "+std::to_string(pages)+" 頁 · "+std::to_string(app.filtered.size())+" 個聊天室");SetWindowTextW(GetDlgItem(app.window,ID_CHAT_PAGE),pageText.c_str());
 auto count=wide("已選 "+std::to_string(app.checked.size())+" 個");SetWindowTextW(GetDlgItem(app.window,ID_SELECTION_COUNT),count.c_str());
 EnableWindow(GetDlgItem(app.window,ID_CHAT_PREV),!app.busy&&app.chatOffset>0);EnableWindow(GetDlgItem(app.window,ID_CHAT_NEXT),!app.busy&&until<app.filtered.size());
 EnableWindow(GetDlgItem(app.window,ID_START),!app.busy&&!app.checked.empty());app.drawing=false;
}
void chatSelectionCommand(int id){
 if(app.busy)return;
 if(id==ID_CHAT_PREV)app.chatOffset=app.chatOffset>=app.chatPageSize?app.chatOffset-app.chatPageSize:0;
 else if(id==ID_CHAT_NEXT){if(app.chatOffset+app.chatPageSize<app.filtered.size())app.chatOffset+=app.chatPageSize;}
 else if(id==ID_CLEAR_ALL)app.checked.clear();
 else if(id==ID_SELECT_PAGE||id==ID_CLEAR_PAGE){for(const auto& chat:app.shown){auto key=chat.at("source_id").get<std::string>();if(id==ID_SELECT_PAGE)app.checked.insert(key);else app.checked.erase(key);}}
 else if(id==ID_SELECT_FILTERED){for(const auto& chat:app.filtered)app.checked.insert(chat.at("source_id").get<std::string>());}
 else app.chatOffset=0;
 drawChats();
}
void clearConnection(){app.source.reset();app.chats=J::array();app.checked.clear();app.chatOffset=0;refreshChatSources();drawChats();}
std::string dateRange(int id,bool following){SYSTEMTIME st{};if(SendMessageW(GetDlgItem(app.window,id),DTM_GETSYSTEMTIME,0,reinterpret_cast<LPARAM>(&st))!=GDT_VALID)return "";if(following){FILETIME ft{};SYSTEMTIME midnight{};midnight.wYear=st.wYear;midnight.wMonth=st.wMonth;midnight.wDay=st.wDay;if(!SystemTimeToFileTime(&midnight,&ft))throw Error("日期範圍不正確。");ULARGE_INTEGER value{};value.LowPart=ft.dwLowDateTime;value.HighPart=ft.dwHighDateTime;value.QuadPart+=864000000000ULL;ft.dwLowDateTime=value.LowPart;ft.dwHighDateTime=value.HighPart;FileTimeToSystemTime(&ft,&st);}char out[40];std::snprintf(out,sizeof out,"%04u-%02u-%02uT00:00:00+08:00",st.wYear,st.wMonth,st.wDay);return normalizeTime(out);}
std::optional<fs::path> newCollectionPath(fs::path root,const std::function<bool(const fs::path&)>& confirm){
 requireAclVolume(root);noLinks(root);
 if(fs::is_regular_file(root/L"archive.sqlite3"))return root;
 if(!fs::exists(root))return root;
 if(!fs::is_directory(root))throw Error("請選擇資料夾，不是檔案。");
 auto proposed=root/L"ChatAtlas-Collection";unsigned suffix=2;
 while(fs::exists(proposed)){if(suffix>999)throw Error("這個位置已有太多同名資料夾，請選擇其他保存位置。");proposed=root/(L"ChatAtlas-Collection-"+std::to_wstring(suffix++));}
 if(!confirm(proposed))return std::nullopt;
 noLinks(proposed);requireLocalPrivatePath(proposed);PrivateSecurity security;
 if(!CreateDirectoryW(proposed.c_str(),&security.attributes))throw Error("新聊天紀錄位置已被使用或無法建立，請重新選擇保存位置。");
 requirePrivateDirectory(proposed);return proposed;
}

std::string historyStatus(const std::string& value){for(auto [key,label]:std::vector<std::pair<std::string,std::string>>{{"complete","完成"},{"complete_with_issues","待補存"},{"stopped","已停止"},{"interrupted","中途結束"},{"failed","未完成"},{"running","進行中"}})if(value==key)return label;return "待確認";}
// Display only: stored timestamps and capture ranges remain UTC.
SYSTEMTIME taiwanTime(const std::string& value){auto normalized=normalizeTime(value);unsigned y,m,d,h,minute,second;if(std::sscanf(normalized.c_str(),"%u-%u-%uT%u:%u:%u",&y,&m,&d,&h,&minute,&second)!=6)throw Error("時間格式不正確。");SYSTEMTIME utc{};utc.wYear=y;utc.wMonth=m;utc.wDay=d;utc.wHour=h;utc.wMinute=minute;utc.wSecond=second;FILETIME ft{};if(!SystemTimeToFileTime(&utc,&ft))throw Error("時間格式不正確。");ULARGE_INTEGER ticks{};ticks.LowPart=ft.dwLowDateTime;ticks.HighPart=ft.dwHighDateTime;ticks.QuadPart+=8ULL*3600*10000000;ft.dwLowDateTime=ticks.LowPart;ft.dwHighDateTime=ticks.HighPart;SYSTEMTIME local{};if(!FileTimeToSystemTime(&ft,&local))throw Error("時間格式不正確。");return local;}
std::string historyStarted(const std::string& value){if(value.empty())return "未記錄";try{auto time=taiwanTime(value);char out[32];std::snprintf(out,sizeof out,"%04u-%02u-%02u %02u:%02u:%02u",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);return out;}catch(const Error&){return "時間無法辨識";}}
void drawHistory(){auto pages=std::max<int64_t>(1,(app.historyTotal+99)/100);SetWindowTextW(GetDlgItem(app.window,ID_HISTORY_PAGE),wide("第 "+std::to_string(app.historyOffset/100+1)+" / "+std::to_string(pages)+" 頁，共 "+std::to_string(app.historyTotal)+" 筆").c_str());ListView_DeleteAllItems(app.historyList);for(auto& row:app.history){auto time=row.value("started_at","");std::wstring started=wide(historyStarted(time));LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=ListView_GetItemCount(app.historyList);item.pszText=started.data();int index=ListView_InsertItem(app.historyList,&item);auto counts=row.value("counts",J::object());auto count=[&](const char* key){return counts.contains(key)&&counts[key].is_number_integer()?counts[key].get<int64_t>():0;};std::vector<std::wstring> values={wide(chatDisplayName(J{{"title",row.value("chat_title",std::string())}})),wide(historyStatus(row.value("status",""))),wide("內容 "+std::to_string(count("items"))+" · 沿用 "+std::to_string(count("reused"))+" · 新增 "+std::to_string(count("downloaded"))+" · 待補 "+std::to_string(count("failed")+count("unavailable")))};for(int column=0;column<3;column++)ListView_SetItemText(app.historyList,index,column+1,values[column].data());}}
void postHistory(Store& store,const fs::path& root,size_t offset=0){auto total=store.historyCount();if(offset>=static_cast<size_t>(std::max<int64_t>(0,total)))offset=total?((static_cast<size_t>(total)-1)/100)*100:0;auto rows=store.history(100,offset);auto payload=std::make_unique<HistoryResult>(HistoryResult{root,std::move(rows),offset,total});if(!PostMessageW(app.window,MSG_HISTORY,0,reinterpret_cast<LPARAM>(payload.get())))throw Error("採集紀錄無法顯示，請重新讀取。");payload.release();}
J selectedHistory(){int index=ListView_GetNextItem(app.historyList,-1,LVNI_SELECTED);if(index<0||static_cast<size_t>(index)>=app.history.size())throw Error("請先選取一筆採集紀錄。");return app.history[index];}
std::optional<SYSTEMTIME> historyDate(const std::string& value,bool following){if(value.empty())return std::nullopt;auto normalized=normalizeTime(value);unsigned y,m,d,h,minute,second;if(std::sscanf(normalized.c_str(),"%u-%u-%uT%u:%u:%u",&y,&m,&d,&h,&minute,&second)!=6)throw Error("這筆紀錄的期間無法套用，請手動設定。");SYSTEMTIME utc{};utc.wYear=y;utc.wMonth=m;utc.wDay=d;utc.wHour=h;utc.wMinute=minute;utc.wSecond=second;FILETIME ft{};if(!SystemTimeToFileTime(&utc,&ft))throw Error("這筆紀錄的期間無法套用。");ULARGE_INTEGER ticks{};ticks.LowPart=ft.dwLowDateTime;ticks.HighPart=ft.dwHighDateTime;ticks.QuadPart+=8ULL*3600*10000000;if(following)ticks.QuadPart-=24ULL*3600*10000000;ft.dwLowDateTime=ticks.LowPart;ft.dwHighDateTime=ticks.HighPart;SYSTEMTIME local{};if(!FileTimeToSystemTime(&ft,&local)||local.wHour||local.wMinute||local.wSecond||normalized.find(".000+")==std::string::npos)throw Error("這筆紀錄不是整日範圍，請手動設定期間後再整理。");return local;}
void applyHistory(const J& row){if(!app.source)throw Error("請先連接同一個 LINE 帳號，再套用採集紀錄。");if(row.value("status","")=="running")throw Error("這筆作業仍在執行，請完成後再使用。");auto sourceId=row.value("source_id","");auto found=std::find_if(app.chats.begin(),app.chats.end(),[&](const J& chat){auto stored=app.source->storageChat(chat);return app.source->accountFor(chat)==row.value("account","")&&stored.value("source_id","")==sourceId&&stored.value("kind","")==row.value("chat_kind","")&&stored.value("parent_source_id",J(nullptr))==row.value("parent_source_id",J(nullptr));});if(found==app.chats.end())throw Error("目前連線找不到相同帳號與聊天室，未變更整理設定。");auto options=row.value("options",J());if(!options.is_object()||options.value("version",0)!=1||!options.contains("kinds")||!options["kinds"].is_array())throw Error("這筆舊紀錄沒有完整整理設定，請手動選擇內容。");std::set<std::string> kinds;for(auto& value:options["kinds"]){if(!value.is_string()||!allKinds.count(value.get<std::string>()))throw Error("這筆紀錄的內容設定不完整，請手動選擇。");kinds.insert(value.get<std::string>());}if(kinds.empty())throw Error("這筆紀錄沒有可套用的內容設定。");for(auto group:std::vector<std::set<std::string>>{{"message","thread"},{"album","album_item"},{"note","comment"}}){size_t count=0;for(auto& kind:group)count+=kinds.count(kind);if(count&&count!=group.size())throw Error("這筆紀錄含個別內容類型，請手動選擇整理內容。");}auto mode=options.value("message_mode","");if(mode!="update"&&mode!="skip_text")throw Error("這筆紀錄的文字設定無法套用。");auto since=options.value("since",""),until=options.value("until","");if(!since.empty()&&!until.empty()&&normalizeTime(since)>=normalizeTime(until))throw Error("這筆紀錄的日期範圍不正確。");auto from=historyDate(since,false),to=historyDate(until,true);
 // Validate the complete record before changing any visible selection.
 app.checked={found->at("source_id").get<std::string>()};SetWindowTextW(app.search,L"");for(int id:{ID_CHAT_TYPE,ID_CHAT_SOURCE})SendMessageW(GetDlgItem(app.window,id),CB_SETCURSEL,0,0);app.chatOffset=static_cast<size_t>(std::distance(app.chats.begin(),found))/app.chatPageSize*app.chatPageSize;drawChats();for(auto [id,kind]:std::vector<std::pair<int,const char*>>{{ID_MESSAGES,"message"},{ID_ALBUMS,"album"},{ID_NOTES,"note"}})SendMessageW(GetDlgItem(app.window,id),BM_SETCHECK,kinds.count(kind)?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(GetDlgItem(app.window,ID_MESSAGE_MODE),CB_SETCURSEL,mode=="skip_text"?1:0,0);SendMessageW(GetDlgItem(app.window,ID_FROM),DTM_SETSYSTEMTIME,from?GDT_VALID:GDT_NONE,from?reinterpret_cast<LPARAM>(&*from):0);SendMessageW(GetDlgItem(app.window,ID_UNTIL),DTM_SETSYSTEMTIME,to?GDT_VALID:GDT_NONE,to?reinterpret_cast<LPARAM>(&*to):0);SetWindowTextW(app.folder,app.historyRoot.c_str());TabCtrl_SetCurSel(app.tabs,0);activatePage(0);setStatus("已套用設定，請確認後按「保存選取的聊天室」。會重新讀取來源並沿用已驗證的檔案，不會自動開始。");}
bool confirmHistoryDelete(){return MessageBoxW(app.window,L"只刪除選取的採集紀錄。\n\n不會刪除對話、檔案或檔案沿用資料。\n這筆紀錄的日期與整理設定將無法從清單恢復。是否繼續？",L"刪除採集紀錄",MB_OKCANCEL|MB_ICONWARNING)==IDOK;}
std::string deleteHistoryAndRefresh(Store& store,int64_t jobId,const std::function<void()>& refresh){store.deleteHistory(jobId);std::string message="已刪除這筆紀錄，對話、檔案與沿用資料仍保留。";try{refresh();}catch(...){message+="\n紀錄列表未能更新，請重新讀取採集紀錄；不用再次刪除。";}return message;}
void historyCommand(int id,const std::function<bool()>& confirmDelete=confirmHistoryDelete){if(id==ID_HISTORY_APPLY){applyHistory(selectedHistory());return;}if(id==ID_HISTORY_REFRESH||id==ID_HISTORY_PREV||id==ID_HISTORY_NEXT){auto root=id==ID_HISTORY_REFRESH?folderPath(true):app.historyRoot;if(root.empty())throw Error("請先讀取這份聊天紀錄的紀錄。");auto offset=id==ID_HISTORY_PREV?(app.historyOffset>=100?app.historyOffset-100:0):id==ID_HISTORY_NEXT?app.historyOffset+100:0;auto upgrade=approveArchiveOpen(root);if(!upgrade)return;startTask([root,offset,allowUpgrade=*upgrade]{auto store=openRememberedArchive(root,[&]{app.control->check();},allowUpgrade);postHistory(*store,root,offset);return storeNotice(store)+"已讀取採集紀錄。可用上一頁、下一頁查看所有保留紀錄。";},true);return;}auto row=selectedHistory();auto root=app.historyRoot;if(root.empty())throw Error("請先讀取這份聊天紀錄的紀錄。");if(row.value("status","")=="running")throw Error("正在執行的紀錄不能刪除。");if(!confirmDelete())return;auto upgrade=approveArchiveOpen(root);if(!upgrade)return;auto jobId=row.at("id").get<int64_t>();auto offset=app.historyOffset;startTask([root,jobId,offset,allowUpgrade=*upgrade]{auto store=openRememberedArchive(root,[&]{app.control->check();},allowUpgrade);auto message=deleteHistoryAndRefresh(*store,jobId,[&]{postHistory(*store,root,offset);});return storeNotice(store)+message;},true);}
bool confirmNewCollection(const fs::path& path){auto message=L"將建立新的私人聊天紀錄資料夾：\n\n"+path.wstring()+L"\n\n不會修改原資料夾或其他檔案的權限。是否繼續？";return MessageBoxW(app.window,message.c_str(),L"建立私人聊天紀錄",MB_OKCANCEL|MB_ICONINFORMATION)==IDOK;}
std::vector<std::string> storedChatIds(Source& source,const J& chats){std::vector<std::string> ids;for(const auto& chat:chats){auto stored=source.storageChat(chat);ids.push_back(key({"chat",source.accountFor(chat),stored.at("source_id").get<std::string>()}));}return ids;}
void postCaptureProgress(const CaptureProgress& value){auto event=new CaptureProgress(value);if(!PostMessageW(app.window,MSG_PROGRESS,0,reinterpret_cast<LPARAM>(event)))delete event;}
std::string fileKindLabel(const std::string& kind){return kind=="image"?"圖片":kind=="video"?"影片":kind=="audio"?"語音":"檔案";}
std::string fileStatusLabel(const std::string& status){
 if(status=="skipped")return "略過預覽";if(status=="saved")return "原檔已保存";if(status=="reused")return "沿用已保存原檔";if(status=="preview")return "僅有預覽";if(status=="unavailable")return "目前無法取得";if(status=="failed")return "保存失敗";if(status=="stopped")return "已停止";if(status=="running")return "正在保存";return "尚未保存";
}
std::string readableFileName(const J& row){
 auto name=row.value("name",row.value("file_name",std::string()));bool encoded=displayIdentifier(name);
 for(const auto* prefix:{"image-","video-","audio-","file-"})if(name.rfind(prefix,0)==0&&name.size()>18){auto stem=utf8(path8(name).stem());auto suffix=stem.substr(std::char_traits<char>::length(prefix));if(suffix.size()>12&&std::all_of(suffix.begin(),suffix.end(),[](unsigned char c){return std::isalnum(c)||c=='-'||c=='_';}))encoded=true;}
 if(name.empty()||encoded){name=fileKindLabel(row.value("kind","file"));auto sent=row.value("sent_at","");if(!sent.empty())name+=" · "+historyStarted(sent);}
 return name;
}
std::string reportState(){auto value=SendMessageW(GetDlgItem(app.window,ID_FILE_STATE),CB_GETCURSEL,0,0);return value==1?"saved":value==2?"preview":value==3?"pending":value==4?"unavailable":value==5?"failed":"all";}
void drawFileRows(){
 if(!app.fileList)return;ListView_DeleteAllItems(app.fileList);const auto filter=reportState(),query=narrow(getText(GetDlgItem(app.window,ID_FILE_SEARCH)));
 J rows=app.showProgress?app.progressRows:app.fileReport.value("files",J::array());size_t displayed=0;
 for(const auto& row:rows){auto status=row.value("status","pending"),name=readableFileName(row),chat=row.value("chat_title","");if(displayIdentifier(chat)||chat.empty())chat="名稱待確認";
  if(app.showProgress&&filter!="all"&&status!=filter&&!(filter=="saved"&&status=="reused"))continue;if(app.showProgress&&!query.empty()&&name.find(query)==std::string::npos&&chat.find(query)==std::string::npos)continue;
  auto title=wide(chat);LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=ListView_GetItemCount(app.fileList);item.pszText=title.data();auto index=ListView_InsertItem(app.fileList,&item);
  std::vector<std::wstring> fields={wide(name),wide(fileKindLabel(row.value("kind","file"))),wide(fileStatusLabel(status)),wide(row.value("reason",std::string()))};for(int column=0;column<4;++column)ListView_SetItemText(app.fileList,index,column+1,fields[static_cast<size_t>(column)].data());++displayed;
 }
 if(app.showProgress){auto label=wide("目前工作 · "+std::to_string(displayed)+" 個已出現的檔案");SetWindowTextW(GetDlgItem(app.window,ID_FILE_PAGE),label.c_str());EnableWindow(GetDlgItem(app.window,ID_FILE_PREV),FALSE);EnableWindow(GetDlgItem(app.window,ID_FILE_NEXT),FALSE);return;}
 const auto total=app.fileReport.value("total",size_t(0)),limit=app.fileReport.value("limit",size_t(50));auto pages=std::max<size_t>(1,(total+limit-1)/limit);
 auto page=wide("第 "+std::to_string(app.fileOffset/limit+1)+" / "+std::to_string(pages)+" 頁 · "+std::to_string(total)+" 個檔案 · "+(app.fileVerifyContents?"已核對檔案內容":"已核對本機檔案"));SetWindowTextW(GetDlgItem(app.window,ID_FILE_PAGE),page.c_str());EnableWindow(GetDlgItem(app.window,ID_FILE_PREV),!app.busy&&app.fileOffset>0);EnableWindow(GetDlgItem(app.window,ID_FILE_NEXT),!app.busy&&app.fileOffset+limit<total);
}
void resetCaptureResults(){KillTimer(app.window,2);SendMessageW(GetDlgItem(app.window,ID_FILE_STATE),CB_SETCURSEL,0,0);SetWindowTextW(GetDlgItem(app.window,ID_FILE_SEARCH),L"");KillTimer(app.window,2);app.taskError=false;app.showProgress=true;app.progressRows=J::array();app.progressIndex.clear();app.fileOffset=0;SetWindowTextW(app.taskNote,L"正在保存訊息。完成的原檔會逐筆保留，這裡會顯示目前檔名與結果。");TabCtrl_SetCurSel(app.tabs,1);activatePage(1);drawFileRows();}
void receiveCaptureProgress(const CaptureProgress& event){
 app.showProgress=true;
 auto current=chatDisplayName(J{{"title",event.chat_title}});std::string note="聊天室 "+std::to_string(event.chat_index)+" / "+std::to_string(event.chat_total)+" · "+current;
 if(!event.file_id.empty())note+="\n"+fileStatusLabel(event.phase=="file"&&event.status=="pending"?"running":event.status)+"："+readableFileName(J{{"name",event.file_name},{"kind",event.kind}})+" · "+std::to_string(event.file_index)+" / "+std::to_string(event.file_total);
 if(!event.reason.empty())note+="\n"+event.reason;SetWindowTextW(app.taskNote,wide(note).c_str());
 if(!event.file_id.empty()){
  if(event.status=="skipped")return;auto identity=key({event.chat_id,event.file_id,event.variant});auto found=app.progressIndex.find(identity);J row={{"id",event.file_id},{"chat_title",event.chat_title},{"name",event.file_name},{"kind",event.kind},{"variant",event.variant},{"status",event.status},{"reason",event.reason}};
  if(event.phase=="file"&&event.status=="pending")row["status"]="running";
  if(found==app.progressIndex.end()){app.progressIndex.emplace(identity,app.progressRows.size());app.progressRows.push_back(std::move(row));}else app.progressRows[found->second]=std::move(row);
  drawFileRows();
 }
}
void postFileReport(Store& store,const fs::path& root,size_t offset=0,const std::string& state="all",const std::string& query="",Tick tick={},bool verifyContents=false){
 auto report=captureReport(store,50,offset,state,query,tick,verifyContents);auto pending=std::make_unique<FileReportResult>(FileReportResult{root,std::move(report),state,query});if(!PostMessageW(app.window,MSG_FILE_REPORT,verifyContents?1:0,reinterpret_cast<LPARAM>(pending.get())))throw Error("檔案結果無法顯示，請重新檢查。");pending.release();
}
std::string checkIssueDetails(Store& store,const J& issues,Tick tick){
 std::string result;size_t shown=0;for(const auto& issue:issues){if(tick)tick();if(shown++>=12)break;auto id=issue.value("id",std::string());J matches=J::array();
  if(!id.empty())matches=store.db.rows("SELECT a.name,a.kind,i.sent_at,c.title AS chat_title FROM attachments a JOIN chats c ON c.id=a.chat_id LEFT JOIN items i ON i.id=a.item_id WHERE a.id=? OR a.blob_id=? ORDER BY a.id LIMIT 1",{id,id},tick);
  std::string label="訊息紀錄";if(!matches.empty()){const auto& file=matches[0];auto chat=chatDisplayName(J{{"title",file.value("chat_title",std::string())}});label=chat+" · "+readableFileName(file);}
  auto detail=issue.value("detail",std::string("請查看檔案狀態。"));for(const auto& [from,to]:std::vector<std::pair<std::string,std::string>>{{"收藏","聊天紀錄"},{"附件","檔案"}}){size_t pos=0;while((pos=detail.find(from,pos))!=std::string::npos){detail.replace(pos,from.size(),to);pos+=to.size();}}
  result+="\n"+label+"："+detail;
 }
 if(issues.size()>12)result+="\n另有 "+std::to_string(issues.size()-12)+" 個項目；請用表格翻頁查看每個檔案。";return result;
}
void fileReportClicked(bool verifyContents=false,size_t offset=0){
 if(app.busy){drawFileRows();return;}auto root=folderPath(true);auto upgrade=approveArchiveOpen(root);if(!upgrade)return;auto state=reportState(),query=narrow(getText(GetDlgItem(app.window,ID_FILE_SEARCH)));
 startTask([root,offset,state,query,verifyContents,allowUpgrade=*upgrade]{auto tick=[&]{app.control->check();};auto store=openRememberedArchive(root,tick,allowUpgrade);J issues=J::array();if(verifyContents){postStatus("正在核對已保存的檔案內容，可隨時停止…");issues=store->check(tick);}postFileReport(*store,root,offset,state,query,tick,verifyContents);auto details=verifyContents?checkIssueDetails(*store,issues,tick):std::string();return storeNotice(store)+(verifyContents?"完整檢查完成，需留意 "+std::to_string(issues.size())+" 個項目。下表逐檔列出目前狀態，可依檢查結果篩選。":"已列出檔案的目前可用情況與具體原因。")+details+"\n儲存位置："+utf8(root);},true);
 TabCtrl_SetCurSel(app.tabs,1);activatePage(1);
}
std::string preserveStoppedCapture(Store& store,const fs::path& root){
 app.preserving=true;PostMessageW(app.window,MSG_PRESERVING,0,0);auto tick=[&]{app.preserveControl->check();};
 try{
  tick();postStatus("抓取已停止，正在整理已保存訊息的閱讀頁…");std::string warning;auto page=exportHtml(store,tick,&warning,false);postFileReport(store,root,0,"all","",tick,false);
  return "抓取已停止，已保存的訊息與原檔都已保留。\n可按「開啟已保存的訊息」查看本次結果。\n閱讀頁："+utf8(page)+"\n儲存位置："+utf8(root)+"\n"+warning;
 }catch(const Cancelled&){return "抓取已停止；已寫入的訊息與原檔仍保留。\n閱讀頁整理也已停止，可稍後按「更新離線閱讀頁」繼續。\n儲存位置："+utf8(root);}
 catch(const Error& error){return "抓取已停止；已寫入的訊息與原檔仍保留。\n閱讀頁尚未完成："+std::string(error.what())+"\n請按「更新離線閱讀頁」重試。\n儲存位置："+utf8(root);}
}

void collectClicked(const std::function<bool(const fs::path&)>& confirm=confirmNewCollection){
 if(app.busy)return;if(!app.source)throw Error("請先連接 LINE，或查看人工範例。");J selected=J::array();for(const auto& chat:app.chats)if(app.checked.count(chat.at("source_id").get<std::string>()))selected.push_back(chat);
 if(selected.empty())throw Error("請先勾選要保存的聊天室。");std::set<std::string> kinds;
 for(auto [id,values]:std::vector<std::pair<int,std::set<std::string>>>{{ID_MESSAGES,{"message","thread"}},{ID_ALBUMS,{"album","album_item"}},{ID_NOTES,{"note","comment"}}})if(SendMessageW(GetDlgItem(app.window,id),BM_GETCHECK,0,0)==BST_CHECKED)kinds.insert(values.begin(),values.end());
 if(kinds.empty())throw Error("請至少選擇一種要保存的內容。");auto since=dateRange(ID_FROM,false),until=dateRange(ID_UNTIL,true);
 if(nativeDatabaseConnection(app.source)&&(kinds.count("album")||kinds.count("note")))throw Error("本機資料庫來源提供完整訊息、圖片、影片與檔案；相簿或記事本需使用適用的進階來源。");
 if(!since.empty()&&!until.empty()&&since>=until)throw Error("開始日期不能晚於結束日期。");auto root=folderPath();
 if(!fs::is_regular_file(root/L"archive.sqlite3")){auto proposed=newCollectionPath(root,confirm);if(!proposed)return;root=*proposed;SetWindowTextW(app.folder,root.c_str());}
 auto mode=SendMessageW(GetDlgItem(app.window,ID_MESSAGE_MODE),CB_GETCURSEL,0,0)==1?std::string("skip_text"):std::string("update");auto upgrade=approveArchiveOpen(root);if(!upgrade)return;auto source=app.source;resetCaptureResults();
 startTask([root,selected,kinds,since,until,source,mode,allowUpgrade=*upgrade]{
  auto store=openRememberedArchive(root,[&]{app.control->check();},allowUpgrade);auto& s=*store;
  try{
   auto result=collect(s,*source,selected,*app.control,kinds,since,until,postStatus,mode,postCaptureProgress);
   postStatus("正在匯出完整文字紀錄…");auto textFolder=exportChatText(s,storedChatIds(*source,selected),[&]{app.control->check();});
   postStatus("正在產生離線閱讀頁…");std::string readerWarning;auto page=exportHtml(s,[&]{app.control->check();},&readerWarning);readerWarning+=openPublishedReader(page,*app.control);postFileReport(s,root,0,"all","",[&]{app.control->check();});
   return storeNotice(store)+readerWarning+"已保存 "+std::to_string(result["items"].get<int>())+" 則訊息；沿用 "+std::to_string(result["reused"].get<int>())+" 個原檔，新增 "+std::to_string(result["downloaded"].get<int>())+" 個原檔。\n未完成或無法取得："+std::to_string(result["failed"].get<int>()+result["unavailable"].get<int>())+" 個，請在保存結果查看原因。\nTXT："+utf8(textFolder)+"\n儲存位置："+utf8(root);
  }catch(const Cancelled&){return preserveStoppedCapture(s,root);}
 },true);
}
// Publication is the commit point. Preference/UI failures must not disguise a saved collection.
std::string finishPublishedImport(const ImportResult& imported,const std::function<void(const fs::path&)>& remember=rememberArchive){
 std::string message="已匯入新的私人聊天紀錄，原備份與其權限未修改。請開啟地圖或讀取採集紀錄。\n新聊天紀錄位置："+utf8(imported.root);
 try{remember(imported.root);}catch(...){message+="\n提醒：新聊天紀錄已保存，但偏好位置未能記住，下次請重新選擇此位置。";}
 auto result=std::make_unique<fs::path>(imported.root);
 if(PostMessageW(app.window,MSG_LOCATION,0,reinterpret_cast<LPARAM>(result.get())))result.release();
 else message+="\n提醒：聊天紀錄位置無法更新到畫面，請重新選擇上述新聊天紀錄。";
 if(!imported.warning.empty())message+="\n"+imported.warning;
 if(!imported.retained.empty())message+="\n另有私人暫存副本保留，請確認後自行處理："+utf8(imported.retained);
 return message;
}
void importClicked(){auto source=chooseFolder(L"選擇已解壓縮的完整聊天紀錄（內含 archive.sqlite3）");if(source.empty())return;if(!fs::is_regular_file(source/L"archive.sqlite3"))throw Error("這不是完整聊天紀錄備份。請先解壓縮完整 ZIP；只有離線網頁的人工範例不能匯入。");auto parent=chooseFolder(L"選擇新私人聊天紀錄的保存位置（原備份不會修改）");if(parent.empty())return;requireAclVolume(parent);noLinks(parent);auto target=parent/L"ChatAtlas-Imported";unsigned suffix=2;while(fs::exists(target)){if(suffix>999)throw Error("這個位置已有太多同名資料夾，請選擇其他位置。");target=parent/(L"ChatAtlas-Imported-"+std::to_wstring(suffix++));}auto message=L"會建立新的私人聊天紀錄：\n\n"+target.wstring()+L"\n\n原備份與原資料夾權限保持不變。匯入後會重新驗證檔案並產生閱讀頁面。是否繼續？";if(MessageBoxW(app.window,message.c_str(),L"匯入完整聊天紀錄副本",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;startTask([source,target]{ImportResult imported;try{imported=importBackup(source,target,[&]{app.control->check();});}catch(const ImportFailure& e){throw Error(std::string(e.what())+"\n原備份未修改。"+(e.retained.empty()?std::string():"保留的私人暫存副本："+utf8(e.retained)));}return finishPublishedImport(imported);},true);}

std::string publishedZipMessage(const fs::path& retained){return "完整聊天紀錄副本已保存。"+(retained.empty()?std::string():"\n私人資料庫暫存副本未能清除，仍保留在："+utf8(retained));}
// A report belongs to the most recent completed probe, never to a failed retry.
void refreshProbeReport(const std::function<J(Control*)>& inspect=probe){app.report=J();app.control->check();auto result=inspect(app.control.get());app.control->check();app.report=std::move(result);}
struct AttachmentPicker {J rows;HWND list=nullptr;int selected=-1;bool done=false;};
LRESULT CALLBACK attachmentPickerProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
 auto state=reinterpret_cast<AttachmentPicker*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(message==WM_NCCREATE){state=static_cast<AttachmentPicker*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
 if(!state)return DefWindowProcW(window,message,wp,lp);
 auto accept=[&]{auto row=ListView_GetNextItem(state->list,-1,LVNI_SELECTED);if(row>=0&&static_cast<size_t>(row)<state->rows.size()){state->selected=row;state->done=true;}};
 if(message==WM_COMMAND){if(LOWORD(wp)==IDOK){accept();return 0;}if(LOWORD(wp)==IDCANCEL){state->done=true;return 0;}}
 if(message==WM_NOTIFY&&reinterpret_cast<NMHDR*>(lp)->code==NM_DBLCLK){accept();return 0;}
 if(message==WM_CLOSE){state->done=true;return 0;}
 return DefWindowProcW(window,message,wp,lp);
}
std::optional<J> choosePendingAttachment(const J& rows){
 if(rows.empty())return std::nullopt;
 WNDCLASSW type{};type.lpfnWndProc=attachmentPickerProcedure;type.hInstance=GetModuleHandleW(nullptr);type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);type.lpszClassName=L"ChatAtlasAttachmentPicker";
 if(!RegisterClassW(&type)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw Error("無法開啟檔案選擇畫面。");
 AttachmentPicker state{rows};auto work=workArea(app.window);int width=std::min(scaled(850),int(work.right-work.left)),height=std::min(scaled(510),int(work.bottom-work.top));
 HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,type.lpszClassName,L"選擇要補入原檔的圖片或檔案",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,app.window,nullptr,type.hInstance,&state);
 if(!window)throw Error("無法開啟檔案選擇畫面。");
 struct Cleanup{HWND window;~Cleanup(){DestroyWindow(window);EnableWindow(app.window,TRUE);SetForegroundWindow(app.window);}} cleanup{window};
 RECT client{};GetClientRect(window,&client);
 auto control=[&](const wchar_t* kind,const wchar_t* label,DWORD style,int id,int x,int y,int w,int h){auto child=CreateWindowExW(0,kind,label,WS_CHILD|WS_VISIBLE|style,scaled(x),scaled(y),w,h,window,reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),type.hInstance,nullptr);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);return child;};
 control(L"STATIC",L"先選擇對應訊息，再選取原檔。僅列出前 1,000 筆待補的 LINE 檔案。",0,0,16,15,client.right-scaled(32),scaled(42));
 state.list=control(WC_LISTVIEWW,L"",WS_BORDER|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,10,16,62,client.right-scaled(32),client.bottom-scaled(133));
 ListView_SetExtendedListViewStyle(state.list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
 for(auto [title,columnWidth]:std::vector<std::pair<const wchar_t*,int>>{{L"聊天室",160},{L"時間 (UTC+8)",180},{L"檔案名稱",420}}){LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=scaled(columnWidth);column.pszText=const_cast<wchar_t*>(title);ListView_InsertColumn(state.list,Header_GetItemCount(ListView_GetHeader(state.list)),&column);}
 for(auto& row:rows){std::wstring chat=wide(row.value("chat_title",""));LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=ListView_GetItemCount(state.list);item.pszText=chat.data();auto index=ListView_InsertItem(state.list,&item);auto date=wide(historyStarted(row.value("sent_at",""))),name=wide(row.value("name","檔案"));ListView_SetItemText(state.list,index,1,date.data());ListView_SetItemText(state.list,index,2,name.data());}
 const int buttonY=std::max(0,int(client.bottom)-scaled(55));auto ok=CreateWindowExW(0,L"BUTTON",L"選擇原檔…",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,client.right-scaled(290),buttonY,scaled(140),scaled(36),window,reinterpret_cast<HMENU>(IDOK),type.hInstance,nullptr);auto cancel=CreateWindowExW(0,L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|WS_TABSTOP,client.right-scaled(135),buttonY,scaled(115),scaled(36),window,reinterpret_cast<HMENU>(IDCANCEL),type.hInstance,nullptr);for(auto child:{ok,cancel})SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);
 EnableWindow(app.window,FALSE);ShowWindow(window,SW_SHOW);SetFocus(state.list);MSG message{};
 while(!state.done){auto result=GetMessageW(&message,nullptr,0,0);if(result<=0){if(!result)PostQuitMessage(static_cast<int>(message.wParam));break;}if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
 if(state.selected<0)return std::nullopt;return rows[static_cast<size_t>(state.selected)];
}
void importMediaClicked(){
 auto root=folderPath(true);auto upgrade=approveArchiveOpen(root);if(!upgrade)return;J pending;
 {Store store(root,{},*upgrade);pending=pendingLineAttachments(store);}
 if(pending.empty()){setStatus("這份聊天紀錄目前沒有需要補入原檔的 LINE 檔案。");return;}
 auto selected=choosePendingAttachment(pending);if(!selected)return;
 auto original=chooseFile(false,L"選擇這則訊息的檔案原檔",L"檔案原檔\0*.*\0\0",nullptr);if(original.empty())return;
 auto attachmentId=selected->at("id").get<std::string>();startTask([root,original,attachmentId]{auto store=openRememberedArchive(root,[&]{app.control->check();});auto& s=*store;importLineAttachment(s,attachmentId,original,*app.control);std::string readerWarning;auto page=exportHtml(s,[&]{app.control->check();},&readerWarning);readerWarning+=openPublishedReader(page,*app.control);postFileReport(s,root,0,"all","",[&]{app.control->check();});return storeNotice(store)+readerWarning+"原檔已保存，離線閱讀頁已更新。";},true);
}

constexpr int ID_DB_PICK_LIST=2101,ID_DB_PICK_PATH=2102,ID_DB_PICK_BROWSE=2103,ID_DB_PICK_SUMMARY=2104,ID_DB_PICK_ALL=2105,ID_DB_PICK_CLEAR=2106,ID_DB_PICK_COUNT=2107;
constexpr size_t maxLineDatabaseSelections=64,maxLineDatabaseSelectionChars=maxLineDatabaseSelections*32768+2;
std::vector<fs::path> validateLineDatabaseSelection(const std::vector<fs::path>& paths){
 std::vector<fs::path> result;std::set<std::wstring> seen;
 for(const auto& raw:paths){
  if(!raw.is_absolute()||raw.native().size()>=32768||(_wcsicmp(raw.extension().c_str(),L".edb")!=0&&_wcsicmp(raw.extension().c_str(),L".db")!=0))throw Error("請選擇 .edb 或 .db，聊天紀錄資料庫不能作為 LINE 來源。");
  auto path=absoluteNormalizedPath(raw);if(seen.insert(path.wstring()).second)result.push_back(std::move(path));
  if(result.size()>maxLineDatabaseSelections)throw Error("一次最多選擇 64 份 LINE 資料庫。");
 }
 return result;
}
std::vector<fs::path> parseLineDatabaseFileSelection(const std::vector<wchar_t>& buffer){
 if(buffer.empty()||buffer.size()>maxLineDatabaseSelectionChars)throw Error("資料庫選擇回應過長或不完整。");
 if(!buffer[0])return {};
 size_t at=0;
 auto word=[&]{if(at>=buffer.size())throw Error("資料庫選擇回應不完整。");auto end=std::find(buffer.begin()+at,buffer.end(),L'\0');if(end==buffer.end())throw Error("資料庫選擇回應不完整。");std::wstring result(buffer.begin()+at,end);at=static_cast<size_t>(end-buffer.begin())+1;return result;};
 const auto first=fs::path(word());if(at>=buffer.size())throw Error("資料庫選擇回應不完整。");
 if(!buffer[at])return validateLineDatabaseSelection({first});
 if(!first.is_absolute())throw Error("資料庫選擇回應缺少完整資料夾位置。");
 std::vector<fs::path> paths;
 while(at<buffer.size()&&buffer[at]){auto name=fs::path(word());if(name.empty()||name.is_absolute()||name.has_parent_path()||name==L"."||name==L"..")throw Error("資料庫選擇回應含不正確的檔名。");paths.push_back(first/name);if(paths.size()>maxLineDatabaseSelections)throw Error("一次最多選擇 64 份 LINE 資料庫。");}
 if(at>=buffer.size())throw Error("資料庫選擇回應不完整。");
 return validateLineDatabaseSelection(paths);
}
std::vector<fs::path> chooseLineDatabaseFiles(const fs::path& directory,HWND owner){
 std::vector<wchar_t> buffer(maxLineDatabaseSelectionChars,L'\0');OPENFILENAMEW of{};of.lStructSize=sizeof of;of.hwndOwner=owner;of.lpstrFile=buffer.data();of.nMaxFile=static_cast<DWORD>(buffer.size());of.lpstrTitle=L"批次選擇 LINE 資料庫（Ctrl／Shift 多選，最多 64 份）";of.lpstrFilter=L"LINE 資料庫 (*.edb;*.db)\0*.edb;*.db\0\0";of.lpstrInitialDir=directory.empty()?nullptr:directory.c_str();of.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_NOCHANGEDIR|OFN_DONTADDTORECENT|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
 if(!GetOpenFileNameW(&of)){if(CommDlgExtendedError())throw Error("無法完成批次選擇，請減少檔案數量後重試（最多 64 份）。");return {};}
 return parseLineDatabaseFileSelection(buffer);
}
struct LineDatabasePicker {
 LineDatabaseLocations locations;
 HWND window=nullptr,list=nullptr,path=nullptr,summary=nullptr,count=nullptr,connect=nullptr;
 std::vector<fs::path> selected,result;
 bool done=false,drawing=false;
 std::function<std::vector<fs::path>(const fs::path&)> browse;
};
void showLineDatabaseSelection(LineDatabasePicker& state){
 if(state.drawing||!state.list||!state.path||!state.connect)return;
 state.selected.clear();
 for(size_t index=0;index<state.locations.candidates.size();++index)if(ListView_GetCheckState(state.list,static_cast<int>(index)))state.selected.push_back(state.locations.candidates[index].path);
 auto row=ListView_GetNextItem(state.list,-1,LVNI_FOCUSED);if(row<0)row=ListView_GetNextItem(state.list,-1,LVNI_SELECTED);
 auto shown=row>=0&&static_cast<size_t>(row)<state.locations.candidates.size()?state.locations.candidates[static_cast<size_t>(row)].path:(state.selected.empty()?fs::path{}:state.selected.front());
 SetWindowTextW(state.path,shown.c_str());
 const auto count=L"已勾選 "+std::to_wstring(state.selected.size())+L" 份｜目前列的完整路徑（可複製）";SetWindowTextW(state.count,count.c_str());
 EnableWindow(state.connect,!state.selected.empty()&&state.selected.size()<=maxLineDatabaseSelections);
}
void checkLineDatabaseRows(LineDatabasePicker& state,bool checked){
 state.drawing=true;for(int row=0;row<ListView_GetItemCount(state.list);++row)ListView_SetCheckState(state.list,row,checked);state.drawing=false;showLineDatabaseSelection(state);
}
LRESULT CALLBACK lineDatabaseListProcedure(HWND list,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR reference){
 auto state=reinterpret_cast<LineDatabasePicker*>(reference);
 if(message==WM_KEYDOWN&&wp==VK_SPACE&&state){
  auto focused=ListView_GetNextItem(list,-1,LVNI_FOCUSED);if(focused<0)focused=ListView_GetNextItem(list,-1,LVNI_SELECTED);
  if(focused>=0){const bool checked=!ListView_GetCheckState(list,focused);state->drawing=true;bool any=false;for(int row=ListView_GetNextItem(list,-1,LVNI_SELECTED);row>=0;row=ListView_GetNextItem(list,row,LVNI_SELECTED)){ListView_SetCheckState(list,row,checked);any=true;}if(!any)ListView_SetCheckState(list,focused,checked);state->drawing=false;showLineDatabaseSelection(*state);}return 0;
 }
 if(message==WM_CHAR&&wp==L' ')return 0;
 if(message==WM_NCDESTROY)RemoveWindowSubclass(list,lineDatabaseListProcedure,id);
 return DefSubclassProc(list,message,wp,lp);
}
void drawLineDatabaseCandidates(LineDatabasePicker& state,bool checked){
 state.drawing=true;ListView_DeleteAllItems(state.list);
 for(const auto& candidate:state.locations.candidates){auto name=candidate.path.filename().wstring(),parent=candidate.path.parent_path().wstring(),size=std::to_wstring(candidate.size/1024)+L" KB";LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=ListView_GetItemCount(state.list);item.pszText=name.data();int row=ListView_InsertItem(state.list,&item);ListView_SetItemText(state.list,row,1,parent.data());ListView_SetItemText(state.list,row,2,size.data());ListView_SetCheckState(state.list,row,checked);}
 if(checked&&!state.locations.candidates.empty())ListView_SetItemState(state.list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
 state.drawing=false;showLineDatabaseSelection(state);
}
LRESULT CALLBACK lineDatabasePickerProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
 auto state=reinterpret_cast<LineDatabasePicker*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(message==WM_NCCREATE){state=static_cast<LineDatabasePicker*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
 if(!state)return DefWindowProcW(window,message,wp,lp);
 auto accept=[&]{showLineDatabaseSelection(*state);if(!state->selected.empty()&&state->selected.size()<=maxLineDatabaseSelections){state->result=state->selected;state->done=true;}};
 try{
  if(message==WM_COMMAND){
   if(LOWORD(wp)==IDOK){accept();return 0;}
   if(LOWORD(wp)==IDCANCEL){state->result.clear();state->done=true;return 0;}
   if(LOWORD(wp)==ID_DB_PICK_ALL||LOWORD(wp)==ID_DB_PICK_CLEAR){checkLineDatabaseRows(*state,LOWORD(wp)==ID_DB_PICK_ALL);return 0;}
   if(LOWORD(wp)==ID_DB_PICK_BROWSE){
    auto selected=state->browse(state->locations.defaultDirectory);if(selected.empty())return 0;
    selected=validateLineDatabaseSelection(selected);
    std::vector<LineDatabaseCandidate> candidates;for(const auto& path:selected){LineDatabaseCandidate candidate;candidate.path=path;std::error_code ec;candidate.size=fs::file_size(path,ec);if(ec)candidate.size=0;candidates.push_back(std::move(candidate));}
    state->locations.candidates=std::move(candidates);drawLineDatabaseCandidates(*state,true);
    SetWindowTextW(state->summary,(L"已手動選擇 "+std::to_wstring(selected.size())+L" 份，請按「批次連接」。").c_str());
    SetFocus(state->connect);return 0;
   }
  }
  if(message==WM_NOTIFY){auto notice=reinterpret_cast<NMHDR*>(lp);
   if(notice->idFrom==ID_DB_PICK_LIST&&notice->code==LVN_ITEMCHANGED){showLineDatabaseSelection(*state);return 0;}
  }
  if(message==WM_CLOSE){state->result.clear();state->done=true;return 0;}
 }catch(const Error& e){SetWindowTextW(state->summary,wide(e.what()).c_str());return 0;}
 catch(...){SetWindowTextW(state->summary,L"無法批次選檔，請重試或取消。");return 0;}
 return DefWindowProcW(window,message,wp,lp);
}
HWND createLineDatabasePicker(LineDatabasePicker& state){
 if(state.locations.candidates.size()>maxLineDatabaseSelections)throw Error("資料庫候選超過 64 份，請手動縮小選取範圍。");
 WNDCLASSW type{};type.lpfnWndProc=lineDatabasePickerProcedure;type.hInstance=GetModuleHandleW(nullptr);type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);type.lpszClassName=L"ChatAtlasLineDatabasePicker";
 if(!RegisterClassW(&type)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw Error("無法開啟 LINE 資料庫選擇畫面。");
 auto work=workArea(app.window);int width=std::min(scaled(840),int(work.right-work.left)),height=std::min(scaled(510),int(work.bottom-work.top));
 auto window=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,type.lpszClassName,L"批次選擇 LINE 資料庫",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,app.window,nullptr,type.hInstance,&state);
 if(!window)throw Error("無法開啟 LINE 資料庫選擇畫面。");state.window=window;
 try{
  RECT client{};GetClientRect(window,&client);const int margin=scaled(16),content=client.right-margin*2,bottom=client.bottom;
  auto control=[&](const wchar_t* kind,const wchar_t* label,DWORD style,int id,int x,int y,int w,int h){auto child=CreateWindowExW(0,kind,label,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),type.hInstance,nullptr);if(!child)throw Error("無法建立資料庫選擇控制項。");SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(app.font),TRUE);return child;};
  auto hint=control(L"STATIC",L"勾選多份 LINE 資料庫後按「批次連接」。\nCtrl／Shift 選取多列，空白鍵切換勾選。",0,0,margin,scaled(6),content,scaled(30));SendMessageW(hint,WM_SETFONT,reinterpret_cast<WPARAM>(app.smallFont),TRUE);
  auto summary=state.locations.candidates.empty()?L"尚未找到，可手動多選 .edb／.db。":(L"找到 "+std::to_wstring(state.locations.candidates.size())+L" 份。請勾選要讀取的來源。");
  if(state.locations.limited)summary=L"僅列前 "+std::to_wstring(state.locations.candidates.size())+L" 份，其他來源請手動選檔。";
  state.summary=control(L"STATIC",summary.c_str(),0,ID_DB_PICK_SUMMARY,margin,scaled(40),content-scaled(156),scaled(28));SendMessageW(state.summary,WM_SETFONT,reinterpret_cast<WPARAM>(app.smallFont),TRUE);
  control(L"BUTTON",L"全選",WS_TABSTOP,ID_DB_PICK_ALL,client.right-margin-scaled(146),scaled(40),scaled(68),scaled(28));
  control(L"BUTTON",L"清除",WS_TABSTOP,ID_DB_PICK_CLEAR,client.right-margin-scaled(68),scaled(40),scaled(68),scaled(28));
  state.list=control(WC_LISTVIEWW,L"",WS_BORDER|WS_TABSTOP|LVS_REPORT|LVS_SHOWSELALWAYS,ID_DB_PICK_LIST,margin,scaled(72),content,std::max(scaled(40),bottom-scaled(180)));ListView_SetExtendedListViewStyle(state.list,LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
  if(!SetWindowSubclass(state.list,lineDatabaseListProcedure,1,reinterpret_cast<DWORD_PTR>(&state)))throw Error("無法建立批次選擇鍵盤操作。");
  for(auto [title,columnWidth]:std::vector<std::pair<const wchar_t*,int>>{{L"資料庫檔案",scaled(190)},{L"所在資料夾",std::max(scaled(150),content-scaled(275))},{L"大小",scaled(80)}}){LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=columnWidth;column.pszText=const_cast<wchar_t*>(title);ListView_InsertColumn(state.list,Header_GetItemCount(ListView_GetHeader(state.list)),&column);}
  state.count=control(L"STATIC",L"",0,ID_DB_PICK_COUNT,margin,bottom-scaled(100),content,scaled(20));SendMessageW(state.count,WM_SETFONT,reinterpret_cast<WPARAM>(app.smallFont),TRUE);
  state.path=control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_READONLY|ES_AUTOHSCROLL,ID_DB_PICK_PATH,margin,bottom-scaled(78),content,scaled(26));
  const int gap=scaled(10),cancelWidth=scaled(80),connectWidth=scaled(150),manualWidth=std::min(scaled(190),content-cancelWidth-connectWidth-gap*2);
  auto manual=control(L"BUTTON",L"手動多選…",WS_TABSTOP|BS_MULTILINE,ID_DB_PICK_BROWSE,margin,bottom-scaled(42),manualWidth,scaled(30));
  state.connect=control(L"BUTTON",L"批次連接",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,client.right-margin-cancelWidth-gap-connectWidth,bottom-scaled(42),connectWidth,scaled(30));
  control(L"BUTTON",L"取消",WS_TABSTOP,IDCANCEL,client.right-margin-cancelWidth,bottom-scaled(42),cancelWidth,scaled(30));
  if(!state.browse)state.browse=[window](const fs::path& directory){return chooseLineDatabaseFiles(directory,window);};
  drawLineDatabaseCandidates(state,state.locations.candidates.size()==1);
  if(state.locations.candidates.empty())SetFocus(manual);else SetFocus(state.list);return window;
 }catch(...){DestroyWindow(window);state.window=nullptr;throw;}
}
std::vector<fs::path> chooseLineDatabases(){
 LineDatabasePicker state;state.locations=currentLineDatabaseLocations();auto window=createLineDatabasePicker(state);
 struct Cleanup{HWND window;~Cleanup(){DestroyWindow(window);EnableWindow(app.window,TRUE);SetForegroundWindow(app.window);}} cleanup{window};
 EnableWindow(app.window,FALSE);ShowWindow(window,SW_SHOW);SetFocus(state.locations.candidates.empty()?GetDlgItem(window,ID_DB_PICK_BROWSE):state.list);
 MSG message{};while(!state.done){auto result=GetMessageW(&message,nullptr,0,0);if(result<=0){if(!result)PostQuitMessage(static_cast<int>(message.wParam));break;}if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
 return state.result;
}

using LineBatchConnector=std::function<LineBatchOpenResult(const std::vector<fs::path>&,Control&,std::function<void(std::string)>)>;
LineBatchOpenResult connectLineBatch(const std::vector<fs::path>& files,Control& control,std::function<void(std::string)> progress){return openLineDatabases(files,control,std::move(progress));}
std::string lineBatchNotice(const LineBatchOpenResult& result){
 auto notice=result.summary();size_t shown=0;
 for(const auto& entry:result.entries)if(entry.status!="ready"&&shown++<12)notice+="\n"+utf8(entry.path.filename())+"："+entry.reason;
 if(shown>12)notice+="\n另有 "+std::to_string(shown-12)+" 份未列出。";
 return notice;
}
void publishLineConnection(const std::shared_ptr<Source>& source,J chats,bool preserveOptions=false){
 app.control->check();auto pending=std::make_unique<ConnectionResult>(ConnectionResult{source,std::move(chats),preserveOptions});
 if(!PostMessageW(app.window,MSG_CHATS,0,reinterpret_cast<LPARAM>(pending.get())))throw Error("本機聊天室清單無法顯示，請重新連接。");pending.release();
}
void lineDatabaseClicked(const std::function<std::vector<fs::path>()>& pick=chooseLineDatabases,const LineBatchConnector& connect=connectLineBatch){
 if(app.busy)return;auto files=pick();if(files.empty())return;
 clearConnection();app.report=J();
 startTask([files,connect]{auto result=connect(files,*app.control,postStatus);if(!result.source)throw CapabilityError(lineBatchNotice(result));auto chats=result.source->chatsWithControl(*app.control);publishLineConnection(result.source,std::move(chats));return lineBatchNotice(result)+"\n請勾選聊天室並按「保存選取的聊天室」。對話會匯出 TXT 與離線閱讀頁，檔案會嘗試自動保存。";},true);
}
void autoCaptureClicked(const std::function<LineDatabaseLocations()>& discover=[] {return currentLineDatabaseLocations();},const LineBatchConnector& connect=connectLineBatch,const std::function<void(const fs::path&)>& openReader=openPage){
 if(app.busy)return;auto requestedRoot=folderPath();auto since=dateRange(ID_FROM,false),until=dateRange(ID_UNTIL,true);
 if(!since.empty()&&!until.empty()&&since>=until)throw Error("開始日期不能晚於結束日期。");auto upgrade=approveArchiveOpen(requestedRoot);if(!upgrade)return;
 auto mode=SendMessageW(GetDlgItem(app.window,ID_MESSAGE_MODE),CB_GETCURSEL,0,0)==1?std::string("skip_text"):std::string("update");resetCaptureResults();
 startTask([requestedRoot,since,until,mode,allowUpgrade=*upgrade,discover,connect,openReader]{
  postStatus("正在自動尋找 LINE 本機對話資料庫…");auto locations=discover();app.control->check();std::vector<fs::path> files;for(const auto& candidate:locations.candidates)files.push_back(candidate.path);
  if(files.empty())throw CapabilityError("未在 LINE 常見位置找到對話資料庫。請保持 LINE 登入，或按「批次選擇資料庫」一次指定多個 .edb／.db。");
  auto connected=connect(files,*app.control,postStatus);if(!connected.source)throw CapabilityError(lineBatchNotice(connected));auto source=connected.source;auto selected=source->chatsWithControl(*app.control);
  if(selected.empty())throw CapabilityError(lineBatchNotice(connected)+"\n來源內沒有可保存的聊天室，尚未建立新資料夾。");app.control->check();auto root=requestedRoot;
  if(!fs::is_regular_file(root/L"archive.sqlite3")){auto proposed=newCollectionPath(root,[](const fs::path&){return true;});if(!proposed)throw Cancelled();root=*proposed;}
  auto tick=[&]{app.control->check();};auto archive=openRememberedArchive(root,tick,allowUpgrade);auto location=std::make_unique<fs::path>(root);
  if(!PostMessageW(app.window,MSG_LOCATION,0,reinterpret_cast<LPARAM>(location.get())))throw Error("儲存位置無法顯示，請重新開啟程式。");location.release();publishLineConnection(source,selected,true);
  try{
   auto counts=collect(*archive,*source,selected,*app.control,{"message","thread"},since,until,postStatus,mode,postCaptureProgress);
   postStatus("正在匯出 TXT 對話紀錄…");auto textFolder=exportChatText(*archive,storedChatIds(*source,selected),tick);
   postStatus("正在產生離線閱讀頁…");std::string warning;auto page=exportHtml(*archive,tick,&warning);warning+=openPublishedReader(page,*app.control,openReader);postFileReport(*archive,root,0,"all","",tick);
   auto notice=lineBatchNotice(connected)+"\n"+storeNotice(archive)+warning;if(locations.limited)notice+="尋找範圍或數量已達上限，其他來源可使用批次選擇。\n";
   return notice+"\n已保存 "+std::to_string(selected.size())+" 個聊天室、"+std::to_string(counts["items"].get<int>())+" 則訊息，新增 "+std::to_string(counts["downloaded"].get<int>())+" 個原檔。\nTXT："+utf8(textFolder)+"\n儲存位置："+utf8(root)+"\n未完成或無法取得："+std::to_string(counts["failed"].get<int>()+counts["unavailable"].get<int>())+" 個。";
  }catch(const Cancelled&){return preserveStoppedCapture(*archive,root);}
 },true);
}
void command(int id){
 if(id==ID_SEARCH||id==ID_CHAT_TYPE||id==ID_CHAT_SOURCE||id==ID_CHAT_PAGE_SIZE||id==ID_CHAT_PREV||id==ID_CHAT_NEXT||id==ID_SELECT_PAGE||id==ID_CLEAR_PAGE||id==ID_SELECT_FILTERED||id==ID_CLEAR_ALL){chatSelectionCommand(id);return;}
 if(id==ID_FILE_STATE||id==ID_FILE_SEARCH){if(app.showProgress)drawFileRows();SetTimer(app.window,2,220,nullptr);return;}
 if(id==ID_FILE_REFRESH||id==ID_FILE_PREV||id==ID_FILE_NEXT){if(app.busy)return;size_t offset=id==ID_FILE_PREV?(app.fileOffset>=50?app.fileOffset-50:0):id==ID_FILE_NEXT?app.fileOffset+50:0;fileReportClicked(app.fileVerifyContents,offset);return;}
 if(id==ID_SETTINGS){TabCtrl_SetCurSel(app.tabs,4);activatePage(4);return;}
 if(id==ID_VIEW_RESULT){if(app.busy)return;command(ID_OPEN);return;}if(id==ID_INFO){MessageBoxW(app.window,L"聊藏 0.3.1-alpha.11\n\n離線閱讀、對話地圖與完整聊天紀錄。\n直接開啟使用，不需額外安裝環境。\n真實 LINE 採集仍待實機相容性驗證。",L"關於聊藏",MB_OK|MB_ICONINFORMATION);return;}if(id==ID_STOP){auto active=app.preserving.load()?app.preserveControl:app.control;active->stopped=true;active->paused=false;setStatus(app.preserving.load()?"正在停止閱讀頁整理；已保存訊息與原檔仍會保留…":"正在停止抓取，接著保留已保存訊息的閱讀頁…");return;}if(id==ID_PAUSE){auto active=app.preserving.load()?app.preserveControl:app.control;active->paused=!active->paused;SetWindowTextW(GetDlgItem(app.window,ID_PAUSE),active->paused?L"繼續":L"暫停");setStatus(active->paused?"已要求暫停，目前步驟完成後暫停。":"正在繼續保存…");return;}if(app.busy)return;if(id==ID_OPEN_DATA){openProgramData();return;}if(id==ID_IMPORT){importClicked();return;}if(id==ID_HISTORY_REFRESH||id==ID_HISTORY_APPLY||id==ID_HISTORY_DELETE||id==ID_HISTORY_PREV||id==ID_HISTORY_NEXT){historyCommand(id);return;}if(id==ID_BROWSE){browse();return;}if(id==ID_UIA_CONNECT){clearConnection();app.report=J();auto file=app.config/L"connection.local.json";if(!fs::exists(file)){startTask([]{refreshProbeReport();auto v=app.report.value("version","");return "已辨識 LINE "+v+"。尚未載入連線設定，因此只完成版本檢查。採集需先在「連線檢查」載入適用設定，且仍待實機驗證；可先開啟既有聊天紀錄或人工範例。";},true);return;}auto profile=J::parse(readText(file,131072));startTask([profile]{auto source=std::make_shared<WindowsSource>(profile,app.control.get());auto chats=source->chatsWithControl(*app.control);app.control->check();auto pending=std::make_unique<ConnectionResult>(ConnectionResult{std::move(source),std::move(chats)});if(!PostMessageW(app.window,MSG_CHATS,0,reinterpret_cast<LPARAM>(pending.get())))throw Error("連線結果無法顯示，請重新連接 LINE。");pending.release();return "已連接 LINE，請勾選要整理的聊天室。";},true);return;}
 if(id==ID_AUTO_CAPTURE){autoCaptureClicked();return;}
 if(id==ID_CONNECT||id==ID_LINE_DATABASE){lineDatabaseClicked();return;}
 if(id==ID_MEDIA_IMPORT){importMediaClicked();return;}
 if(id==ID_DEMO){auto root=app.config/L"Example";auto upgrade=approveArchiveOpen(root);if(!upgrade)return;startTask([root,allowUpgrade=*upgrade]{DemoSource source;auto store=openRememberedArchive(root,[&]{app.control->check();},allowUpgrade);auto& s=*store;collect(s,source,source.chats(),*app.control,allKinds,"","",postStatus);std::string readerWarning;auto page=exportHtml(s,[&]{app.control->check();},&readerWarning);readerWarning+=openPublishedReader(page,*app.control);postFileReport(s,root,0,"all","",[&]{app.control->check();});return storeNotice(store)+readerWarning+"已產生人工範例，沒有讀取你的 LINE。";},true);return;}
 if(id==ID_START){collectClicked();return;}if(id==ID_PROBE){app.report=J();startTask([]{refreshProbeReport();auto v=app.report.value("version","");return "LINE 版本："+v+"。發行者已核對；完整採集仍待實機驗證。";},true);return;}
 if(id==ID_PROFILE){auto file=chooseFile(false,L"選擇本機連線設定",L"連線設定 (*.json)\0*.json\0\0",nullptr);if(file.empty())return;auto content=readText(file,131072);WindowsSource::validate(J::parse(content));atomicText(app.config/L"connection.local.json",content);clearConnection();app.report=J();setStatus("本機連線設定已載入，請重新連接 LINE。");return;}
 if(id==ID_REPORT){if(app.report.is_null())throw Error("請先執行連線檢查。");auto dest=chooseFile(true,L"另存不含聊天內容的檢查結果",L"檢查結果 (*.json)\0*.json\0\0",L"LINE-connection-report.json");if(!dest.empty()){if(fs::exists(dest))throw Error("這個位置已有檔案，請改用其他名稱。");atomicText(dest,safeJson(app.report));setStatus("檢查結果已保存，不含聊天、帳號名稱或私人路徑。");}return;}
 if(id==ID_OPEN){auto root=folderPath(true);auto page=root/L"index.html";if(!fs::is_regular_file(page))throw Error("請先按「更新離線閱讀頁」。");openPage(page);rememberArchive(root);return;}
 if(id==ID_CHECK){fileReportClicked(true,app.fileOffset);return;}
 if(id==ID_REBUILD){auto root=folderPath(true);auto upgrade=approveArchiveOpen(root);if(!upgrade)return;startTask([root,allowUpgrade=*upgrade]{auto store=openRememberedArchive(root,[&]{app.control->check();},allowUpgrade);auto& s=*store;std::string readerWarning;auto page=exportHtml(s,[&]{app.control->check();},&readerWarning,false);readerWarning+=openPublishedReader(page,*app.control);postFileReport(s,root,0,"all","",[&]{app.control->check();});return storeNotice(store)+readerWarning+"已更新保存訊息的離線閱讀頁，可直接查看已保存原檔。";},true);return;}
 if(id==ID_BACKUP){auto root=folderPath(true);auto dest=chooseFile(true,L"另存完整聊天紀錄",L"ZIP 副本 (*.zip)\0*.zip\0\0",L"聊藏-聊天紀錄副本.zip");if(dest.empty())return;if(MessageBoxW(app.window,L"完整 ZIP 包含私人對話與檔案，而且沒有加密。\n請保存在你信任的位置；隨身碟不一定能限制其他人讀取。若需放到共用位置，請先存到本機，再自行複製到信任的位置。",L"確認保存完整副本",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;auto upgrade=approveArchiveOpen(root);if(!upgrade)return;startTask([root,dest,allowUpgrade=*upgrade]{auto store=openRememberedArchive(root,[&]{app.control->check();},allowUpgrade);auto& s=*store;std::string readerWarning;auto retained=makeZip(s,dest,[&]{app.control->check();},&readerWarning);return storeNotice(store)+readerWarning+publishedZipMessage(retained);},true);return;}}
void create(HWND w){
 app.window=w;app.dpi=windowDpi(w);rebuildFonts();app.background=CreateSolidBrush(RGB(245,247,245));app.config=portableConfig();
 auto label=[&](int id,const wchar_t* text,int page,int role=0){auto c=child(L"STATIC",text,0,id,page,24,152,800,28);setRole(c,role);return c;};
 auto button=[&](int id,const wchar_t* text,int page,DWORD style=BS_PUSHBUTTON){return child(L"BUTTON",text,style,id,page,24,152,156,34);};
 auto combo=[&](int id,int page,std::initializer_list<const wchar_t*> values){auto c=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,id,page,24,152,160,220);for(auto value:values)SendMessageW(c,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));SendMessageW(c,CB_SETCURSEL,0,0);return c;};
 label(ID_HEADER,L"聊藏 · 聊天紀錄",-1,1);label(ID_SUBTITLE,L"保存完整訊息，清楚查看每個圖片、影片與檔案的結果。",-1,2);auto about=button(ID_INFO,L"0.3.1-alpha.11 · 關於",-1,BS_FLAT);setRole(about,2);
 app.tabs=child(WC_TABCONTROLW,L"",0,ID_TABS,-1,24,98,832,36);for(auto title:{L"聊天室",L"保存結果",L"連線檢查",L"抓取紀錄",L"保存設定"}){TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=const_cast<wchar_t*>(title);TabCtrl_InsertItem(app.tabs,TabCtrl_GetItemCount(app.tabs),&tab);}
 button(ID_AUTO_CAPTURE,L"自動抓取全部",0,BS_OWNERDRAW);button(ID_CONNECT,L"批次選擇資料庫…",0);button(ID_DEMO,L"查看人工範例",0);button(ID_SETTINGS,L"日期與保存設定",0);
 label(ID_MAIN_HINT,L"先登入 LINE。自動抓取會保存可讀來源的完整訊息；也可選取部分聊天室。",0,2);
 app.search=child(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,ID_SEARCH,0,24,234,320,32);SendMessageW(app.search,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"搜尋聊天室名稱或來源"));
 combo(ID_CHAT_TYPE,0,{L"全部類型",L"個人對話",L"群組",L"社群",L"社群聊天室"});combo(ID_CHAT_SOURCE,0,{L"全部來源"});combo(ID_CHAT_PAGE_SIZE,0,{L"25 / 頁",L"50 / 頁",L"100 / 頁"});
 button(ID_SELECT_PAGE,L"選取本頁",0);button(ID_CLEAR_PAGE,L"取消本頁",0);button(ID_SELECT_FILTERED,L"選取篩選結果",0);button(ID_CLEAR_ALL,L"清空所有選取",0);label(ID_SELECTION_COUNT,L"已選 0 個",0,2);
 app.list=child(WC_LISTVIEWW,L"",WS_BORDER|LVS_REPORT|LVS_SHOWSELALWAYS,ID_LIST,0,24,320,832,220);ListView_SetExtendedListViewStyle(app.list,LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
 for(auto [text,width]:std::vector<std::pair<const wchar_t*,int>>{{L"聊天室 / 對象",450},{L"類型",120},{L"來源",210}}){LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=width;column.pszText=const_cast<wchar_t*>(text);ListView_InsertColumn(app.list,Header_GetItemCount(ListView_GetHeader(app.list)),&column);}
 button(ID_CHAT_PREV,L"上一頁",0);button(ID_CHAT_NEXT,L"下一頁",0);label(ID_CHAT_PAGE,L"第 1 / 1 頁 · 0 個聊天室",0,2);
 label(ID_FOLDER_LABEL,L"儲存位置",-1,2);auto last=app.config/L"Collections";try{auto preferences=J::parse(readText(app.config/L"preferences.local.json",16384));if(preferences.contains("last_archive")&&preferences["last_archive"].is_string())last=savedArchive(preferences);}catch(...){}
 app.folder=child(L"EDIT",last.c_str(),WS_BORDER|ES_AUTOHSCROLL,ID_FOLDER,-1,132,524,580,32);button(ID_BROWSE,L"選擇資料夾",-1);
 button(ID_START,L"保存選取的聊天室",0,BS_DEFPUSHBUTTON);button(ID_PAUSE,L"暫停",-1);button(ID_STOP,L"停止並保留",-1);button(ID_VIEW_RESULT,L"開啟已保存的訊息",-1);EnableWindow(GetDlgItem(w,ID_PAUSE),FALSE);EnableWindow(GetDlgItem(w,ID_STOP),FALSE);EnableWindow(GetDlgItem(w,ID_START),FALSE);
 label(ID_RESULT_TITLE,L"圖片、影片與檔案的保存結果",1);label(ID_RESULT_HINT,L"依檢查結果篩選，查看每個原檔、預覽或未保存項目的具體原因。",1,2);
 button(ID_OPEN,L"開啟訊息紀錄",1,BS_OWNERDRAW);button(ID_CHECK,L"完整檢查檔案",1);button(ID_REBUILD,L"更新離線閱讀頁",1);button(ID_BACKUP,L"另存完整 ZIP",1);button(ID_IMPORT,L"匯入完整備份",1);button(ID_FILE_REFRESH,L"重新整理",1);button(ID_MEDIA_IMPORT,L"補入圖片或檔案原檔",1);button(ID_OPEN_DATA,L"開啟程式資料夾",1);
 combo(ID_FILE_STATE,1,{L"全部檢查狀態",L"已保存原檔",L"僅有預覽",L"尚未保存",L"目前無法取得",L"保存失敗"});auto fileSearch=child(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,ID_FILE_SEARCH,1,596,270,240,32);SendMessageW(fileSearch,EM_SETLIMITTEXT,512,0);SendMessageW(fileSearch,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"搜尋檔名或聊天室"));
 app.fileList=child(WC_LISTVIEWW,L"",WS_BORDER|LVS_REPORT|LVS_SHOWSELALWAYS,ID_FILES,1,24,314,832,120);ListView_SetExtendedListViewStyle(app.fileList,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
 for(auto [text,width]:std::vector<std::pair<const wchar_t*,int>>{{L"聊天室",180},{L"檔案名稱",240},{L"種類",100},{L"狀態",120},{L"原因與可用情況",240}}){LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=width;column.pszText=const_cast<wchar_t*>(text);ListView_InsertColumn(app.fileList,Header_GetItemCount(ListView_GetHeader(app.fileList)),&column);}
 button(ID_FILE_PREV,L"上一頁",1);button(ID_FILE_NEXT,L"下一頁",1);label(ID_FILE_PAGE,L"尚未開始保存或檢查",1,2);
 app.taskNote=child(L"EDIT",L"此處會保留目前工作的結果與儲存位置。",WS_BORDER|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,ID_TASK_NOTE,1,24,444,832,88);
 label(ID_HISTORY_TITLE,L"抓取紀錄",3);label(ID_HISTORY_HINT,L"每次保存的範圍與結果都會保留。刪除紀錄不會刪除已保存的訊息或檔案。",3,2);
 app.historyList=child(WC_LISTVIEWW,L"",WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,ID_HISTORY,3,24,236,832,220);ListView_SetExtendedListViewStyle(app.historyList,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
 for(auto [text,width]:std::vector<std::pair<const wchar_t*,int>>{{L"開始時間 (UTC+8)",190},{L"聊天室",240},{L"結果",120},{L"訊息 / 檔案",290}}){LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=width;column.pszText=const_cast<wchar_t*>(text);ListView_InsertColumn(app.historyList,Header_GetItemCount(ListView_GetHeader(app.historyList)),&column);}
 button(ID_HISTORY_REFRESH,L"讀取抓取紀錄",3,BS_OWNERDRAW);button(ID_HISTORY_APPLY,L"套用這次保存範圍",3);button(ID_HISTORY_DELETE,L"刪除紀錄",3);button(ID_HISTORY_PREV,L"上一頁",3);button(ID_HISTORY_NEXT,L"下一頁",3);label(ID_HISTORY_PAGE,L"尚未讀取",3,2);
 label(ID_PROBE_TITLE,L"LINE 連線檢查",2);label(ID_PROBE_HINT,L"確認目前 LINE 版本與可使用的操作方式，檢查結果不含聊天內容。",2,2);button(ID_PROBE,L"檢查目前 LINE",2,BS_OWNERDRAW);button(ID_REPORT,L"另存檢查結果",2);button(ID_PROFILE,L"載入進階連線設定",2);button(ID_UIA_CONNECT,L"進階：連接 LINE 畫面",2);label(ID_PROBE_NOTE,L"主要保存流程不需連線設定。\n進階畫面操作需適用的 connection.local.json；僅載入不代表已驗證。\n沒有設定時只檢查 LINE 版本，不會列出聊天室。",2,2);
 label(ID_SETTINGS_TITLE,L"保存設定",4);label(ID_CONTENT_LABEL,L"保存的內容",4,2);
 for(auto [id,text]:std::vector<std::pair<int,const wchar_t*>>{{ID_MESSAGES,L"完整訊息與圖片檔案"},{ID_ALBUMS,L"相簿（進階）"},{ID_NOTES,L"記事本（進階）"}}){auto c=button(id,text,4,BS_AUTOCHECKBOX);SendMessageW(c,BM_SETCHECK,id==ID_MESSAGES?BST_CHECKED:BST_UNCHECKED,0);}
 label(ID_RANGE_LABEL,L"訊息期間",4);for(int id:{ID_FROM,ID_UNTIL}){auto c=child(DATETIMEPICK_CLASSW,L"",DTS_SHOWNONE|DTS_SHORTDATEFORMAT,id,4,140,288,180,32);SendMessageW(c,DTM_SETFORMATW,0,reinterpret_cast<LPARAM>(L"yyyy-MM-dd"));SendMessageW(c,DTM_SETSYSTEMTIME,GDT_NONE,0);}label(ID_RANGE_TO,L"至",4);label(ID_TIME_HINT,L"留空會讀取全部可用期間",4,2);
 label(ID_MODE_LABEL,L"文字保存",4);combo(ID_MESSAGE_MODE,4,{L"完整保存訊息（預設）",L"進階：只保存檔案，不新增文字"});label(ID_MODE_HINT,L"預設保留期間內的完整訊息。\n日期只限制本次讀取；已保存的舊訊息不會被刪除。\n只保存檔案是進階選項，仍會讀取檔案所需的識別與關聯；現有文字仍保留。",4,2);
 app.status=child(L"STATIC",L"先登入 LINE，再自動抓取全部，或批次選擇來源與聊天室。",SS_LEFT,0,-1,24,618,832,48);setRole(app.status,2);
 refreshChatSources();activatePage(0);drawChats();layout();
}
bool drawPrimaryButton(const DRAWITEMSTRUCT& item){
 if(item.CtlType!=ODT_BUTTON||!std::set<UINT>{ID_AUTO_CAPTURE,ID_OPEN,ID_PROBE,ID_HISTORY_REFRESH}.count(item.CtlID))return false;
 const bool disabled=(item.itemState&ODS_DISABLED)!=0,pressed=(item.itemState&ODS_SELECTED)!=0;auto brush=CreateSolidBrush(disabled?RGB(225,231,227):pressed?RGB(27,83,68):RGB(35,107,88));
 FillRect(item.hDC,&item.rcItem,brush);DeleteObject(brush);SetBkMode(item.hDC,TRANSPARENT);SetTextColor(item.hDC,disabled?RGB(101,116,108):RGB(255,255,255));auto old=SelectObject(item.hDC,app.font);auto text=getText(item.hwndItem);auto bounds=item.rcItem;DrawTextW(item.hDC,text.c_str(),static_cast<int>(text.size()),&bounds,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 if(item.itemState&ODS_FOCUS){auto focus=item.rcItem;InflateRect(&focus,-scaled(4),-scaled(4));DrawFocusRect(item.hDC,&focus);}SelectObject(item.hDC,old);return true;
}
LRESULT CALLBACK procedure(HWND w,UINT msg,WPARAM wp,LPARAM lp){try{switch(msg){case WM_DRAWITEM:if(drawPrimaryButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp)))return TRUE;break;case WM_NCCREATE:{using Fn=BOOL(WINAPI*)(HWND);auto fn=reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"EnableNonClientDpiScaling"));if(fn)fn(w);break;}case WM_CREATE:create(w);return 0;case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(lp);auto work=workArea(w);info->ptMinTrackSize={std::min(scaled(560),int(work.right-work.left)),std::min(scaled(400),int(work.bottom-work.top))};return 0;}case WM_SIZE:layout();return 0;case WM_DPICHANGED:{app.dpi=HIWORD(wp);rebuildFonts();app.scrollX=app.scrollY=0;auto suggested=*reinterpret_cast<RECT*>(lp);auto work=workArea(w);int width=std::min(int(suggested.right-suggested.left),int(work.right-work.left)),height=std::min(int(suggested.bottom-suggested.top),int(work.bottom-work.top));int x=std::clamp(int(suggested.left),int(work.left),int(work.right)-width),y=std::clamp(int(suggested.top),int(work.top),int(work.bottom)-height);SetWindowPos(w,nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);layout();return 0;}case WM_VSCROLL:scrollView(false,LOWORD(wp));return 0;case WM_HSCROLL:scrollView(true,LOWORD(wp));return 0;case WM_MOUSEWHEEL:app.scrollY-=MulDiv(GET_WHEEL_DELTA_WPARAM(wp),scaled(72),WHEEL_DELTA);layout();return 0;case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED||HIWORD(wp)==CBN_SELCHANGE||((LOWORD(wp)==ID_SEARCH||LOWORD(wp)==ID_FILE_SEARCH)&&HIWORD(wp)==EN_CHANGE))command(LOWORD(wp));return 0;case WM_TIMER:if(wp==2){KillTimer(w,2);if(app.busy)return 0;if(fs::is_regular_file(folderPath()/L"archive.sqlite3"))fileReportClicked(false,0);else drawFileRows();return 0;}break;case WM_NOTIFY:if(reinterpret_cast<NMHDR*>(lp)->idFrom==ID_LIST&&reinterpret_cast<NMHDR*>(lp)->code==LVN_ITEMCHANGED&&!app.drawing){auto change=reinterpret_cast<NMLISTVIEW*>(lp);if(change->iItem>=0&&static_cast<size_t>(change->iItem)<app.shown.size()){auto id=app.shown[change->iItem]["source_id"].get<std::string>();if(ListView_GetCheckState(app.list,change->iItem))app.checked.insert(id);else app.checked.erase(id);if(!app.busy){auto count=wide("已選 "+std::to_string(app.checked.size())+" 個");SetWindowTextW(GetDlgItem(w,ID_SELECTION_COUNT),count.c_str());EnableWindow(GetDlgItem(w,ID_START),!app.checked.empty());setStatus("已選 "+std::to_string(app.checked.size())+" 個聊天室。切換頁面或篩選不會清除選取。");}}}if(reinterpret_cast<NMHDR*>(lp)->idFrom==ID_TABS&&reinterpret_cast<NMHDR*>(lp)->code==TCN_SELCHANGE)activatePage(TabCtrl_GetCurSel(app.tabs));return 0;case MSG_PROGRESS:{std::unique_ptr<CaptureProgress> event(reinterpret_cast<CaptureProgress*>(lp));if(!app.closing)receiveCaptureProgress(*event);return 0;}case MSG_FILE_REPORT:{std::unique_ptr<FileReportResult> result(reinterpret_cast<FileReportResult*>(lp));if(app.closing)return 0;app.completionPage=1;app.fileRoot=std::move(result->root);app.fileReport=std::move(result->data);app.fileOffset=app.fileReport.value("offset",size_t(0));app.fileVerifyContents=wp!=0;app.showProgress=false;auto state=result->state;SendMessageW(GetDlgItem(w,ID_FILE_STATE),CB_SETCURSEL,state=="saved"?1:state=="preview"?2:state=="pending"?3:state=="unavailable"?4:state=="failed"?5:0,0);SetWindowTextW(GetDlgItem(w,ID_FILE_SEARCH),wide(result->query).c_str());KillTimer(w,2);drawFileRows();return 0;}case MSG_PRESERVING:SetWindowTextW(GetDlgItem(w,ID_STOP),L"停止整理閱讀頁");SetWindowTextW(GetDlgItem(w,ID_PAUSE),L"暫停");return 0;case MSG_STATUS:{std::unique_ptr<std::wstring>s(reinterpret_cast<std::wstring*>(lp));SetWindowTextW(app.status,s->c_str());return 0;}case MSG_CHATS:{std::unique_ptr<ConnectionResult>c(reinterpret_cast<ConnectionResult*>(lp));if(app.closing)return 0;if(!c->preserveOptions)app.completionPage=0;app.chats=std::move(c->chats);app.source=std::move(c->source);app.checked.clear();app.chatOffset=0;refreshChatSources();if(nativeDatabaseConnection(app.source)){SendMessageW(GetDlgItem(app.window,ID_MESSAGES),BM_SETCHECK,BST_CHECKED,0);for(int id:{ID_ALBUMS,ID_NOTES})SendMessageW(GetDlgItem(app.window,id),BM_SETCHECK,BST_UNCHECKED,0);if(!c->preserveOptions){for(int id:{ID_FROM,ID_UNTIL})SendMessageW(GetDlgItem(app.window,id),DTM_SETSYSTEMTIME,GDT_NONE,0);SendMessageW(GetDlgItem(app.window,ID_MESSAGE_MODE),CB_SETCURSEL,0,0);}}drawChats();return 0;}case MSG_HISTORY:{std::unique_ptr<HistoryResult> result(reinterpret_cast<HistoryResult*>(lp));if(app.closing)return 0;app.completionPage=3;app.historyRoot=std::move(result->root);app.history=std::move(result->rows);app.historyOffset=result->offset;app.historyTotal=result->total;drawHistory();return 0;}case MSG_LOCATION:{std::unique_ptr<fs::path> path(reinterpret_cast<fs::path*>(lp));if(!app.closing)SetWindowTextW(app.folder,path->c_str());return 0;}case MSG_DONE:{std::unique_ptr<Result> result(reinterpret_cast<Result*>(lp));if(app.worker.joinable())app.worker.join();app.busy=false;app.preserving=false;app.taskError=result->error;taskControls(false);SetWindowTextW(GetDlgItem(w,ID_PAUSE),L"暫停");SetWindowTextW(GetDlgItem(w,ID_STOP),L"停止並保留");SetWindowTextW(app.taskNote,wide(result->message).c_str());auto firstLine=result->message.substr(0,result->message.find('\n'));setStatus((result->stopped?"已停止 · ":result->error?"尚未完成 · ":"完成 · ")+firstLine);if(!app.closing){auto page=result->error?1:app.completionPage;TabCtrl_SetCurSel(app.tabs,page);activatePage(page);auto focused=GetFocus();if(!focused||!app.bases.count(focused)||!IsWindowVisible(focused)||!IsWindowEnabled(focused)){auto target=page==0?app.list:page==3?app.historyList:page==2?GetDlgItem(w,ID_PROBE):app.fileList;if(target&&IsWindowVisible(target)&&IsWindowEnabled(target))SetFocus(target);}revealFocus();}else DestroyWindow(w);return 0;}case WM_CTLCOLORSTATIC:{auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,RGB(33,49,46));SetBkColor(dc,RGB(245,247,245));return reinterpret_cast<LRESULT>(app.background);}case WM_ERASEBKGND:{RECT rect;GetClientRect(w,&rect);FillRect(reinterpret_cast<HDC>(wp),&rect,app.background);return 1;}case WM_CLOSE:if(app.busy){app.closing=true;app.control->stopped=true;app.control->paused=false;app.preserveControl->stopped=true;app.preserveControl->paused=false;EnableWindow(GetDlgItem(w,ID_PAUSE),FALSE);EnableWindow(GetDlgItem(w,ID_STOP),FALSE);setStatus("正在停止，完成的內容仍會保留…");return 0;}DestroyWindow(w);return 0;case WM_DESTROY:app.source.reset();if(app.font)DeleteObject(app.font);if(app.titleFont)DeleteObject(app.titleFont);if(app.smallFont)DeleteObject(app.smallFont);if(app.background)DeleteObject(app.background);PostQuitMessage(0);return 0;}}catch(const Error&e){MessageBoxW(w,wide(e.what()).c_str(),L"請確認",MB_OK|MB_ICONINFORMATION);if(msg==WM_CREATE)return -1;}catch(...){MessageBoxW(w,L"操作尚未完成，請檢查輸入或保存位置。",L"請確認",MB_OK|MB_ICONINFORMATION);if(msg==WM_CREATE)return -1;}return DefWindowProcW(w,msg,wp,lp);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){bool commandLine=false;try{int argc=0;auto args=CommandLineToArgvW(GetCommandLineW(),&argc);if(argc>=3&&(std::wstring(args[1])==L"--demo"||std::wstring(args[1])==L"--check")){commandLine=true;const bool demoMode=std::wstring(args[1])==L"--demo";fs::path target=args[2];LocalFree(args);if(!demoMode&&!fs::is_regular_file(target/L"archive.sqlite3"))throw Error("這個資料夾還沒有可檢查的聊天紀錄。");Store store(target);Control c;if(demoMode){DemoSource demo;auto result=collect(store,demo,demo.chats(),c);std::string warning;exportHtml(store,{},&warning);auto out=result.dump()+"\n";DWORD n;if(!warning.empty()){warning+="\n";WriteFile(GetStdHandle(STD_ERROR_HANDLE),warning.data(),static_cast<DWORD>(warning.size()),&n,nullptr);}WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),out.data(),out.size(),&n,nullptr);}else{auto issues=store.check();auto out=issues.dump()+"\n";DWORD n;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),out.data(),out.size(),&n,nullptr);return issues.empty()?0:1;}return 0;}LocalFree(args);initializeDpi();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX controls{sizeof controls,ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES|ICC_DATE_CLASSES};InitCommonControlsEx(&controls);WNDCLASSEXW wc{};wc.cbSize=sizeof wc;wc.hInstance=instance;wc.lpfnWndProc=procedure;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));if(!wc.hIcon)wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=L"ChatAtlasNativeWindow";RegisterClassExW(&wc);app.dpi=windowDpi(nullptr);auto work=workArea(nullptr);int windowWidth=std::min(scaled(1160),int(work.right-work.left)),windowHeight=std::min(scaled(860),int(work.bottom-work.top));auto w=CreateWindowExW(0,wc.lpszClassName,L"聊藏 · LINE 對話知識地圖",mainWindowStyle,work.left+(work.right-work.left-windowWidth)/2,work.top+(work.bottom-work.top-windowHeight)/2,windowWidth,windowHeight,nullptr,nullptr,instance,nullptr);if(!w)throw Error("程式無法啟動。");ShowWindow(w,show);UpdateWindow(w);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){auto previousFocus=GetFocus();if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(GetFocus()!=previousFocus)revealFocus();}CoUninitialize();return 0;}catch(const Error&e){if(commandLine){auto out=std::string(e.what())+"\n";DWORD n;WriteFile(GetStdHandle(STD_ERROR_HANDLE),out.data(),out.size(),&n,nullptr);}else MessageBoxW(nullptr,wide(e.what()).c_str(),L"聊藏",MB_OK|MB_ICONERROR);return 2;}catch(...){if(commandLine){const char*message="Operation failed. No installer or dependency download was started.\n";DWORD n;WriteFile(GetStdHandle(STD_ERROR_HANDLE),message,static_cast<DWORD>(strlen(message)),&n,nullptr);}else MessageBoxW(nullptr,L"程式無法啟動，請保留整個程式包並確認使用 Windows 10 或更新版本。",L"聊藏",MB_OK|MB_ICONERROR);return 2;}}
#endif
