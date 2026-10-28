#include "core.hpp"
#include <iostream>
using namespace atlas;
namespace {int checks=0;void expect(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}template<class F>void cancelled(F fn){bool stopped=false;try{fn();}catch(const Cancelled&){stopped=true;}expect(stopped,"expected cooperative cancellation, not another failure");}}
int main(int argc,char**argv){try{
 if(argc!=2)throw Error("Provide one empty owned fixture directory");auto base=fs::absolute(path8(argv[1]));if(!fs::is_directory(base)||!fs::is_empty(base))throw Error("Fixture directory must exist and be empty");
 Store store(base/"collection");std::string cid;store.db.transaction([&]{cid=store.putChat("synthetic-cancel-account",{{"source_id","room"},{"title","Artificial cancellation"},{"kind","group"}});store.putItems(cid,J::array({{{"source_id","original"},{"kind","message"},{"text","original text"}}}));});
 int rowTicks=0;cancelled([&]{store.db.rows("WITH RECURSIVE counter(n) AS (VALUES(1) UNION ALL SELECT n+1 FROM counter WHERE n<50) SELECT n FROM counter",{},[&]{if(++rowTicks==10)throw Cancelled();});});expect(rowTicks==10,"row enumeration cancels after materializing multiple rows");expect(store.db.rows("SELECT 1 AS n")[0]["n"]==1,"cancelled row query finalizes its statement and connection remains usable");
 auto original=store.db.rows("SELECT * FROM items");J incoming=J::array();for(int i=0;i<30;i++)incoming.push_back({{"source_id","new-"+std::to_string(i)},{"kind","message"},{"text","synthetic pending text"}});
 bool sawInsertion=false;cancelled([&]{store.db.transaction([&]{store.putItems(cid,incoming,[&]{if(store.db.rows("SELECT count(*) AS n FROM items")[0]["n"].get<int>()>1){sawInsertion=true;throw Cancelled();}});});});
 expect(sawInsertion,"cancellation happens after a real row insertion");expect(store.db.rows("SELECT * FROM items")==original,"cancelled batch rolls back all inserted rows and preserves existing content");expect(store.check({},false).empty(),"cancelled batch preserves valid relations");
 int ticks=0;cancelled([&]{store.identityProblems([&]{if(++ticks==5)throw Cancelled();});});expect(ticks==5,"identity traversal observes bounded ticks");
 ticks=0;cancelled([&]{store.check([&]{if(++ticks==5)throw Cancelled();},false);});expect(ticks==5,"integrity traversal observes bounded ticks");
 const auto page=exportHtml(store);auto before=digest(page).hash;ticks=0;cancelled([&]{exportHtml(store,[&]{if(++ticks==8)throw Cancelled();});});expect(digest(page).hash==before&&store.db.rows("SELECT * FROM items")==original,"cancelled export preserves prior page and all stored items");
 std::cout<<checks<<" cooperative Store cancellation checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
