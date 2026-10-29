#include "core.hpp"
#include "attachment_scope.hpp"
#include "blob_path.hpp"
#include "file.hpp"
#include <algorithm>
#include <sstream>
#include <ctime>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace atlas {
static std::string text(const J&j,const std::string&k,const std::string&def=""){return j.contains(k)&&j[k].is_string()?j[k].get<std::string>():def;}
static constexpr uint64_t partSize=768*1024;
static J cachedParts(Store&s,const J&blob,Tick tick){const auto hash=text(blob,"sha256");if(!canonicalBlobPath(text(blob,"path"),hash)||!blob["size"].is_number_integer()||blob["size"].get<int64_t>()<0)return nullptr;const auto size=blob["size"].get<uint64_t>(),count=std::max<uint64_t>(1,(size+partSize-1)/partSize);try{auto info=J::parse(readText(within(s.root,"downloads/"+hash+"/info.json"),count*512+4096));if(info["sha256"]!=hash||info["size"]!=size||!info["parts"].is_array()||info["parts"].size()!=count)return nullptr;uint64_t total=0,index=0;for(auto&p:info["parts"]){if(tick)tick();auto rel="downloads/"+hash+"/"+std::to_string(index++)+".js";if(p["path"]!=rel||(!p["size"].is_number_unsigned()&&!p["size"].is_number_integer())||p["size"].get<int64_t>()<0||p["size"].get<uint64_t>()>partSize||text(p,"sha256").size()!=64)return nullptr;total+=p["size"].get<uint64_t>();if(digest(within(s.root,rel),tick).hash!=text(p,"script_sha256"))return nullptr;}if(total!=size)return nullptr;return info;}catch(const Cancelled&){throw;}catch(...){return nullptr;}}
bool validParts(Store&s,const J&b,Tick tick){return !cachedParts(s,b,tick).is_null();}
J downloadParts(Store&s,const J&b,Tick tick){if(!canonicalBlobPath(text(b,"path"),text(b,"sha256"))||!b["size"].is_number_integer()||b["size"].get<int64_t>()<0)throw IntegrityError("檔案校驗欄位不正確，未建立下載資料。");auto cached=cachedParts(s,b,tick);if(!cached.is_null())return cached;auto hash=text(b,"sha256"),rel=text(b,"path");VerifiedInput in(within(s.root,rel));if(in.size()!=b["size"].get<uint64_t>())throw IntegrityError("檔案大小改變，知識地圖尚未更新。");J parts=J::array();Sha256 whole;uint64_t size=0,index=0;std::vector<char>buffer(partSize);while(true){if(tick)tick();in.read(buffer.data(),buffer.size());auto n=in.gcount();if(!n&&index)break;std::string_view bytes(buffer.data(),n);whole.update(bytes.data(),bytes.size());size+=n;std::string script="window.VaultBytes.add("+safeJson(hash)+","+std::to_string(index)+","+safeJson(base64(bytes))+");\n",path="downloads/"+hash+"/"+std::to_string(index++)+".js";atomicText(within(s.root,path),script);parts.push_back({{"path",path},{"sha256",sha(bytes)},{"script_sha256",sha(script)},{"size",n}});if(!n)break;}if(in.bad()||whole.finish()!=hash||size!=b["size"].get<uint64_t>())throw Error("檔案內容改變，知識地圖尚未更新。");J info={{"sha256",hash},{"size",size},{"parts",parts}};atomicText(within(s.root,"downloads/"+hash+"/info.json"),safeJson(info));return info;}
static void replaceAll(std::string&str,const std::string&from,const std::string&to){size_t pos=0;while((pos=str.find(from,pos))!=std::string::npos){str.replace(pos,from.size(),to);pos+=to.size();}}
static std::string shaBase64(const std::string&s){auto hex=sha(s);std::string bytes;for(size_t n=0;n<hex.size();n+=2)bytes+=static_cast<char>(std::stoi(hex.substr(n,2),nullptr,16));return base64(bytes);}

namespace {
std::string captureKind(const std::string& kind){
 if(kind=="image")return "圖片";if(kind=="video")return "影片";if(kind=="audio")return "語音";if(kind=="sticker")return "貼圖";return "檔案";
}
std::string captureTime(const std::string& value){
 try{
  const auto normalized=normalizeTime(value);unsigned y,mo,d,h,m,s,ms;
  if(std::sscanf(normalized.c_str(),"%u-%u-%uT%u:%u:%u.%u",&y,&mo,&d,&h,&m,&s,&ms)!=7)return "未標示時間";
  auto instant=std::chrono::sys_days{std::chrono::year{int(y)}/std::chrono::month{mo}/std::chrono::day{d}}+std::chrono::hours{h+8}+std::chrono::minutes{m}+std::chrono::seconds{s}+std::chrono::milliseconds{ms};
  const auto day=std::chrono::floor<std::chrono::days>(instant);const auto date=std::chrono::year_month_day{day};const auto clock=std::chrono::hh_mm_ss{instant-day};
  char out[64]{};std::snprintf(out,sizeof out,"%04d-%02u-%02u_%02u-%02u-%02u-%03u",int(date.year()),unsigned(date.month()),unsigned(date.day()),unsigned(clock.hours().count()),unsigned(clock.minutes().count()),unsigned(clock.seconds().count()),unsigned(clock.subseconds().count()));return out;
 }catch(...){return "未標示時間";}
}
std::string storedExtension(const J& value){
 const auto path=text(value,"path");if(path.empty())return {};
 const auto ext=utf8(path8(path).extension());
 for(const auto* known:{".png",".jpg",".gif",".webp",".wav",".ogg",".mp3",".mp4",".bin"})if(ext==known)return ext;return {};
}
bool generatedName(const J& file){return file.contains("name_generated")&&file["name_generated"].is_boolean()&&file["name_generated"].get<bool>();}
std::string safeCaptureReason(const std::string& given,const std::string& fallback){
 if(given.empty()||given.size()>512||std::any_of(given.begin(),given.end(),[](unsigned char c){return c<32||c==127;}))return fallback;
 std::string lower=given;for(auto& c:lower)if(c>='A'&&c<='Z')c+=char('a'-'A');
 for(const auto* marker:{"://","authorization","x-line-","authtoken","keymaterial","bearer ","token="})if(lower.find(marker)!=std::string::npos)return fallback;
 size_t run=0;for(unsigned char c:given){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='='){if(++run>=48)return fallback;}else run=0;}
 try{J(given).dump();}catch(...){return fallback;}
 auto out=given;for(size_t at=0;(at=out.find("附件",at))!=std::string::npos;){out.replace(at,std::string("附件").size(),"檔案");at+=std::string("檔案").size();}return out;
}
std::string pendingReason(const J& row){
 auto reason=text(row,"reason");if(!reason.empty())return reason;
 const auto status=text(row,"status"),capture=text(row,"capture_status");
 if(status=="pending")return capture=="stopped"||capture=="interrupted"?"抓取已停止，這份檔案尚未完成保存，可重新抓取。":"這份檔案尚未完成保存，可重新抓取。";
 if(status=="unavailable")return "這份檔案目前未能取得，可重新抓取或稍後重試。";
 if(status=="failed")return "這份檔案保存未完成，可重新抓取。";
 return {};
}
bool captureFileAvailable(Store& store,const J& file,Tick tick,bool verifyContents){
 if(tick)tick();const auto path=text(file,"path"),hash=text(file,"sha256");
 if(!canonicalBlobPath(path,hash)||!file.contains("size")||!file["size"].is_number_integer()||file["size"].get<int64_t>()<0)return false;
 try{
  uint64_t size=0;if(verifyContents){const auto actual=digest(within(store.root,path),tick);if(actual.hash!=hash)return false;size=actual.size;}else{VerifiedInput input(within(store.root,path));size=input.size();}
  if(size!=file["size"].get<uint64_t>())return false;
  if(file.contains("expected_size")&&!file["expected_size"].is_null()&&file["expected_size"]!=size)return false;
  if(file.contains("expected_hash")&&!file["expected_hash"].is_null()&&file["expected_hash"]!=hash)return false;
  return true;
 }catch(const Cancelled&){throw;}catch(...){return false;}
}
}
std::string captureFileName(const J& file,const std::string& messageSourceId,const std::string& sentAt){
 auto name=text(file,"name");bool legacy=false;
 const auto source=text(file,"source_id");
 if(text(file,"version")=="line-native-v1"&&(!file.contains("name_generated")||generatedName(file))&&!messageSourceId.empty()&&source==messageSourceId){
  for(const auto* kind:{"image","video","audio","file","sticker"})for(const auto* ext:{".jpg",".mp4",".ogg",".bin"})if(name==std::string(kind)+"-"+messageSourceId+ext)legacy=true;
 }
 if(name.empty()||legacy){name=captureTime(sentAt)+"_"+captureKind(text(file,"kind"));if(text(file,"variant")=="thumbnail")name+="_預覽";auto ext=storedExtension(file);if(!ext.empty())name+=ext;}
 else if(generatedName(file)&&path8(name).extension().empty()){const auto ext=storedExtension(file);if(!ext.empty())name+=ext;}
 return safeName(name);
}

fs::path exportHtml(Store&s,Tick tick,std::string* warning,bool prepareDownloadParts){if(warning)warning->clear();J problems=s.check(tick,false,prepareDownloadParts);for(auto&p:problems)if(std::set<std::string>{"identity","database","relations"}.count(p["code"].get<std::string>()))throw IntegrityError("收藏識別或關聯不正確，未建立知識地圖。");auto generation=sha(now()+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())).substr(0,32);auto relative="reader/"+generation;auto folder=privateDir(within(s.root,relative));J catalog=J::array(),downloads=J::object(),portable=J::array();std::set<std::string>invalid;for(auto&p:problems)if(p["code"]=="file")invalid.insert(text(p,"id"));for(auto&b:s.db.rows("SELECT * FROM blobs ORDER BY id",J::array(),tick)){if(tick)tick();if(invalid.count(text(b,"id")))continue;portable.push_back(b["path"]);if(prepareDownloadParts){auto entry=downloadParts(s,b,tick);auto hash=text(b,"sha256");downloads[hash]=entry;portable.push_back("downloads/"+hash+"/info.json");for(auto&p:entry["parts"])portable.push_back(p["path"]);}}
 for(auto&c:s.db.rows("SELECT c.id,c.title,c.kind,c.account,p.id AS parent_id FROM chats c LEFT JOIN chats p ON p.account=c.account AND p.source_id=c.parent_source_id ORDER BY c.title,c.id",J::array(),tick)){if(tick)tick();auto cid=text(c,"id");J chunk=J::array(),chunks=J::array();int count=0;auto publish=[&]{auto name=cid+"-"+std::to_string(chunks.size())+".js";atomicText(within(folder,name),"window.VaultData.add("+safeJson(cid)+","+safeJson(chunk)+");\n");chunks.push_back(relative+"/"+name);portable.push_back(relative+"/"+name);chunk=J::array();};for(auto&r:s.db.rows("SELECT * FROM items WHERE chat_id=? ORDER BY sent_at,ordinal,id",{cid},tick)){if(tick)tick();J i;for(auto name:{"id","kind","parent_id","reply_source_id","source_id","text","title","sender_name","sent_at","state","ordinal"})i[name]=r[name];i["chat_id"]=cid;if(!r["sender_id"].is_null())i["sender_ref"]=key({"person",text(c,"account"),text(r,"sender_id")});try{auto meta=J::parse(text(r,"metadata","{}"));if(meta.value("content_omitted",false))i["content_omitted"]=true;if(meta.contains("sender_name_resolved")&&meta["sender_name_resolved"].is_boolean())i["sender_name_resolved"]=meta["sender_name_resolved"];if(meta.contains("tags")&&meta["tags"].is_array()){i["tags"]=J::array();for(auto&t:meta["tags"])if(t.is_string()&&t.get_ref<const std::string&>().size()<=128&&i["tags"].size()<12)i["tags"].push_back(t);}}catch(...){}i["attachments"]=J::array();for(auto&a:s.db.rows("SELECT a.*,b.path,b.size,b.sha256,cs.status AS capture_status FROM attachments a LEFT JOIN blobs b ON b.id=a.blob_id LEFT JOIN capture_state cs ON cs.chat_id=a.chat_id WHERE a.item_id=? AND "+std::string(requiredAttachmentSql)+" ORDER BY a.id",{r["id"]},tick)){if(tick)tick();bool valid=!a["path"].is_null()&&!invalid.count(text(a,"blob_id")),saved=a["status"]=="saved"||a["status"]=="preview";J entry={{"id",a["id"]},{"name",captureFileName(a,text(r,"source_id"),text(r,"sent_at"))},{"kind",a["kind"]},{"status",valid?a["status"]:(saved?J("failed"):a["status"])},{"path",valid&&saved?a["path"]:J(nullptr)},{"download",valid&&saved?a["sha256"]:J(nullptr)},{"size",a["size"]},{"reason",saved&&!valid?J("本機副本遺失或與保存紀錄不同，請重新抓取。"):J(pendingReason(a))},{"saved_before",a["saved_before"]!=0}};i["attachments"].push_back(entry);}chunk.push_back(i);count++;if(chunk.size()==500)publish();}if(!chunk.empty())publish();auto jobs=s.db.rows("SELECT status,coverage,finished_at,counts FROM capture_state WHERE chat_id=?",{cid},tick);J job=jobs.empty()?J(nullptr):jobs[0];if(!job.is_null())try{job["counts"]=J::parse(text(job,"counts","{}"));}catch(...){job["counts"]=J::object();}catalog.push_back({{"id",cid},{"title",c["title"]},{"kind",c["kind"]},{"parent_id",c["parent_id"]},{"count",count},{"chunks",chunks},{"last_job",job}});}
 auto accounts=s.db.rows("SELECT DISTINCT account FROM chats",J::array(),tick);bool artificial=!accounts.empty();for(auto&a:accounts)if(text(a,"account")!="synthetic-local-account"&&text(a,"account")!="synthetic-native-account")artificial=false;J manifest={{"version",1},{"created_at",now()},{"chats",catalog},{"summary",s.summary()},{"downloads",downloads},{"artificial_data",artificial},{"problems",problems},{"generation",generation},{"direct_file_downloads",!prepareDownloadParts},{"file_verification",prepareDownloadParts?"sha256":"size"}};std::string page=web_template;replaceAll(page,"@@CSP_SCRIPT@@",shaBase64(web_script));replaceAll(page,"@@CSP_STYLE@@",shaBase64(web_css));replaceAll(page,"@@STYLE@@",web_css);replaceAll(page,"@@MANIFEST@@",safeJson(manifest));replaceAll(page,"@@SCRIPT@@",web_script);if(tick)tick();atomicText(within(s.root,"archive-info.json"),safeJson({{"version",1},{"created_at",manifest["created_at"]},{"summary",manifest["summary"]},{"problems",problems},{"portable_files",portable},{"generation",generation},{"direct_file_downloads",!prepareDownloadParts}}));if(tick)tick();atomicText(within(s.root,"index.html"),page);
 // index.html is the publication commit. Old-reader cleanup cannot undo success.
 try{if(tick)tick();for(auto&entry:fs::directory_iterator(within(s.root,"reader"))){if(tick)tick();auto name=utf8(entry.path().filename());if(name!=generation&&name.size()==32&&std::all_of(name.begin(),name.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');})){noLinks(entry.path());if(entry.is_directory())fs::remove_all(entry.path());}}}catch(...){if(warning)*warning="知識地圖已完成；舊閱讀資料尚未清理，已保留在收藏的 reader 資料夾。";}return s.root/"index.html";
}
static std::string extension(const fs::path&p){VerifiedInput in(p);char b[32]{};in.read(b,32);auto n=in.gcount();auto starts=[&](const char*s,size_t k){return n>=static_cast<std::streamsize>(k)&&std::memcmp(b,s,k)==0;};if(starts("\x89PNG\r\n\x1a\n",8))return ".png";if(starts("\xff\xd8\xff",3))return ".jpg";if(starts("GIF87a",6)||starts("GIF89a",6))return ".gif";if(starts("RIFF",4)&&std::memcmp(b+8,"WEBP",4)==0)return ".webp";if(starts("RIFF",4)&&std::memcmp(b+8,"WAVE",4)==0)return ".wav";if(starts("OggS",4))return ".ogg";if(starts("ID3",3))return ".mp3";if(n>=8&&std::memcmp(b+4,"ftyp",4)==0)return ".mp4";return ".bin";}
static J captureItems(Store&s,const std::string&cid,const Batch&batch,const std::set<std::string>&kinds,const std::string&mode,std::set<std::string>&kept,J&counts,Tick tick){if(tick)tick();
 J result=J::array();if(mode=="update"){for(auto i:batch.items){if(tick)tick();if(kinds.count(text(i,"kind"))){kept.insert(text(i,"source_id"));result.push_back(std::move(i));}}return result;}
 std::map<std::string,J>incoming,existing;for(auto i:batch.items){if(tick)tick();validateItem(i);auto id=text(i,"source_id");if(!incoming.emplace(id,std::move(i)).second)throw Error("來源含重複識別，未更新舊資料。");}
 for(auto&r:s.db.rows("SELECT source_id,kind FROM items WHERE chat_id=?",{cid},tick))existing[text(r,"source_id")]=r;
 std::set<std::string>visiting,done,retained,omitted;
 std::function<void(const std::string&)>keep=[&](const std::string&id){if(tick)tick();
  if(done.count(id))return;if(!visiting.insert(id).second)throw Error("來源關聯形成循環，未建立檔案項目。");
  auto found=incoming.find(id);if(found==incoming.end()){if(existing.count(id)){done.insert(id);visiting.erase(id);return;}throw Error("檔案缺少可核對的所屬內容，未建立孤立項目。");}
  auto item=found->second;auto kind=text(item,"kind");bool message=kind=="message"||kind=="thread";
  if(existing.count(id)&&text(existing[id],"kind")!=kind)throw Error("相同來源的內容類型改變，舊資料已保留。");
  if(existing.count(id)&&(message||!kinds.count(kind))){kept.insert(id);if(message)retained.insert(id);done.insert(id);visiting.erase(id);return;}
  auto parent=text(item,"parent_source_id");if(!parent.empty())keep(parent);
  if(message||!kinds.count(kind)){
   item["text"]="";item["title"]="";item["sender_name"]="";item["sender_id"]=nullptr;item["reply_source_id"]=nullptr;
   item["metadata"]={{"content_omitted",true},{"omission_mode","skip_text"}};if(message)omitted.insert(id);
  }
  result.push_back(std::move(item));kept.insert(id);done.insert(id);visiting.erase(id);
 };
 for(auto&[id,item]:incoming){if(tick)tick();if(kinds.count(text(item,"kind"))){auto kind=text(item,"kind");if(kind!="message"&&kind!="thread")keep(id);else if(existing.count(id))keep(id);}}
 for(auto&a:batch.attachments){if(tick)tick();auto id=text(a,"item_source_id");auto i=incoming.find(id);if(i!=incoming.end()&&kinds.count(text(i->second,"kind")))keep(id);}
 counts["retained_messages"]=retained.size();counts["omitted_messages"]=omitted.size();return result;
}
J captureReport(Store& store,size_t limit,size_t offset,const std::string& status,const std::string& query,Tick tick,bool verifyContents){
 if(limit==0||limit>200||offset>static_cast<size_t>(INT64_MAX)||query.size()>512)throw Error("檔案檢查分頁或搜尋範圍不正確。");
 if(!std::set<std::string>{"all","saved","preview","pending","unavailable","failed","missing","available"}.count(status))throw Error("檔案狀態篩選不正確。");
 if(tick)tick();auto identity=store.identityProblems(tick);if(!identity.empty())throw IntegrityError("對話資料的識別需要檢查，未讀取檔案位置。");
 const auto blank=[] {return J{{"saved",0},{"preview",0},{"pending",0},{"unavailable",0},{"failed",0}};};
 J totals=blank(),storedTotals=blank(),pageTotals=blank(),files=J::array();size_t total=0,scanOffset=0;
 const auto lower=[](std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c+=char('a'-'A');return value;};const auto wanted=lower(query);
 // Filtering must use the checked state, including lost or damaged previously-saved files.
 // Batches bound memory while an exact total and status filter inspect every required file.
 while(true){
  auto rows=store.db.rows("SELECT a.*,c.title AS chat_title,i.source_id AS message_source_id,i.sent_at,i.sender_name,b.path,b.sha256,b.size,cs.status AS capture_status FROM attachments a JOIN chats c ON c.id=a.chat_id JOIN items i ON i.id=a.item_id LEFT JOIN blobs b ON b.id=a.blob_id LEFT JOIN capture_state cs ON cs.chat_id=a.chat_id WHERE "+std::string(requiredAttachmentSql)+" ORDER BY c.title,a.chat_id,i.sent_at,i.ordinal,a.id LIMIT 200 OFFSET ?",{scanOffset},tick);
  if(rows.empty())break;scanOffset+=rows.size();
  for(auto& row:rows){
   if(tick)tick();const auto recorded=text(row,"status");if(storedTotals.contains(recorded))storedTotals[recorded]=storedTotals[recorded].get<int64_t>()+1;
   const bool available=(recorded=="saved"||recorded=="preview")&&captureFileAvailable(store,row,tick,verifyContents);
   auto actual=recorded,reason=pendingReason(row);const auto name=captureFileName(row,text(row,"message_source_id"),text(row,"sent_at"));
   if((recorded=="saved"||recorded=="preview")&&!available){actual="failed";reason=verifyContents?"本機副本遺失或校驗不符，請重新抓取。":"本機副本遺失或大小與保存紀錄不同，請重新抓取。";}
   if(available)reason=recorded=="preview"?"預覽檔已保存，尚未取得原檔。":"原檔已保存，可離線開啟。";
   if(totals.contains(actual))totals[actual]=totals[actual].get<int64_t>()+1;
   const bool matches=status=="all"||(status=="missing"?actual!="saved":status=="available"?available:actual==status);
   if(!matches||(!wanted.empty()&&lower(name+"\n"+text(row,"chat_title")).find(wanted)==std::string::npos))continue;
   const auto position=total++;if(position<offset||files.size()>=limit)continue;
   if(pageTotals.contains(actual))pageTotals[actual]=pageTotals[actual].get<int64_t>()+1;
   files.push_back({{"id",row["id"]},{"chat_id",row["chat_id"]},{"chat_title",row["chat_title"]},{"item_id",row["item_id"]},{"blob_id",row["blob_id"]},{"name",name},{"kind",row["kind"]},{"variant",row["variant"]},{"status",actual},{"stored_status",recorded},{"reason",reason},{"downloadable",available},{"path",available?row["path"]:J(nullptr)},{"size",row["size"]},{"sent_at",row["sent_at"]},{"sender_name",row["sender_name"]},{"saved_before",row["saved_before"]!=0}});
  }
 }
 return {{"files",files},{"total",total},{"offset",offset},{"limit",limit},{"limited",offset+files.size()<total},{"totals",totals},{"totals_scope","verified_states"},{"stored_totals",storedTotals},{"page_totals",pageTotals},{"verification",verifyContents?"sha256":"size"},{"verification_scope","all_required_files"},{"chat_count",store.db.rows("SELECT count(*) AS n FROM chats",J::array(),tick)[0]["n"]},{"item_count",store.db.rows("SELECT count(*) AS n FROM items",J::array(),tick)[0]["n"]}};
}
J collect(Store& s,Source& source,const J& selected,Control& control,std::set<std::string> kinds,const std::string& since,const std::string& until,std::function<void(std::string)> progress,const std::string& messageMode,CaptureProgressCallback detail){
 if(messageMode!="update"&&messageMode!="skip_text")throw Error("對話保存方式不正確。");
 for(const auto& kind:kinds)if(!allKinds.count(kind))throw Error("整理內容類型不正確。");
 if(selected.empty()||kinds.empty()||(!since.empty()&&!until.empty()&&since>=until))throw Error("請先選擇聊天室、內容與有效期間。");
 std::set<std::string> ids;for(const auto& chat:selected)if(!ids.insert(text(chat,"source_id")).second)throw Error("聊天室選取重複。");
 if(std::any_of(selected.begin(),selected.end(),[](const J& chat){return chat.contains("parent_source_id")&&!chat["parent_source_id"].is_null();})){
  const auto available=source.chatsWithControl(control);s.db.transaction([&]{for(const auto& chat:selected){auto parent=text(chat,"parent_source_id");if(!parent.empty()){
   auto found=std::find_if(available.begin(),available.end(),[&](const J& value){return text(value,"source_id")==parent&&text(value,"kind")=="community";});
   if(found==available.end())throw Error("子聊天室缺少有效社群。");s.putChat(source.accountFor(*found),source.storageChat(*found));
  }}});
 }
 const auto emptyCounts=[] {return J{{"items",0},{"reused",0},{"downloaded",0},{"failed",0},{"unavailable",0},{"retained_messages",0},{"omitted_messages",0}};};
 J counts=emptyCounts();auto tick=[&]{control.check();};size_t chatIndex=0;
 for(const auto& chat:selected){
  control.check();++chatIndex;J local=emptyCounts();std::string cid;int64_t jid=0;
  s.db.transaction([&]{
   cid=s.putChat(source.accountFor(chat),source.storageChat(chat));
   J options={{"version",1},{"kinds",kinds},{"since",since},{"until",until},{"message_mode",messageMode}};
   const auto highest=s.db.rows("SELECT max(coalesce((SELECT max(id) FROM jobs),0),coalesce((SELECT max(job_id) FROM capture_state),0)) AS n")[0]["n"].get<int64_t>();
   if(highest==INT64_MAX)throw Error("歷史紀錄識別已達上限，未開始新的整理。");jid=highest+1;
   s.db.execute("INSERT INTO jobs(id,chat_id,status,started_at,options) VALUES(?,?,'running',?,?)",{jid,cid,now(),options.dump()});s.recordCaptureState(jid);
  });
  std::string coverage="unknown";CaptureProgress event;event.chat_id=cid;event.chat_title=text(chat,"title");event.chat_index=chatIndex;event.chat_total=selected.size();
  auto emit=[&](const std::string& phase,const std::string& status,const std::string& reason=""){
   event.phase=phase;event.status=status;event.reason=reason;event.counts=local;if(detail)detail(event);
   if(!progress)return;
   if(phase=="chat")progress("正在讀取「"+event.chat_title+"」的完整對話（"+std::to_string(chatIndex)+" / "+std::to_string(selected.size())+"）…");
   else if(phase=="read_complete")progress("「"+event.chat_title+"」已保存 "+std::to_string(local["items"].get<int>())+" 則內容，正在處理圖片、影片與檔案…");
   else if(!event.file_name.empty()){
    const auto action=status=="pending"?"正在保存":status=="saved"?"已保存":status=="preview"?"已保存預覽":status=="reused"?"已沿用本機原檔":status=="skipped"?"略過可選預覽":status=="unavailable"?"未取得":status=="stopped"?"已停止":"保存未完成";
    auto label="「"+event.chat_title+"」 "+std::to_string(event.file_index)+" / "+std::to_string(event.file_total)+" · "+action+"「"+event.file_name+"」";
    if(!reason.empty()&&status!="pending")label+="："+reason;progress(label);
   }
  };
  auto checkpoint=[&]{s.db.transaction([&]{s.db.execute("UPDATE jobs SET counts=?,coverage=? WHERE id=?",{local.dump(),coverage,jid});s.recordCaptureState(jid);});};
  try{
   emit("chat","reading");const auto batch=source.read(chat,kinds,since,until,control);
   if(!std::set<std::string>{"local_only","visible_only","source_range","unknown"}.count(batch.coverage))throw Error("來源範圍資訊不正確。");coverage=batch.coverage;
   std::set<std::string> provided;std::map<std::string,J> itemInfo;
   for(const auto& item:batch.items){control.check();provided.insert(text(item,"source_id"));itemInfo.emplace(text(item,"source_id"),item);}
   for(const auto& file:batch.attachments)if(!provided.count(text(file,"item_source_id")))throw Error("檔案找不到可核對的所屬訊息。");
   std::set<std::string> kept;J pendingCounts=local;
   const auto items=captureItems(s,cid,batch,kinds,messageMode,kept,pendingCounts,tick);J work=J::array();
   s.db.transaction([&]{s.putItems(cid,items,tick);for(auto file:batch.attachments){control.check();if(kept.count(text(file,"item_source_id"))){validateAttachment(file);work.push_back({{"attachment",file},{"id",s.putAttachment(cid,file)}});}}});
   local["items"]=items.size();local["retained_messages"]=pendingCounts["retained_messages"];local["omitted_messages"]=pendingCounts["omitted_messages"];checkpoint();emit("read_complete","saved");
   event.file_total=work.size();size_t fileIndex=0;
   for(const auto& entry:work){
    control.check();++fileIndex;const auto aid=text(entry,"id");J file=entry["attachment"];
    const auto itemId=text(file,"item_source_id");const auto sentAt=itemInfo.count(itemId)?text(itemInfo[itemId],"sent_at"):std::string();
    event.file_id=aid;event.file_name=captureFileName(file,itemId,sentAt);event.kind=text(file,"kind");event.variant=text(file,"variant");event.file_index=fileIndex;
    auto completeName=[&](const std::string& extension){
     auto shown=captureFileName(file,itemId,sentAt);const bool generated=generatedName(file)||shown!=safeName(text(file,"name"));if(generated&&path8(shown).extension().empty()&&!extension.empty())shown+=extension;file["name"]=safeName(shown);
     event.file_name=text(file,"name");s.db.execute("UPDATE attachments SET name=? WHERE id=?",{event.file_name,aid});
    };
    auto existingName=[&]{const auto paths=s.db.rows("SELECT b.path FROM attachments a JOIN blobs b ON b.id=a.blob_id WHERE a.id=?",{aid},tick);if(!paths.empty())completeName(storedExtension(paths[0]));};
    if(file["variant"]=="thumbnail"){
     const auto originals=s.db.rows("SELECT id FROM attachments WHERE chat_id=? AND item_id=(SELECT item_id FROM attachments WHERE id=?) AND source_id=? AND version=? AND variant='full' AND status='saved' AND blob_id IS NOT NULL",{cid,aid,file["source_id"],file["version"]},tick);
     if(!originals.empty()&&s.reusable(text(originals[0],"id"),tick)){s.db.execute("UPDATE attachments SET status='unavailable',reason=? WHERE id=? AND blob_id IS NULL",{"原檔已保存，未另取預覽。",aid});emit("terminal","skipped","原檔已保存，不需另存這份預覽。");continue;}
    }
    emit("file","pending");
    if(s.reusable(aid,tick)){existingName();local["reused"]=local["reused"].get<int>()+1;checkpoint();emit("terminal",file["variant"]=="thumbnail"?"preview":"reused",file["variant"]=="thumbnail"?"已沿用預覽檔，尚未取得原檔。":"沿用已核對的本機原檔。");continue;}
    bool reused=false;s.db.transaction([&]{reused=s.reuseKnown(aid,tick);});
    if(reused){existingName();local["reused"]=local["reused"].get<int>()+1;checkpoint();emit("terminal",file["variant"]=="thumbnail"?"preview":"reused",file["variant"]=="thumbnail"?"已沿用預覽檔，尚未取得原檔。":"沿用已核對的本機原檔。");continue;}
    s.db.execute("UPDATE attachments SET status='pending',reason='' WHERE id=?",{aid});
    try{
     for(int attempt=0;attempt<3;attempt++){
      const auto stage=privateDir(within(s.root,"download-"+sha(aid+now()+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())).substr(0,24)));
      struct Stage{fs::path path;~Stage(){try{noLinks(path);fs::remove_all(path);}catch(...){}}}cleanup{stage};
      try{
       const auto downloaded=source.download(chat,file,stage,control);noLinks(downloaded);
       const auto relative=fs::relative(fs::absolute(downloaded),fs::absolute(stage));if(relative.empty())throw CapabilityError("檔案位置無法核對。");for(const auto& part:relative)if(part=="..")throw CapabilityError("檔案超出指定位置。");
       const auto data=digest(downloaded,tick);const auto actualExtension=extension(downloaded);const auto destination="assets/"+data.hash.substr(0,2)+"/"+data.hash+actualExtension;
       const int64_t expected=file["expected_size"].is_null()?-1:file["expected_size"].get<int64_t>();
       if((expected>=0&&data.size!=static_cast<uint64_t>(expected))||(!file["expected_sha256"].is_null()&&data.hash!=text(file,"expected_sha256")))throw IntegrityError("檔案與來源不符。");
       copyVerified(downloaded,within(s.root,destination),data.size,data.hash,tick);
       s.db.transaction([&]{s.saveBlob(aid,destination,data,file["variant"]=="thumbnail",tick);completeName(actualExtension);});
       local["downloaded"]=local["downloaded"].get<int>()+1;checkpoint();emit("terminal",file["variant"]=="thumbnail"?"preview":"saved",file["variant"]=="thumbnail"?"預覽已保存，尚未取得原檔。":"原檔已保存。");break;
      }catch(const Cancelled&){throw;}catch(const CapabilityError&){throw;}
      catch(const DownloadUnavailable& error){local["unavailable"]=local["unavailable"].get<int>()+1;const auto reason=safeCaptureReason(error.publicReason,"來源目前未提供可保存的原檔，可重新抓取或稍後重試。");s.db.execute("UPDATE attachments SET status='unavailable',reason=? WHERE id=?",{reason,aid});checkpoint();emit("terminal","unavailable",reason);break;}
      catch(const RetryableDownload& error){if(attempt<2){control.wait(200*(attempt+1));continue;}local["failed"]=local["failed"].get<int>()+1;const auto reason=safeCaptureReason(error.publicReason,"這份檔案暫時無法保存，可稍後重試。");s.db.execute("UPDATE attachments SET status='failed',reason=? WHERE id=?",{reason,aid});checkpoint();emit("terminal","failed",reason);break;}
      catch(const IntegrityError&){local["failed"]=local["failed"].get<int>()+1;const std::string reason="檔案內容與來源不符，未列為已保存。";s.db.execute("UPDATE attachments SET status='failed',reason=? WHERE id=?",{reason,aid});checkpoint();emit("terminal","failed",reason);break;}
      catch(const Error&){throw;}
      catch(const fs::filesystem_error&){if(attempt<2){control.wait(200*(attempt+1));continue;}local["failed"]=local["failed"].get<int>()+1;const std::string reason="本機保存未完成，請檢查保存位置後重試。";s.db.execute("UPDATE attachments SET status='failed',reason=? WHERE id=?",{reason,aid});checkpoint();emit("terminal","failed",reason);break;}
     }
    }catch(const Cancelled&){
     const auto state=s.db.rows("SELECT status FROM attachments WHERE id=?",{aid});const auto retained=state.empty()?std::string():text(state[0],"status");if(retained=="saved"||retained=="preview"){try{emit("terminal",retained,"這份檔案已保存，後續抓取已停止。");}catch(...){}}else{const std::string reason="抓取已停止，這份檔案尚未完成保存，可重新抓取。";s.db.execute("UPDATE attachments SET reason=? WHERE id=? AND status='pending'",{reason,aid});try{emit("terminal","stopped",reason);}catch(...){}}throw;
    }catch(...){
     const auto state=s.db.rows("SELECT status FROM attachments WHERE id=?",{aid});const auto retained=state.empty()?std::string():text(state[0],"status");if(retained=="pending"){const std::string reason="這份檔案保存未完成；先前已保存的內容仍保留。";s.db.execute("UPDATE attachments SET status='failed',reason=? WHERE id=? AND status='pending'",{reason,aid});local["failed"]=local["failed"].get<int>()+1;try{emit("terminal","failed",reason);}catch(...){}}throw;
    }
   }
   s.db.transaction([&]{s.db.execute("UPDATE jobs SET status=?,finished_at=?,coverage=?,counts=? WHERE id=?",{local["failed"]==0&&local["unavailable"]==0?"complete":"complete_with_issues",now(),batch.coverage,local.dump(),jid});s.recordCaptureState(jid);});
   for(auto it=counts.begin();it!=counts.end();++it)it.value()=it.value().get<int>()+local[it.key()].get<int>();
  }catch(...){
   s.db.transaction([&]{s.db.execute("UPDATE jobs SET status=?,finished_at=?,counts=?,coverage=? WHERE id=?",{control.stopped?"stopped":"failed",now(),local.dump(),coverage,jid});s.recordCaptureState(jid);});throw;
  }
 }
 return counts;
}

}
