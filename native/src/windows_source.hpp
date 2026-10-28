#pragma once
#include "core.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
namespace atlas {
std::wstring wide(std::string_view);std::string narrow(std::wstring_view);
class Bridge{HANDLE process=nullptr,input=nullptr,output=nullptr;std::string buffered;public:Bridge();~Bridge();Bridge(const Bridge&)=delete;J call(std::string,J=J::object(),Control*control=nullptr);void close();};
class WindowsSource:public Source{J profile;Bridge bridge;Control* requestControl=nullptr;J request(std::string action,J values=J::object()){return bridge.call(std::move(action),std::move(values),requestControl);};std::map<std::string,std::string>tokens;std::map<std::string,J>downloads;J find(const J&,const std::string&root="");J single(J);std::string field(const J&,const J&);void assertAccount();void assertChat(const J&);void select(const J&,Control&);void panel(const J&,bool open,Control&);public:explicit WindowsSource(J,Control* control=nullptr);J chats()override;J chatsWithControl(Control&)override;Batch read(const J&,const std::set<std::string>&,const std::string&,const std::string&,Control&)override;fs::path download(const J&,const J&,const fs::path&,Control&)override;static J validate(J);};
J probe(Control* control=nullptr);
}
#endif
