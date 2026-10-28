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
 auto originalConfig=app.config;requireUi(originalConfig.filename()==L"ChatAtlasData"&&originalConfig.parent_path()==executableDirectory(),"DPI changes preserve portable data location");
 DestroyWindow(window);CoUninitialize();std::cout<<checks<<" Win32 synthetic DPI checks passed; real multi-monitor and OS-version matrix remain separate.\n";return 0;
 }catch(const std::exception&e){if(app.window&&IsWindow(app.window))DestroyWindow(app.window);std::cerr<<e.what()<<"\n";return 1;}}
#else
int main(){std::cerr<<"This test requires Windows.\n";return 2;}
#endif
