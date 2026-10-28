#include "core.hpp"
#include "database_bounds.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace atlas;
template<class F> void rejected(F fn){bool caught=false;try{fn();}catch(const Error&){caught=true;}assert(caught);}
int main(int argc,char**argv){
 assert(argc==2);const fs::path root=argv[1];assert(fs::is_directory(root));
 const auto path=root/"synthetic-db-contract.sqlite3";assert(!fs::exists(path));
 assert(databaseIntLength(0)==0);
 const auto maximum=static_cast<size_t>(std::numeric_limits<int>::max());
 assert(databaseIntLength(maximum)==std::numeric_limits<int>::max());
 rejected([&]{databaseIntLength(maximum+1);});
 rejected([&]{databaseIntLength(std::numeric_limits<size_t>::max());});
 // Simulate old SQLite returning no rows for unknown PRAGMAs.
 assert(!databasePragmaCapability(J::array(),"trusted_schema",0));
 assert(databasePragmaCapability(J::array({{{"trusted_schema",0}}}),"trusted_schema",0));
 rejected([&]{databasePragmaCapability(J::array({{{"trusted_schema",1}}}),"trusted_schema",0);});
 rejected([&]{databasePragmaCapability(J::array({{{"trusted_schema",false}}}),"trusted_schema",0);});
 rejected([&]{databasePragmaCapability(J::array({{{"unexpected",0}}}),"trusted_schema",0);});
 rejected([&]{databasePragmaCapability(J::array({{{"trusted_schema",0}},{{"trusted_schema",0}}}),"trusted_schema",0);});
 rejected([&]{databasePragmaCapability(J::object(),"trusted_schema",0);});
 {
  Db db(path);
  const auto protections=db.protectionStatus();
  assert(protections["trusted_schema_off"].is_boolean());
  assert(protections["cell_size_check"].is_boolean());
  assert(protections["trusted_schema_off"]==!db.rows("PRAGMA trusted_schema").empty());
  assert(protections["cell_size_check"]==!db.rows("PRAGMA cell_size_check").empty());
  if(protections["trusted_schema_off"]==true)assert(db.rows("PRAGMA trusted_schema")[0]["trusted_schema"]==0);
  if(protections["cell_size_check"]==true)assert(db.rows("PRAGMA cell_size_check")[0]["cell_size_check"]==1);
  assert(db.protectionNotice().empty()==(protections["trusted_schema_off"]==true&&protections["cell_size_check"]==true));
  assert(db.rows(" -- empty SQL\n").empty());
  rejected([&]{db.rows("",J::array({1}));});
  db.execute("CREATE TABLE fixture (id INTEGER PRIMARY KEY, value TEXT)");
  std::string embedded("a\0b",3);
  db.execute("INSERT INTO fixture(value) VALUES(?)",J::array({embedded}));
  assert(db.rows("SELECT value FROM fixture")[0]["value"].get<std::string>()==embedded);
  auto values=db.rows("SELECT NULL AS n, '' AS e, x'' AS b, 42 AS i");
  assert(values[0]["n"].is_null());assert(values[0]["e"]=="");assert(values[0]["b"]=="");assert(values[0]["i"]==42);
  rejected([&]{db.rows("SELECT ?",J::array({J::object()}));});
  rejected([&]{db.rows("SELECT 1",J::array({1}));});
  int noParameterTicks=0;assert(db.rows("SELECT 7 AS n",J::array(),[&]{++noParameterTicks;})[0]["n"]==7);assert(noParameterTicks>0);
  assert(db.rows("SELECT ? AS n",J(nullptr),[]{})[0]["n"].is_null());
  rejected([&]{db.rows("SELECT 1",J(nullptr),[]{});});
  rejected([&]{db.rows("SELECT 1",J::array({1}),[]{});});
  int rowTicks=0;bool rowCancelled=false;try{db.rows("WITH RECURSIVE counter(n) AS (VALUES(1) UNION ALL SELECT n+1 FROM counter WHERE n<50) SELECT n FROM counter",J::array(),[&]{if(++rowTicks==10)throw Cancelled();});}catch(const Cancelled&){rowCancelled=true;}assert(rowCancelled&&rowTicks==10);assert(db.rows("SELECT 1 AS ok")[0]["ok"]==1);
  rejected([&]{db.rows("not valid sql");});
  assert(db.rows("SELECT 1 AS ok")[0]["ok"]==1);
  bool reachedRollback=false,caughtRollback=false;
  try{db.transaction([&]{
   db.execute("INSERT INTO fixture(value) VALUES('rollback')");
   assert(db.rows("SELECT count(*) AS n FROM fixture")[0]["n"]==2);
   reachedRollback=true;throw Error("synthetic rollback sentinel");
  });}catch(const Error&e){caughtRollback=std::string(e.what())=="synthetic rollback sentinel";}
  assert(reachedRollback&&caughtRollback);
  assert(db.rows("SELECT count(*) AS n FROM fixture")[0]["n"]==1);
  const auto backup=root/"synthetic-db-backup.sqlite3";
  db.backup(backup);
  {Db copy(backup,false);assert(copy.protectionStatus()==protections);assert(copy.rows("SELECT count(*) AS n FROM fixture")[0]["n"]==1);}
  fs::remove(backup);
 }
 {Db db(path,false);assert(db.rows("SELECT count(*) AS n FROM fixture")[0]["n"]==1);if(db.protectionStatus()["trusted_schema_off"]==true)assert(db.rows("PRAGMA trusted_schema")[0]["trusted_schema"]==0);if(db.protectionStatus()["cell_size_check"]==true)assert(db.rows("PRAGMA cell_size_check")[0]["cell_size_check"]==1);}
 fs::remove(path);std::cout<<"database contract tests passed\n";
}
