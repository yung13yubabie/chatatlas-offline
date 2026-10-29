#include "core.hpp"
#include "line_summary.hpp"
#include <iostream>
using namespace atlas;
int main(int argc,char**argv){try{
 if(argc<3){std::cout<<"ChatAtlas native core: demo/check/rebuild <directory>, backup <directory> <zip>, line-summary <snapshot.json> <directory>\n";return 2;}
 std::string command=argv[1];fs::path root=path8(argv[2]);
 if(command=="line-summary"){
  if(argc!=4)return 2;
  Control control;LineSummarySource source(root,&control);
  Store s(path8(argv[3]));std::string warning;
  auto result=collect(s,source,source.chats(),control,{"message","thread"},source.since(),source.until());
  exportHtml(s,{},&warning);std::cout<<result.dump()<<"\n";
  if(!warning.empty())std::cerr<<warning<<"\n";
  std::cerr<<"已匯入本機期間的對話與附件文字標記；未下載附件原檔、相簿或記事本。\n";
  return 0;
 }
 if(command!="demo"&&!fs::is_regular_file(root/"archive.sqlite3"))throw Error("這個資料夾還沒有收藏。");
 Store s(root);const auto notice=s.db.protectionNotice();if(!notice.empty())std::cerr<<notice<<"\n";
 Control control;std::string warning;
 if(command=="demo"){
  DemoSource source(argc>=4&&std::string(argv[3])=="2"?2:1);auto result=collect(s,source,source.chats(),control);
  exportHtml(s,{},&warning);std::cout<<result.dump()<<"\n";
 }else if(command=="check"){
  auto problems=s.check();std::cout<<problems.dump()<<"\n";return problems.empty()?0:1;
 }else if(command=="rebuild"){
  exportHtml(s,{},&warning);std::cout<<"知識地圖已產生。\n";
 }else if(command=="backup"&&argc==4){
  auto retained=makeZip(s,path8(argv[3]),{},&warning);std::cout<<"副本已保存。\n";
  if(!retained.empty())std::cerr<<"備份已完成，但私人資料庫暫存副本未能移除，確認不需要後可手動清理："<<utf8(retained)<<"\n";
 }else return 2;
 if(!warning.empty())std::cerr<<warning<<"\n";
 return 0;
}catch(const Error&e){std::cerr<<e.what()<<"\n";return 2;}
catch(...){std::cerr<<"操作未完成，已保存的內容仍在。\n";return 2;}}
