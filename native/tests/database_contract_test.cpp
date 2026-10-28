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
 {
  Db db(path);
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
  rejected([&]{db.rows("not valid sql");});
  assert(db.rows("SELECT 1 AS ok")[0]["ok"]==1);
  try{db.transaction([&]{db.execute("INSERT INTO fixture(value) VALUES('rollback')");throw Error("test");});}catch(const Error&){}
  assert(db.rows("SELECT count(*) AS n FROM fixture")[0]["n"]==1);

 }
 {Db db(path,false);assert(db.rows("SELECT count(*) AS n FROM fixture")[0]["n"]==1);}
 fs::remove(path);std::cout<<"database contract tests passed\n";
}
