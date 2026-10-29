#include "line_summary.hpp"
#include <iostream>
#include <stdexcept>
using namespace atlas;
namespace {
int checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F fn,const char* message){bool rejected=false;try{fn();}catch(const Error&){rejected=true;}require(rejected,message);}
template<class F>void cancels(F fn,const char* message){bool stopped=false;try{fn();}catch(const Cancelled&){stopped=true;}require(stopped,message);}
J message(const std::string& id,const std::string& time,const std::string& text="人工對話"){
 return {{"message_id",id},{"type","text"},{"sender","人工成員"},{"content",text},{"urls",J::array()},{"sent_at",time}};
}
J fixture(){
 auto threaded=message("thread-reply","2026-10-25T08:00:03+08:00","討論串中的人工回覆");
 threaded["thread_id"]="original-thread";threaded["thread_root_message_id"]="unavailable-root";
 return {{"format","chatatlas.line-summary"},{"schema",1},{"account_id","synthetic-reader-account"},
  {"chat",{{"source_id","synthetic-chat"},{"title","人工測試群組"},{"kind","group"}}},
  {"since","2026-10-25T08:00:00+08:00"},{"until","2026-10-25T08:00:05+08:00"},{"local_range_complete",true},
  {"messages",J::array({
   message("b","2026-10-25T08:00:00.1234567+08:00","同時間的第二則"),
   message("a","2026-10-25T08:00:00.1234567+08:00","同時間的第一則 <script>人工字串</script>"),
   {{"message_id","image"},{"type","image"},{"sender",nullptr},{"content",nullptr},{"sent_at","2026-10-25T00:00:01Z"}},
   {{"message_id","link"},{"type","link"},{"sender","人工成員"},{"url","https://example.invalid/synthetic"},{"title","人工連結"},{"description",nullptr},{"sent_at","2026-10-25T00:00:02.000000Z"}},
   threaded})}};
}
}
int main(int argc,char**argv){try{
 require(argc==2,"fixture directory required");fs::path root=path8(argv[1]);require(fs::is_directory(root),"fixture directory exists");
 auto snapshot=root/"snapshot.json";auto original=fixture();atomicText(snapshot,original.dump());auto originalHash=digest(snapshot).hash;const auto originalPermissions=fs::status(snapshot).permissions();
 Control control;LineSummarySource source(snapshot,&control);
 require(source.account=="line-summary:synthetic-reader-account","account namespace does not collide with demos or UI source");
 require(source.chats().size()==1&&source.chats()[0]==original["chat"],"exact authorized chat exposed");
 require(source.since()=="2026-10-25T00:00:00.000+00:00"&&source.until()=="2026-10-25T00:00:05.000+00:00","snapshot bounds normalized for capture history");
 auto batch=source.read(source.chats()[0],{"message","thread"},"","",control);
 require(batch.coverage=="local_only"&&batch.items.size()==5&&batch.attachments.empty(),"local scope with no invented binary attachments");
 require(batch.items[0]["source_id"]=="a"&&batch.items[1]["source_id"]=="b","equal timestamps sorted by stable message identity");
 require(batch.items[0]["ordinal"]==0&&batch.items[1]["ordinal"]==1,"stable ordinal");
 require(batch.items[0]["sent_at"]=="2026-10-25T00:00:00.123+00:00"&&batch.items[0]["metadata"]["original_sent_at"]==original["messages"][1]["sent_at"],"storage milliseconds preserve original precision in metadata");
 require(batch.items[2]["text"].get<std::string>().find("未提供附件原檔")!=std::string::npos&&batch.items[2]["sender_name"]=="","media placeholder and absent sender preserved");
 require(batch.items[3]["text"].get<std::string>().find("https://example.invalid/synthetic")!=std::string::npos,"link record without content remains useful");
 require(batch.items[4]["kind"]=="message"&&batch.items[4]["parent_source_id"].is_null()&&batch.items[4]["metadata"]["thread_root_message_id"]=="unavailable-root","original thread metadata does not create missing foreign keys");
 for(auto& item:batch.items)require(item["sender_id"].is_null(),"display name does not become a person identity");
 auto precise=source.read(source.chats()[0],{"message"},"2026-10-25T00:00:00.123456Z","2026-10-25T00:00:00.123457Z",control);
 require(precise.items.size()==2,"fractional range comparisons retain precision");
 require(source.read(source.chats()[0],{"message"},"2026-10-25T00:00:01Z","2026-10-25T00:00:02Z",control).items.size()==1,"subrange has half-open upper bound");
 require(source.read(source.chats()[0],{"message"},"2026-10-25T00:00:04Z","",control).items.empty(),"empty subrange is valid");
 auto altered=source.chats()[0];altered["title"]="另一個群組";rejects([&]{source.read(altered,{"message"},"","",control);},"tampered selection rejected");
 rejects([&]{source.read(source.chats()[0],{"message","album"},"","",control);},"unsupported album scope rejected");
 rejects([&]{source.read(source.chats()[0],{"thread"},"","",control);},"flat messages cannot be reported as thread-only import");
 rejects([&]{source.read(source.chats()[0],{"message"},"2026-10-24T00:00:00Z","",control);},"earlier requested scope rejected");
 rejects([&]{source.read(source.chats()[0],{"message"},"","2026-10-26T00:00:00Z",control);},"later requested scope rejected");
 rejects([&]{source.read(source.chats()[0],{"message"},source.until(),source.since(),control);},"reversed requested scope rejected");
 rejects([&]{source.download(source.chats()[0],J::object(),root,control);},"source never supplies fabricated attachments");
 {
  Store store(root/"collection");auto count=collect(store,source,source.chats(),control,{"message","thread"},source.since(),source.until());
  require(count["items"]==5&&count["downloaded"]==0,"real collector consumes snapshot source");
  exportHtml(store);require(store.check().empty(),"imported database and HTML are internally valid");
  require(store.db.rows("SELECT count(*) AS n FROM items")[0]["n"]==5,"all source message rows persisted");
  require(store.db.rows("SELECT count(*) AS n FROM attachments")[0]["n"]==0,"unavailable source media not called saved files");
  auto history=store.history();require(history.size()==1&&history[0]["coverage"]=="local_only"&&history[0]["options"]["since"]==source.since(),"capture history records finite local bounds");
  auto firstId=store.db.rows("SELECT id FROM items WHERE source_id='a'")[0]["id"];
  collect(store,source,source.chats(),control,{"message","thread"},source.since(),source.until());
  require(store.db.rows("SELECT count(*) AS n FROM items")[0]["n"]==5&&store.db.rows("SELECT id FROM items WHERE source_id='a'")[0]["id"]==firstId,"repeat snapshot deduplicates stable IDs");
  auto updated=original;updated["messages"][1]["content"]="人工修正內容";auto updatedPath=root/"updated.json";atomicText(updatedPath,updated.dump());LineSummarySource next(updatedPath);
  collect(store,next,next.chats(),control,{"message","thread"},next.since(),next.until());
  require(store.db.rows("SELECT count(*) AS n FROM items")[0]["n"]==5&&store.db.rows("SELECT text FROM items WHERE source_id='a'")[0]["text"]=="人工修正內容","later snapshot updates existing source identity");
  exportHtml(store);require(store.check().empty(),"updated snapshot remains exportable");
 }
 require(digest(snapshot).hash==originalHash&&fs::status(snapshot).permissions()==originalPermissions,"reading and collecting never modifies snapshot bytes or permissions");
 // In-memory source stays bound to the accepted snapshot even if its path changes.
 atomicText(snapshot,"changed after loading");require(source.read(source.chats()[0],{"message"},"","",control).items.size()==5,"accepted source is immutable");atomicText(snapshot,original.dump());
 auto invalid=root/"invalid.json";
 auto rejectMutation=[&](const std::function<void(J&)>& mutate,const char* reason){auto value=original;mutate(value);atomicText(invalid,value.dump());rejects([&]{LineSummarySource candidate(invalid);},reason);};
 rejectMutation([](J& value){value["format"]="other";},"wrong format rejected");
 rejectMutation([](J& value){value["schema"]=true;},"boolean schema rejected");
 rejectMutation([](J& value){value["account_id"]="";},"empty account rejected");
 rejectMutation([](J& value){value["chat"]["source_id"]=2;},"incorrect chat identity type rejected");
 rejectMutation([](J& value){value["chat"]["kind"]="subchat";},"unproven community relation rejected");
 rejectMutation([](J& value){value["local_range_complete"]=false;},"incomplete pagination rejected");
 rejectMutation([](J& value){value["local_range_complete"]=1;},"integer completion flag rejected");
 rejectMutation([](J& value){value["since"]="2026-10-25T00:00:00";},"timezone-free range rejected");
 rejectMutation([](J& value){value["since"]="2026-10-25T00:00:00.000001Z";},"submillisecond coverage bounds rejected without silent rounding");
 rejectMutation([](J& value){value["until"]=value["since"];},"empty snapshot range rejected");
 rejectMutation([](J& value){value["messages"].push_back(value["messages"][0]);},"duplicate message identities rejected");
 rejectMutation([](J& value){value["messages"][0]["sent_at"]=value["until"];},"upper boundary message rejected");
 rejectMutation([](J& value){value["messages"][0]["sent_at"]="2026-10-24T23:59:59.999999999Z";},"lower out of range message rejected");
 rejectMutation([](J& value){value["messages"][0]["content"]=J::array();},"non-string text rejected");
 rejectMutation([](J& value){value["messages"][0]["sender"]=5;},"non-string sender rejected");
 rejectMutation([](J& value){value["messages"][0]["urls"]=J::array({5});},"malformed URL metadata rejected");
 rejectMutation([](J& value){value["messages"][0]["thread_id"]="thread";},"partial thread identity rejected");
 rejectMutation([](J& value){value["messages"][0]["invented_payload_path"]="payload.bin";},"unexpected fields rejected");
 rejectMutation([](J& value){value["messages"][0].erase("content");},"missing text content rejected");
 rejectMutation([](J& value){value["messages"]=J::object();},"non-array messages rejected");
 atomicText(invalid,"{\"format\":\"chatatlas.line-summary\",\"format\":\"replacement\"}");rejects([&]{LineSummarySource candidate(invalid);},"duplicate JSON object keys rejected");
 atomicText(invalid,"invalid JSON");rejects([&]{LineSummarySource candidate(invalid);},"malformed JSON rejected");
 atomicText(invalid,std::string(1,static_cast<char>(255)));rejects([&]{LineSummarySource candidate(invalid);},"invalid UTF-8 rejected");
 auto oversized=root/"oversized.json";atomicText(oversized,"");fs::resize_file(oversized,64ULL*1024*1024+1);rejects([&]{LineSummarySource candidate(oversized);},"oversized snapshot rejected before parsing");
 auto empty=original;empty["messages"]=J::array();auto emptyPath=root/"empty.json";atomicText(emptyPath,empty.dump());LineSummarySource emptySource(emptyPath);require(emptySource.read(emptySource.chats()[0],{"message"},"","",control).items.empty(),"completed empty local range accepted");
 Control stopped;stopped.stopped=true;
 cancels([&]{LineSummarySource candidate(root/"missing.json",&stopped);},"constructor cancellation checked before I/O");
 cancels([&]{source.read(source.chats()[0],{"message"},"","",stopped);},"read cancellation honored");
 cancels([&]{source.download(source.chats()[0],J::object(),root,stopped);},"download cancellation honored");
 {
  Store store(root/"cancelled");cancels([&]{collect(store,source,source.chats(),stopped,{"message"},source.since(),source.until());},"collector cancellation honored");
  require(store.db.rows("SELECT count(*) AS n FROM items")[0]["n"]==0&&store.historyCount()==0,"pre-cancelled capture records no partial import");
 }
#ifndef _WIN32
 auto link=root/"linked.json";fs::create_symlink(snapshot,link);rejects([&]{LineSummarySource candidate(link);},"snapshot symlink rejected");
#endif
 require(digest(snapshot).hash==originalHash,"all malformed and cancelled paths preserve accepted source");
 std::cout<<checks<<" line-summary snapshot checks passed; synthetic data only, live LINE not tested.\n";return 0;
 }catch(const std::exception& error){std::cerr<<"Line-summary snapshot test failed: "<<error.what()<<"\n";return 1;}}
