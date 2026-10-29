#include "private_path.hpp"
#include <iostream>
#include <stdexcept>
using namespace atlas;
static unsigned checks=0;
static void expect(bool ok){++checks;if(!ok)throw std::runtime_error("Private path policy contract failed");}
int main(){
 for(auto p:{LR"(C:\)",LR"(D:\new\missing)",LR"(c:\space name\中文)",LR"(Z:\mapped)",LR"(\\?\C:\long\name)",LR"(\\?\z:\)",L"C:/folder",L"//?/C:/folder"})expect(privatePathKind(p)==PrivatePathKind::drive);
 // Drive spelling alone is NOT evidence that a mapped drive is local.
 for(auto p:{LR"(\\server\share)",LR"(\\server\share\folder)",LR"(\\?\UNC\server\share\folder)",LR"(\\?\unc\server\share)",LR"(\\?\UnC\server\share)",LR"(\\server)",L"//server/share",L"//?/uNc/server/share"})expect(privatePathKind(p)==PrivatePathKind::remote);
 for(auto p:{L"",L"relative",L"C:relative",L"\\rooted",LR"(\\.\C:\folder)",LR"(\??\C:\folder)",LR"(\\?\GLOBALROOT\Device\Mup\server\share)",LR"(\\?\Volume{00000000-0000-0000-0000-000000000000}\folder)",LR"(\\?\PIPE\pipe)",LR"(\\?\UNC)",L"1:\\folder",L"C:",LR"(\\?\C:relative)"})expect(privatePathKind(p)==PrivatePathKind::unsupported);
 expect(privatePathKind(std::wstring(L"C:\\ok\0\\hidden",13))==PrivatePathKind::unsupported);
 for(unsigned type=0;type<10;++type)expect(localPrivateDriveType(type)==(type==2||type==3||type==5||type==6));
 std::cout<<"Private path spelling/drive policy: "<<checks<<" passed (not a live SMB test)\n";
}
