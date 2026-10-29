#include "line_database.hpp"
#include "line_cipher.hpp"
#include "line_media.hpp"
#include "file.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <limits>
#include <utility>

namespace atlas {
// Keep a read-only filesystem identity anchor alive for the SQLite connection.
// Ordinary LINE writes preserve this identity; replacing the file does not.
class LineDatabaseIdentity {
#ifdef _WIN32
 HANDLE handle=INVALID_HANDLE_VALUE;
#else
 int handle=-1;
#endif
 public:
 std::string value;
 explicit LineDatabaseIdentity(const fs::path& path){
  noLinks(path);
#ifdef _WIN32
  auto expected=expectedFilePath(path);
  handle=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
  if(handle==INVALID_HANDLE_VALUE)throw CapabilityError("無法確認所選 LINE 資料庫的檔案識別。");
  try{verifyFileHandle(handle,expected);BY_HANDLE_FILE_INFORMATION info{};
   if(GetFileType(handle)!=FILE_TYPE_DISK||!GetFileInformationByHandle(handle,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||(!info.nFileIndexHigh&&!info.nFileIndexLow))throw CapabilityError("無法確認所選 LINE 資料庫的檔案識別。");
   value=std::to_string(info.dwVolumeSerialNumber)+":"+std::to_string(info.nFileIndexHigh)+":"+std::to_string(info.nFileIndexLow);
  }catch(...){CloseHandle(handle);handle=INVALID_HANDLE_VALUE;throw;}
#else
  handle=::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK);struct stat info{};
  if(handle<0)throw CapabilityError("無法確認所選 LINE 資料庫的檔案識別。");
  if(fstat(handle,&info)||!S_ISREG(info.st_mode)){::close(handle);handle=-1;throw CapabilityError("無法確認所選 LINE 資料庫的檔案識別。");}
  value=std::to_string(static_cast<uintmax_t>(info.st_dev))+":"+std::to_string(static_cast<uintmax_t>(info.st_ino));
#endif
 }
 ~LineDatabaseIdentity(){
#ifdef _WIN32
  if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);
#else
  if(handle>=0)::close(handle);
#endif
 }
 LineDatabaseIdentity(const LineDatabaseIdentity&)=delete;
};
namespace {
constexpr size_t pageSize=500,maxMessages=200000;
constexpr uint64_t maxMessageBytes=128ULL*1024*1024;
constexpr int64_t lastTime=std::chrono::duration_cast<std::chrono::milliseconds>(
 std::chrono::sys_days{std::chrono::year{3000}/std::chrono::January/1}.time_since_epoch()).count();
std::string text(const J& value,const char* name,size_t max=4*1024*1024){
 if(!value.contains(name)||value[name].is_null())return {};
 if(!value[name].is_string())throw CapabilityError("LINE 資料庫欄位型別不相容。");
 const auto& result=value[name].get_ref<const std::string&>();
 if(result.size()>max||result.find('\0')!=std::string::npos)throw CapabilityError("LINE 單一欄位過大或含無法保存的字元。");
 try{J(result).dump();}catch(const J::exception&){throw CapabilityError("LINE 文字不是有效的 UTF-8，未更新收藏。");}
 return result;
}
std::string id(const J& row,const char* field){
 auto result=text(row,field,1024);
 if(result.empty()||std::any_of(result.begin(),result.end(),[](unsigned char c){return c<32||c==127;}))throw CapabilityError("LINE 資料庫缺少穩定識別。");
 return result;
}
std::string lowerAscii(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return value;}
bool lineIdentifier(const std::string& value){
 if(value.size()!=33||std::string("ucrsp").find(lowerAscii(value.substr(0,1)))==std::string::npos)return false;
 return std::all_of(value.begin()+1,value.end(),[](unsigned char c){return std::isxdigit(c)!=0;});
}
bool privateField(const std::string& field){
 auto name=lowerAscii(field);name.erase(std::remove_if(name.begin(),name.end(),[](unsigned char c){return c=='_'||c=='-'||c==' ';}),name.end());
 static const std::set<std::string> protectedNames={"keymaterial","enckm","authtoken","accesstoken","refreshtoken","xlineaccess","authorization","password","privatekey","encryptionkey","e2eekey","sharedsecret"};
 return protectedNames.count(name);
}
bool privatePayload(const J& value){
 if(value.is_object()){for(auto item=value.begin();item!=value.end();++item)if(privateField(item.key())||privatePayload(item.value()))return true;}
 else if(value.is_array())for(const auto& item:value)if(privatePayload(item))return true;
 return false;
}
bool embedsPrivateValue(const J&,const std::string&);
bool privateTextMarker(const std::string& value){
 const auto lower=lowerAscii(value);for(auto marker:{"keymaterial","enc_km","authtoken","access_token","x-line-access","authorization","privatekey","encryptionkey"})if(lower.find(marker)!=std::string::npos)return true;return false;
}
std::string labelField(std::string value){value=lowerAscii(std::move(value));value.erase(std::remove_if(value.begin(),value.end(),[](unsigned char c){return c=='_'||c=='-';}),value.end());return value;}
const J* profileField(const J& value,const std::string& field){for(auto current=value.begin();current!=value.end();++current)if(labelField(current.key())==field)return &current.value();return nullptr;}
std::string unnamed(const std::string& label,size_t number){return number?label+"（"+std::to_string(number)+"）":label;}
std::string cleanName(std::string value,const std::string& source){
 if(value.empty()||value.size()>4096||value.find('\0')!=std::string::npos)return {};
 try{J(value).dump();}catch(const J::exception&){return {};}
 for(auto& c:value)if(static_cast<unsigned char>(c)<32||c==127)c=' ';
 // LINE permits local aliases; whitespace-only aliases must fall back to the
 // synced name instead of hiding it. Trim common Unicode spacer characters too.
 const std::vector<std::string> spacers={" ","\xC2\xA0","\xE3\x80\x80","\xE2\x80\x8B","\xEF\xBB\xBF"};
 bool changed=true;while(changed&&!value.empty()){changed=false;for(const auto& part:spacers){if(value.starts_with(part)){value.erase(0,part.size());changed=true;}if(value.ends_with(part)){value.resize(value.size()-part.size());changed=true;}}}
 if(value.empty()||value==source||lineIdentifier(value))return {};
 if(value.front()=='{'||value.front()=='['){
  // Some caches serialize a display-name record. Read only explicit label
  // fields; never treat arbitrary profile JSON or credential payload as a name.
  auto record=J::parse(value,[](int depth,J::parse_event_t,J&){if(depth>8)throw CapabilityError("LINE 名稱資訊過深。");return true;},false);
  if(record.is_object()){
   for(auto field=record.begin();field!=record.end();++field){const auto name=labelField(field.key());if((name=="mid"||name=="membermid"||name=="squaremembermid")&&!field.value().is_null()&&(!field.value().is_string()||(!field.value().get_ref<const std::string&>().empty()&&field.value()!=source)))return {};}
   for(auto field:{"displaynameoverridden","displayname","name","nickname","title"})if(const auto* found=profileField(record,field);found&&found->is_string()){
    auto candidate=found->get<std::string>();if(candidate!=value&&!candidate.empty()&&!embedsPrivateValue(record,candidate)&&!privateTextMarker(candidate)&&candidate.front()!='{'&&candidate.front()!='['){auto result=cleanName(std::move(candidate),source);if(!result.empty())return result;}
   }
   for(auto field:{"profile","contact","member"})if(const auto* found=profileField(record,field);found&&found->is_object()){auto result=cleanName(found->dump(),source);if(!result.empty()&&!embedsPrivateValue(record,result))return result;}
  }
  if(!record.is_discarded())return {};
 }
 return value;
}
std::string pickName(const J& row,const std::vector<std::string>& fields,const std::string& source){
 for(const auto& field:fields)if(row.contains(field)&&row[field].is_string()){
  try{auto raw=row[field].get<std::string>();if(raw.size()>4096)continue;if(lowerAscii(field).find("profile")!=std::string::npos){auto record=J::parse(raw,[](int depth,J::parse_event_t,J&){if(depth>8)throw CapabilityError("LINE 名稱資訊過深。");return true;},false);if(!record.is_object())continue;raw=record.dump();}auto value=cleanName(std::move(raw),source);if(!value.empty())return value;}catch(const Error&){/* Optional metadata must not hide messages. */}
 }
 return {};
}
const std::vector<std::string> contactNameFields={"_displayNameOverridden","displayNameOverridden","_displayName","displayName","_name","name","_nickname","nickname","_profile","profile","_profileInfo","profileInfo"};
const std::vector<std::string> memberNameFields={"_displayName","displayName","_name","name","_nickname","nickname","_profile","profile","_profileInfo","profileInfo"};
const std::vector<std::string> chatNameFields={"_displayNameOverridden","displayNameOverridden","_chatName","chatName","_displayName","displayName","_name","name","_title","title"};
std::string fallbackChatTitle(const std::string& kind,size_t number){
 const auto label=kind=="group"?"未命名群組":kind=="room"?"未命名多人對話":kind=="community"?"未命名社群":"未命名對話";
 return unnamed(label,number);
}
int64_t integer(const J& row,const char* field){
 if(!row.contains(field)||!row[field].is_number_integer()||
    (row[field].is_number_unsigned()&&row[field].get<uint64_t>()>uint64_t(INT64_MAX)))throw CapabilityError("LINE 整數欄位不相容。");
 return row[field].get<int64_t>();
}
std::string timestamp(int64_t milliseconds){
 if(milliseconds<0||milliseconds>=lastTime||!std::in_range<time_t>(milliseconds/1000))throw CapabilityError("LINE 訊息時間超出支援範圍。");
 time_t seconds=static_cast<time_t>(milliseconds/1000);std::tm utc{};
#ifdef _WIN32
 if(gmtime_s(&utc,&seconds))throw CapabilityError("LINE 訊息時間無法轉換。");
#else
 if(!gmtime_r(&seconds,&utc))throw CapabilityError("LINE 訊息時間無法轉換。");
#endif
 char whole[32]{};if(std::strftime(whole,sizeof whole,"%Y-%m-%dT%H:%M:%S",&utc)!=19)throw CapabilityError("LINE 訊息時間無法格式化。");
 char fraction[16]{};std::snprintf(fraction,sizeof fraction,".%03dZ",int(milliseconds%1000));
 return normalizeTime(std::string(whole)+fraction);
}
int64_t boundary(const std::string& value,int64_t fallback){
 if(value.empty())return fallback;
 auto normalized=normalizeTime(value);
 // Source timestamps are integer milliseconds. Do not round a requested scope.
 auto dot=value.find('.',19);
 if(dot!=std::string::npos){size_t at=dot+1;for(size_t digits=0;at<value.size()&&value[at]>='0'&&value[at]<='9';++at,++digits)if(digits>=3&&value[at]!='0')throw CapabilityError("LINE 讀取期間請使用毫秒或更低精度。");}
 unsigned y,mo,d,h,m,s,ms;
 if(std::sscanf(normalized.c_str(),"%u-%u-%uT%u:%u:%u.%u",&y,&mo,&d,&h,&m,&s,&ms)!=7)throw CapabilityError("LINE 讀取期間不正確。");
 const auto days=std::chrono::sys_days{std::chrono::year{int(y)}/std::chrono::month{mo}/std::chrono::day{d}};
 return std::chrono::duration_cast<std::chrono::milliseconds>(days.time_since_epoch()).count()+int64_t(h)*3600000+int64_t(m)*60000+int64_t(s)*1000+ms;
}
class Snapshot {
 LineCipher& db;bool active=true;
 public:
 explicit Snapshot(LineCipher& value):db(value){db.rows("BEGIN");}
 ~Snapshot(){if(active)try{db.rows("ROLLBACK");}catch(...) {}}
 void commit(){db.rows("COMMIT");active=false;}
};
J metadata(const J& raw){
 auto encoded=text(raw,"_contentMetadata");
 if(encoded.empty())return J::object();
 auto value=J::parse(encoded,[](int depth,J::parse_event_t,J&){if(depth>32)throw CapabilityError("LINE 訊息資訊巢狀層數過多，未繼續整理。");return true;},false);
 return value.is_object()?value:J::object();
}
std::string publicMessageBody(const J& row,int64_t type){
 auto raw=text(row,"_text");if(raw.empty())return {};
 auto first=raw.find_first_not_of(" \t\r\n");if(first==std::string::npos)return {};
 if(raw[first]=='{'||raw[first]=='['){
  auto payload=J::parse(raw,[](int depth,J::parse_event_t,J&){if(depth>32)throw CapabilityError("LINE 訊息內容巢狀層數過多。");return true;},false);
  if(!payload.is_discarded()){
   if(payload.is_object()){
    std::string content;std::set<std::string> seen;
    for(auto field:{"text","message","displayText","caption","description","title"})if(payload.contains(field)&&payload[field].is_string()){
     auto value=payload[field].get<std::string>();if(!value.empty()&&!embedsPrivateValue(payload,value)&&!privateTextMarker(value)&&seen.insert(value).second){if(!content.empty())content+='\n';content+=value;}
    }
    if(!content.empty())return content;
   }
   if(privatePayload(payload)||privateTextMarker(raw))return {};
   // Unrecognized but non-secret content remains available instead of silently
   // discarding an entire system or rich message.
   return raw;
  }
 }
 if(privateTextMarker(raw))return {};
 const auto last=raw.find_last_not_of(" \t\r\n");auto bounded=raw.substr(first,last-first+1);
 if((type==1||type==2||type==3||type==7||type==14||type==21)&&bounded.size()>=32&&std::all_of(bounded.begin(),bounded.end(),[](unsigned char c){return std::isalnum(c)||c=='+'||c=='/'||c=='='||c=='_'||c=='-';}))return {};
 return raw;
}
bool embedsPrivateValue(const J& value,const std::string& visible){
 if(value.is_object()){for(auto field=value.begin();field!=value.end();++field){if(privateField(field.key())&&field.value().is_string()){const auto& secret=field.value().get_ref<const std::string&>();if(!secret.empty()&&visible.find(secret)!=std::string::npos)return true;}if(embedsPrivateValue(field.value(),visible))return true;}}
 else if(value.is_array())for(const auto& item:value)if(embedsPrivateValue(item,visible))return true;
 return false;
}
std::string messageText(const J& row,int64_t type){
 if(type==0)return text(row,"_text");
 std::string label;
 switch(type){case 1:case 21:label="圖片";break;case 2:label="影片";break;case 3:label="音訊";break;case 6:label="通話";break;case 7:label="貼圖";break;case 12:label="連結";break;case 13:label="聯絡人";break;case 14:label="檔案";break;case 15:label="位置";break;case 16:label="動態通知";break;case 17:case 22:label="圖文訊息";break;case 18:label="系統訊息";break;case 19:label="音樂";break;case 20:label="付款訊息";break;default:label="其他訊息";}
 // E2EENext transports per-file keyMaterial inside non-text _text JSON.
 // Preserve readable message text while excluding transport credentials.
 auto result="["+label+"]";std::set<std::string> added;auto meta=metadata(row);
 const auto append=[&](const std::string& value){if(!value.empty()&&!privateTextMarker(value)&&!embedsPrivateValue(meta,value)&&added.insert(value).second)result+="\n"+value;};
 if(type==14){for(auto name:{"FILE_NAME","fileName"}){auto filename=text(meta,name);if(!filename.empty()){append(filename);break;}}}
 if(type==12||type==16)for(auto name:{"title","url","linkUrl","desc"})append(text(meta,name));
 if(type==18)for(auto name:{"text","message","displayText","description"})append(text(meta,name));
 append(publicMessageBody(row,type));
 return result;
}
}

LineDatabaseSource::LineDatabaseSource(const fs::path& selected,const std::string& passphrase,fs::path mediaRoot,Control* requested)
 :selectedDatabase(absoluteNormalizedPath(selected)){
 Control fallback;auto& control=requested?*requested:fallback;control.check();noLinks(selectedDatabase);
 identity=std::make_unique<LineDatabaseIdentity>(selectedDatabase);
 account="line-db:"+key({utf8(selectedDatabase),identity->value});
 database=std::make_unique<LineCipher>(selectedDatabase,passphrase,&control);
 media=std::make_unique<LineMediaResolver>(selectedDatabase,std::move(mediaRoot));
 verifyIdentity();Snapshot snapshot(*database);inspectSchema(control);loadChats(control);snapshot.commit();verifyIdentity();control.check();
}
LineDatabaseSource::~LineDatabaseSource()=default;
void LineDatabaseSource::verifyIdentity()const{
 LineDatabaseIdentity current(selectedDatabase);
 if(!identity||current.value!=identity->value)throw CapabilityError("所選 LINE 資料庫檔案已被替換，請重新連接；未混合不同來源。");
}
bool LineDatabaseSource::hasTable(const std::string& name)const{return tables.count(name);}
bool LineDatabaseSource::hasColumn(const std::string& table,const std::string& column)const{
 auto found=tables.find(table);if(found==tables.end())return false;
 const auto wanted=lowerAscii(column);return std::any_of(found->second.begin(),found->second.end(),[&](const auto& value){return lowerAscii(value)==wanted;});
}
J LineDatabaseSource::nameRows(const std::string& table,const std::string& identityColumn,const std::vector<std::string>& fields,Control& control){
 if(!hasTable(table)||!hasColumn(table,identityColumn))return J::array();
 const auto column=[](const std::string& name,size_t limit){const auto quoted="\""+name+"\"";return "CASE WHEN typeof("+quoted+")='text' AND length(CAST("+quoted+" AS BLOB))<="+std::to_string(limit)+" THEN "+quoted+" END AS "+quoted;};
 std::string projection=column(identityColumn,1024);std::set<std::string> selected{lowerAscii(identityColumn)};
 for(const auto& field:fields)if(hasColumn(table,field)&&selected.insert(lowerAscii(field)).second)projection+=","+column(field,4096);
 return database->rows("SELECT "+projection+" FROM \""+table+"\"",J::array(),[&]{control.check();});
}
void LineDatabaseSource::inspectSchema(Control& control){
 auto tick=[&]{control.check();};
 const std::set<std::string> supported={"_chat","_message","_contact","_profile","_groupChat","_room","_squareChat","_squareMember","_squareThread"};
 for(auto& row:database->rows("SELECT name,type,sql FROM sqlite_master WHERE name LIKE '\\_%' ESCAPE '\\'",J::array(),tick)){
  auto rawName=text(row,"name",128);auto found=std::find_if(supported.begin(),supported.end(),[&](const auto& candidate){return lowerAscii(candidate)==lowerAscii(rawName);});if(found==supported.end())continue;const auto name=*found;
  if(text(row,"type",32)!="table"){if(name=="_message"||name=="_squareThread")throw CapabilityError("LINE 所需來源不是一般資料表。");continue;}
  auto definition=text(row,"sql");std::string upper=definition;std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char c){return char(std::toupper(c));});
  if(upper.find("VIRTUAL")!=std::string::npos){if(name=="_message"||name=="_squareThread")throw CapabilityError("LINE 來源資料表格式尚未支援。");continue;}
  auto info=database->rows("PRAGMA table_info(\""+name+"\")",J::array(),tick);std::set<std::string> columns,primary;
  for(auto& column:info){columns.insert(text(column,"name",128));if(integer(column,"pk"))primary.insert(text(column,"name",128));}
  if((name=="_message"&&primary!=std::set<std::string>{"_id"})||(name=="_squareThread"&&primary!=std::set<std::string>{"_threadMid"}))throw CapabilityError("LINE 訊息或討論串識別格式不相容。");
  tables.emplace(name,std::move(columns));
 }
 const std::map<std::string,std::set<std::string>> required={
  {"_message",{"_id","_chatId","_from","_createdTime","_text","_contentType","_contentMetadata"}},
  {"_squareThread",{"_threadMid","_chatMid","_messageId"}}};
 if(!hasTable("_message"))throw CapabilityError("這不是支援的 LINE 訊息資料庫，沒有讀取其他資料庫或帳號。");
 for(auto& [table,fields]:required)if(hasTable(table))for(auto& field:fields)if(!tables[table].count(field))throw CapabilityError("LINE 資料庫欄位與目前支援的格式不相容。");
 // Display metadata varies between LINE builds. Missing optional label fields
 // do not make the message history unreadable; ignore only a table whose own
 // identity column cannot be recognized and use a human-readable fallback.
 for(const auto& [table,field]:std::map<std::string,std::string>{{"_chat","_id"},{"_contact","_mid"},{"_profile","_mid"},{"_groupChat","_chatMid"},{"_room","_mid"},{"_squareChat","_squareChatMid"},{"_squareMember","_squareMemberMid"}})if(hasTable(table)&&!hasColumn(table,field))tables.erase(table);
}
std::map<std::string,std::string> LineDatabaseSource::names(Control& control){
 std::map<std::string,std::string> result;
 const auto add=[&](const std::string& table,const std::string& identityColumn,const std::vector<std::string>& fields,bool directOnly=false){
  std::map<std::string,std::string> found;std::set<std::string> ambiguous;
  for(const auto& row:nameRows(table,identityColumn,fields,control)){
   control.check();std::string mid;try{mid=id(row,identityColumn.c_str());}catch(const Error&){continue;}
   if(directOnly&&(!lineIdentifier(mid)||mid.front()!='u'))continue;
   auto name=pickName(row,fields,mid);if(name.empty())continue;
   auto existing=found.find(mid);if(existing!=found.end()&&existing->second!=name)ambiguous.insert(mid);else found.emplace(mid,std::move(name));
  }
  for(const auto& [mid,name]:found)if(!ambiguous.count(mid))result.try_emplace(mid,name);
 };
 add("_contact","_mid",contactNameFields);
 add("_profile","_mid",contactNameFields);
 add("_squareMember","_squareMemberMid",memberNameFields);
 // A retained direct-chat title may remain after its contact cache row was
 // removed. It is a fallback for that exact MID, not a name-based identity join.
 add("_chat","_id",chatNameFields,true);
 return result;
}
void LineDatabaseSource::loadChats(Control& control){
 auto tick=[&]{control.check();};auto people=names(control);
 std::map<std::string,J> known;
 auto add=[&](const std::string& source,const std::string& title,const std::string& kind,const std::string& service=""){auto name=cleanName(title,source);known[source]={{"source_id",source},{"title",name},{"kind",kind},{"name_resolved",!name.empty()}};if(!service.empty())chatServices[source]=service;};
 const auto metadataChats=[&](const std::string& table,const std::string& identityColumn,const std::string& kind,const std::string& service){
  std::map<std::string,std::string> labels;std::set<std::string> ambiguous;
  for(const auto& row:nameRows(table,identityColumn,chatNameFields,control)){
   std::string mid;try{mid=id(row,identityColumn.c_str());}catch(const Error&){continue;}
   auto name=kind=="direct"?(people.count(mid)?people.at(mid):std::string()):pickName(row,chatNameFields,mid);
   auto existing=labels.find(mid);if(existing!=labels.end()&&!existing->second.empty()&&!name.empty()&&existing->second!=name)ambiguous.insert(mid);
   else if(existing==labels.end()||existing->second.empty())labels[mid]=std::move(name);
  }
  for(const auto& [mid,name]:labels)add(mid,ambiguous.count(mid)?std::string():name,kind,service);
 };
 metadataChats("_contact","_mid","direct","talk");
 metadataChats("_room","_mid","room","talk");
 metadataChats("_groupChat","_chatMid","group","talk");
 metadataChats("_squareChat","_squareChatMid","community","g2");
 std::set<std::string> available,threads;
 for(const auto& row:nameRows("_chat","_id",chatNameFields,control)){
  std::string source;try{source=id(row,"_id");}catch(const Error&){continue;}available.insert(source);
  auto name=pickName(row,chatNameFields,source);
  if(!known.count(source)){const auto kind=lineIdentifier(source)&&source.front()=='u'?"direct":lineIdentifier(source)&&source.front()=='r'?"room":lineIdentifier(source)&&source.front()=='s'?"community":"group";if(people.count(source))name=people.at(source);add(source,name,kind,kind==std::string("community")?"g2":"talk");}
  else if(!name.empty()&&!known[source].value("name_resolved",false)){known[source]["title"]=name;known[source]["name_resolved"]=true;}
 }
 // Some LINE builds retain messages after a _chat metadata row disappears.
 for(auto& row:database->rows("SELECT DISTINCT _chatId FROM _message ORDER BY _chatId COLLATE BINARY",J::array(),tick))available.insert(id(row,"_chatId"));
 if(hasTable("_squareThread"))for(auto& row:database->rows("SELECT _threadMid,_chatMid FROM _squareThread",J::array(),tick)){threads.insert(id(row,"_threadMid"));available.insert(id(row,"_chatMid"));}
 size_t unnamedChats=0;
 for(auto& source:available){control.check();if(threads.count(source))continue;if(!known.count(source)){const auto kind=lineIdentifier(source)&&source.front()=='u'?"direct":lineIdentifier(source)&&source.front()=='r'?"room":lineIdentifier(source)&&source.front()=='s'?"community":"group";add(source,people.count(source)?people.at(source):std::string(),kind,kind==std::string("community")?"g2":"talk");}auto chat=known[source];if(!chat.value("name_resolved",false))chat["title"]=fallbackChatTitle(chat.value("kind","group"),++unnamedChats);validateChat(chat);catalog.push_back(chat);}
 // Number unresolved senders by their source-local, sorted identity. The number
 // is a display aid only; the original MID remains the saved sender identity.
 for(const auto& row:database->rows("SELECT DISTINCT _from FROM _message WHERE typeof(_from)='text' AND _from<>'' AND length(CAST(_from AS BLOB))<=1024 ORDER BY _from COLLATE BINARY LIMIT 100000",J::array(),tick)){
  control.check();std::string sender;try{sender=text(row,"_from",1024);}catch(const Error&){continue;}if(!sender.empty()&&!people.count(sender))unknownSenderNumbers.emplace(sender,unknownSenderNumbers.size()+1);
 }
}
J LineDatabaseSource::chats(){verifyIdentity();return catalog;}
J LineDatabaseSource::chatsWithControl(Control& control){control.check();verifyIdentity();auto value=catalog;control.check();return value;}
Batch LineDatabaseSource::read(const J& selected,const std::set<std::string>& kinds,const std::string& since,const std::string& until,Control& control){
 control.check();verifyIdentity();if(std::find(catalog.begin(),catalog.end(),selected)==catalog.end())throw CapabilityError("選取的聊天室與目前 LINE 資料庫不符，請重新連接。");
 if(kinds.empty()||!kinds.count("message"))throw CapabilityError("LINE 本機資料庫來源請選取對話與附件。");
 for(auto& kind:kinds)if(kind!="message"&&kind!="thread")throw CapabilityError("此 LINE 資料庫來源提供對話與附件，尚未提供相簿或記事本。");
 const auto from=boundary(since,0),to=boundary(until,lastTime);if(from>=to)throw CapabilityError("LINE 讀取的結束期間必須晚於開始期間。");
 auto tick=[&]{control.check();};Snapshot snapshot(*database);auto contacts=names(control);auto chatId=id(selected,"source_id");
 // Additional genuine LINE columns (for example _contentInfo) stay in memory
 // for the native attachment resolver. Only the explicit item fields below are
 // archived; session tokens, URLs and media keys are never copied wholesale.
 std::string projection="m.*",source="_message m",filter="m._chatId=?";J chatArgs=J::array({chatId});
 if(hasTable("_squareThread")){
  projection+=",t._threadMid AS _history_thread_id,t._messageId AS _history_thread_root_id";
  source+=" LEFT JOIN _squareThread t ON t._threadMid=m._chatId AND t._chatMid=?";
  filter="m._chatId IN (SELECT ? UNION SELECT _threadMid FROM _squareThread WHERE _chatMid=?)";chatArgs=J::array({chatId,chatId,chatId});
 }
 const auto query="SELECT "+projection+" FROM "+source+" WHERE "+filter+
  " AND m._createdTime>=? AND m._createdTime<? AND (? IS NULL OR m._createdTime>? OR (m._createdTime=? AND m._id COLLATE BINARY>?)) ORDER BY m._createdTime,m._id COLLATE BINARY LIMIT ?";
 Batch result;result.coverage="local_only";std::set<std::string> seen;int64_t last=from;J after=nullptr;uint64_t bytes=0;
 for(;;){
  control.check();verifyIdentity();J args=chatArgs;for(auto value:J::array({from,to,after,last,last,after,int64_t(pageSize)}))args.push_back(std::move(value));
  auto page=database->rows(query,args,tick);
  for(auto& row:page){
   control.check();const auto mid=id(row,"_id"),actualChat=id(row,"_chatId");const auto sent=integer(row,"_createdTime"),type=row["_contentType"].is_null()?0:integer(row,"_contentType");
   if(sent<from||sent>=to||(!after.is_null()&&(sent<last||(sent==last&&mid<=after.get<std::string>())))||!seen.insert(mid).second)throw CapabilityError("LINE 分頁識別或排序改變，未覆寫收藏。");
   J info={{"source","line-database"},{"content_type",type}};
   if(actualChat!=chatId){info["thread_id"]=id(row,"_history_thread_id");auto root=text(row,"_history_thread_root_id",1024);if(!root.empty())info["thread_root_message_id"]=root;}
   auto sender=text(row,"_from",1024);std::string senderName;
   if(!sender.empty()){
    info["sender_name_resolved"]=contacts.count(sender)!=0;
    if(contacts.count(sender))senderName=contacts.at(sender);
    else{auto found=unknownSenderNumbers.find(sender);if(found==unknownSenderNumbers.end()&&unknownSenderNumbers.size()<100000)found=unknownSenderNumbers.emplace(sender,unknownSenderNumbers.size()+1).first;senderName=unnamed("未知成員",found==unknownSenderNumbers.end()?0:found->second);}
   }
   J item={{"source_id",mid},{"kind","message"},{"text",messageText(row,type)},{"sender_id",sender.empty()?J(nullptr):J(sender)},{"sender_name",senderName},{"sent_at",timestamp(sent)},{"ordinal",result.items.size()},{"metadata",info}};
   validateItem(item);auto encoded=item.dump();bytes+=encoded.size();
   if(result.items.size()>=maxMessages||bytes>maxMessageBytes)throw CapabilityError("選取期間的 LINE 本機訊息過多，請縮小日期後分批整理；未把部分讀取標成完成。");
   const bool hasMedia=type==1||type==2||type==3||type==7||type==14||type==21;
   if(hasMedia){auto service=chatServices.find(chatId);if(service!=chatServices.end())media->setChatService(actualChat,service->second);}
   auto attachments=hasMedia?media->attachments(actualChat,row,&control):J::array();
   if(!attachments.is_array())throw CapabilityError("LINE 附件索引格式不相容。");
   for(auto attachment:attachments){control.check();attachment["item_source_id"]=mid;validateAttachment(attachment);bytes+=attachment.dump().size();if(bytes>maxMessageBytes)throw CapabilityError("選取期間的 LINE 附件索引過多，請縮小日期後分批整理。");attachmentChats[key({chatId,mid,text(attachment,"source_id"),text(attachment,"variant"),text(attachment,"version")})]=actualChat;result.attachments.push_back(std::move(attachment));}
   result.items.push_back(std::move(item));last=sent;after=mid;
  }
  if(page.size()<pageSize)break;
 }
 snapshot.commit();verifyIdentity();control.check();return result;
}
fs::path LineDatabaseSource::download(const J& selected,const J& attachment,const fs::path& stage,Control& control){
 control.check();verifyIdentity();if(std::find(catalog.begin(),catalog.end(),selected)==catalog.end())throw CapabilityError("附件所屬聊天室與 LINE 資料庫不符。");
 const auto parent=id(selected,"source_id");auto found=attachmentChats.find(key({parent,text(attachment,"item_source_id"),text(attachment,"source_id"),text(attachment,"variant"),text(attachment,"version")}));
 if(found==attachmentChats.end())throw CapabilityError("附件不屬於目前讀取的 LINE 聊天室。");
 auto result=media->copy(found->second,attachment,stage,control);verifyIdentity();return result;
}
void LineDatabaseSource::bindMediaFile(const J& selected,const std::string& messageId,const fs::path& path,bool thumbnail,Control* requested){
 Control fallback;auto& control=requested?*requested:fallback;control.check();verifyIdentity();
 if(std::find(catalog.begin(),catalog.end(),selected)==catalog.end())throw CapabilityError("附件所屬聊天室與 LINE 資料庫不符。");
 auto parent=id(selected,"source_id");J args=J::array({messageId,parent});std::string filter="_chatId=?";
 if(hasTable("_squareThread")){filter="_chatId IN (SELECT ? UNION SELECT _threadMid FROM _squareThread WHERE _chatMid=?)";args.push_back(parent);}
 auto rows=database->rows("SELECT _id,_chatId FROM _message WHERE _id=? AND "+filter,args,[&]{control.check();});
 if(rows.size()!=1)throw CapabilityError("指定訊息不屬於目前選取的 LINE 聊天室，未連結檔案。");
 media->bindFile(id(rows[0],"_chatId"),id(rows[0],"_id"),path,thumbnail,&control);verifyIdentity();control.check();
}
void LineDatabaseSource::setSession(std::shared_ptr<LineSession> session){
 verifyIdentity();media->setSession(std::move(session));
}
}
