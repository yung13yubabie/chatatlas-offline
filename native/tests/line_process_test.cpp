#include "line_process.hpp"
#include <iostream>
#include <stdexcept>
using namespace atlas;
namespace {
int checks=0;
void require(bool valid,const char* message){++checks;if(!valid)throw std::runtime_error(message);}
template<class F>void rejects(F fn){bool failed=false;try{fn();}catch(const Error&){failed=true;}require(failed,"invalid input is rejected");}
const std::string mixed="aB12cD34eF56aB78cD90eF12aB34cD56";
const std::string second="0123456789abcdef0123456789abcdef";
void append(LineKeyCandidates& candidates,const std::string& text,bool final=true){candidates.append(reinterpret_cast<const unsigned char*>(text.data()),text.size(),final);}
void append(LineSessionCandidates& candidates,const std::string& text,bool final=true){candidates.append(reinterpret_cast<const unsigned char*>(text.data()),text.size(),final);}
std::string utf16(const std::string& text){std::string out;for(char c:text){out+=c;out+='\0';}return out;}
}
int main(){try{
 Control control;
 {
  LineKeyCandidates candidates;append(candidates,"!"+mixed+"!");require(candidates.size()==1,"ASCII key recognized");
  auto result=candidates.findMatch(control,[&](const std::string& key){return key==mixed;});require(result==mixed,"case-sensitive passphrase retained");eraseLineKey(result);require(result.empty(),"caller-owned key erased");
 }
 {
  LineKeyCandidates candidates;append(candidates,utf16("!"+mixed+"!"));require(candidates.size()==1,"UTF16 key recognized");require(candidates.findMatch(control,[&](const std::string& key){return key==mixed;})==mixed,"UTF16 retains original case");
 }
 for(size_t split=0;split<72;++split){
  auto text=" !"+utf16(mixed)+"! ";LineKeyCandidates candidates;const auto at=std::min(split,text.size());
  append(candidates,text.substr(0,at),false);append(candidates,text.substr(at),true);
  require(candidates.size()==1,"UTF16 key survives every split without duplication");
  require(candidates.findMatch(control,[&](const std::string& key){return key==mixed;})==mixed,"split key matches exact passphrase");
 }
 for(size_t split=0;split<38;++split){
  auto text="!"+mixed+"!";LineKeyCandidates candidates;const auto at=std::min(split,text.size());append(candidates,text.substr(0,at),false);append(candidates,text.substr(at),true);
  require(candidates.size()==1,"ASCII key survives every chunk split");
 }
 {
  LineKeyCandidates candidates;auto text="!"+second+"!"+mixed+"!"+mixed+"!";append(candidates,text);
  std::vector<bool> order;candidates.findMatch(control,[&](const std::string& key){order.push_back(key==mixed);return false;});require(order==std::vector<bool>({true,false}),"most frequent candidate tested first");
 }
 {
  // Overlap bytes must not inflate the frequency of a boundary candidate.
  LineKeyCandidates candidates;auto text="!"+second+"!"+mixed+"!";
  for(size_t at=0;at<text.size();++at)append(candidates,text.substr(at,1),false);
  candidates.append(nullptr,0,true);append(candidates,"!"+second+"!");
  std::vector<bool> order;candidates.findMatch(control,[&](const std::string& key){order.push_back(key==second);return false;});require(order==std::vector<bool>({true,false}),"streaming overlap does not count candidates twice");
 }
 {
  LineKeyCandidates candidates;append(candidates,"A"+mixed+"A !"+mixed+"q !"+std::string(64,'a')+"!");require(candidates.size()==0,"partial hashes and alphanumeric neighbors rejected");
  append(candidates,utf16("A"+mixed+"A !"+mixed+"q !"+std::string(64,'a')+"!"));require(candidates.size()==0,"UTF16 boundary checks reject partial hashes");
 }
 {
  LineKeyCandidates candidates;append(candidates,mixed.substr(0,16));append(candidates,mixed.substr(16));require(candidates.size()==0,"separate memory regions do not concatenate");
  append(candidates,mixed);require(candidates.size()==1,"region edges support complete keys");
 }
 {
  LineKeyCandidates candidates(1);append(candidates,mixed);rejects([&]{append(candidates,second);});require(candidates.size()==1,"candidate limit retains bounded storage");
  LineKeyCandidates empty;rejects([&]{empty.append(nullptr,1);});rejects([&]{LineKeyCandidates invalid(0);});
 }
 {
  LineKeyCandidates candidates;append(candidates,mixed);Control stopped;stopped.stopped=true;bool probed=false;
  rejects([&]{candidates.findMatch(stopped,[&](const std::string&){probed=true;return true;});});require(!probed,"cancellation occurs before probing");
  bool ticked=false;auto match=candidates.findMatch(control,[&](const std::string&){return true;},[&]{ticked=true;});require(ticked&&match==mixed,"caller deadline checked around probe");eraseLineKey(match);
  rejects([&]{candidates.findMatch(control,{});});
  Control cancelled;rejects([&]{candidates.findMatch(cancelled,[&](const std::string&){cancelled.stopped=true;return true;});});
  bool scanned=false;rejects([&]{findLineDatabaseKey(path8("missing-synthetic-database.edb"),stopped,[&](const std::string&){scanned=true;return true;});});require(!scanned,"already cancelled native scan reads no process or database");
 }
 {
  // Batch connection reuses the one process's pool for different databases;
  // a failed or successful probe must not consume another database's key.
  LineKeyCandidates process;append(process,"!"+mixed+"!"+second+"!");
  auto first=process.findMatch(control,[&](const std::string& key){return key==mixed;});
  auto failed=process.findMatch(control,[](const std::string&){return false;});
  auto later=process.findMatch(control,[&](const std::string& key){return key==second;});
  require(first==mixed&&failed.empty()&&later==second,"one scanned pool supports matching multiple explicit database probes");
  require(process.size()==2,"batch matching does not consume or append process candidates");
  eraseLineKey(first);eraseLineKey(later);
  LineKeyCandidates otherProcess;append(otherProcess,mixed);
  auto absent=otherProcess.findMatch(control,[&](const std::string& key){return key==second;});
  require(absent.empty()&&otherProcess.size()==1,"another process cannot inherit a previous process's database key");
  rejects([&]{process.findMatch(control,[](const std::string&)->bool{throw CapabilityError("synthetic probe failure");});});
  auto retry=process.findMatch(control,[&](const std::string& key){return key==second;});
  require(retry==second,"failed database probe does not corrupt the reusable process pool");eraseLineKey(retry);
  bool laterProbed=false;Control cancelled;cancelled.stopped=true;
  rejects([&]{process.findMatch(cancelled,[&](const std::string&){laterProbed=true;return true;});});
  require(!laterProbed,"batch cancellation prevents the next database's key probe");
 }
 {
  // These preconditions run before any native process access, on every OS.
  rejects([&]{visitLineDatabaseKeys(control,{});});
  Control stopped;stopped.stopped=true;bool visited=false,reported=false;
  rejects([&]{visitLineDatabaseKeys(stopped,[&](LineKeyCandidates&,std::shared_ptr<LineSession>,Tick){visited=true;return false;},[&](std::string){reported=true;});});
  require(!visited&&!reported,"an already cancelled batch scan cannot read processes or call its visitor");
 }
 const std::string auth="synthetic-account-session-token",obs="synthetic-observed-obs-token",application="DESKTOPWIN\t1.2.3\tWINDOWS\t10";
 const auto headers="GET /r/talk/m/synthetic HTTP/1.1\r\nHost: obs.line-apps.com\r\nX-Line-Access: "+obs+"\r\nX-Line-Application: "+application+"\r\n\r\n";
 const auto account=std::string("{\"authToken\":\"")+auth+"\",\"X-Line-Application\":\"DESKTOPWIN\\t1.2.3\\tWINDOWS\\t10\"}";
 {
  LineSessionCandidates candidates;append(candidates,headers);auto session=candidates.preferred();require(session&&session->obsAccessToken()==obs&&session->accessToken().empty(),"observed OBS HTTP token is distinguished from account auth");require(session->application()==application,"actual application header retained without inventing a version");require(!session->current(),"synthetic credential contexts cannot authorize network access without a live signed process binding");
 }
 {
  LineSessionCandidates candidates;append(candidates,account);auto session=candidates.preferred();require(session&&session->accessToken()==auth&&session->obsAccessToken().empty(),"labelled JSON auth remains account auth, not an OBS token");require(session->application()==application,"escaped JSON tabs retained in actual application descriptor");
 }
 for(const auto& context:{headers,account})for(bool wide:{false,true}){
  auto bytes=wide?utf16(context):context;
  for(size_t split=0;split<=bytes.size();++split){
   LineSessionCandidates candidates;append(candidates,bytes.substr(0,split),false);append(candidates,bytes.substr(split),true);auto session=candidates.preferred();
   require(session&&session->application()==application,"HTTP and JSON session labels survive every ASCII and UTF16 chunk split");
   require(context==headers?session->obsAccessToken()==obs:session->accessToken()==auth,"session token survives every chunk boundary without type changes");
  }
 }
 {
  LineSessionCandidates candidates;append(candidates,"random-unlabelled-synthetic-token");require(!candidates.preferred(),"random memory strings are never guessed to be tokens");
  append(candidates,"{\"authToken\":\"unterminated");require(!candidates.preferred(),"incomplete labelled contexts do not yield credentials");
  append(candidates,"{\"chatText\":\"\\\"authToken\\\":\\\"synthetic-chat-data\\\"\"}");require(!candidates.preferred(),"escaped chat text is not interpreted as session context");
  append(candidates,"{\"authToken\":\"bad\\r\\nheader\"}");require(!candidates.preferred(),"header-breaking credentials are refused");
 }
 {
  LineSessionCandidates candidates;auto other=headers;other.replace(other.find("obs.line-apps.com"),17,"obs.line-apps.com.evil.invalid");append(candidates,other);auto session=candidates.preferred();require(!session||session->obsAccessToken().empty(),"lookalike OBS hosts cannot designate an OBS credential");
  LineSessionCandidates isolated;append(isolated,"Host: obs.line-apps.com\r\n\r\nX-Line-Access: synthetic-after-blank\r\n");session=isolated.preferred();require(!session||session->obsAccessToken().empty(),"OBS hostname from another HTTP block cannot bind a token");
  LineSessionCandidates json;append(json,"{\"host\":\"obs.line-apps.com\",\"X-Line-Access\":\"synthetic-json-obs\"}");session=json.preferred();require(session&&session->obsAccessToken()=="synthetic-json-obs","explicit JSON request header context supports OBS token capture");
 }
 {
  LineSessionCandidates candidates;for(int n=0;n<64;++n)append(candidates,"{\"authToken\":\"synthetic-token-"+std::to_string(n)+"\"}");rejects([&]{append(candidates,"{\"authToken\":\"synthetic-one-too-many\"}");});
  rejects([&]{candidates.append(nullptr,1);});
 }
 std::cout<<checks<<" native LINE key recognizer checks passed; no live process or key data used.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Native LINE process test failed: "<<e.what()<<"\n";return 1;}}
