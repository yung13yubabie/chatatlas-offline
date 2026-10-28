#include "core.hpp"
#include "database_bounds.hpp"
#include <algorithm>
#include <cstring>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace atlas {
struct SqlApi{void*lib=nullptr;
 int(*errorCode)(void*);int(*open)(const char*,void**,int,const char*);int(*close)(void*);int(*prepare)(void*,const char*,int,void**,const char**);int(*step)(void*);int(*finalize)(void*);int(*bindText)(void*,int,const char*,int,void(*)(void*));int(*bindInt)(void*,int,int64_t);int(*bindNull)(void*,int);int(*columns)(void*);const char*(*name)(void*,int);int(*type)(void*,int);const unsigned char*(*text)(void*,int);int(*bytes)(void*,int);int64_t(*integer)(void*,int);int64_t(*last)(void*);int(*busy)(void*,int);void*(*backupInit)(void*,const char*,void*,const char*);int(*backupStep)(void*,int);int(*backupFinish)(void*);
 template<class T>void symbol(T&target,const char*name){
#ifdef _WIN32
  target=reinterpret_cast<T>(GetProcAddress(static_cast<HMODULE>(lib),name));
#else
  target=reinterpret_cast<T>(dlsym(lib,name));
#endif
  if(!target)throw Error("這台電腦的資料庫元件不相容。");}
 SqlApi(){
#ifdef _WIN32
  lib=LoadLibraryExW(L"winsqlite3.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
  lib=dlopen("libsqlite3.so.0",RTLD_NOW|RTLD_LOCAL);
#endif
  if(!lib)throw Error("這台電腦缺少可用的資料庫元件。");
try {
#define S(field,name) symbol(field,"sqlite3_" name)
  S(errorCode,"errcode");S(open,"open_v2");S(close,"close_v2");S(prepare,"prepare_v2");S(step,"step");S(finalize,"finalize");S(bindText,"bind_text");S(bindInt,"bind_int64");S(bindNull,"bind_null");S(columns,"column_count");S(name,"column_name");S(type,"column_type");S(text,"column_text");S(bytes,"column_bytes");S(integer,"column_int64");S(last,"last_insert_rowid");S(busy,"busy_timeout");S(backupInit,"backup_init");S(backupStep,"backup_step");S(backupFinish,"backup_finish");
#undef S
  } catch(...) {
#ifdef _WIN32
   FreeLibrary(static_cast<HMODULE>(lib));
#else
   dlclose(lib);
#endif
   lib=nullptr;throw;
  }
 }
};static SqlApi&sql(){static SqlApi api;return api;}
Db::Db(const fs::path&p,bool create){noLinks(p);auto&s=sql();int rc=s.open(utf8(p).c_str(),&db,(create?2|4:1)|0x8000,nullptr);if(rc){if(db)s.close(db);db=nullptr;throw Error("收藏資料庫無法開啟，請保留原資料夾後再試。");}try {
 if(s.busy(db,3000))throw Error("資料庫等候設定失敗。");
 }catch(...){s.close(db);db=nullptr;throw;}
}
Db::~Db(){if(db)sql().close(db);}
J Db::rows(const std::string&query,const J&givenArgs){
 const int queryLength=databaseIntLength(query.size());
 J args=givenArgs.is_array()?givenArgs:J::array({givenArgs});
 databaseIntLength(args.size());
 auto&s=sql();void*statement=nullptr;
 struct Final{void*&p;~Final(){if(p)sql().finalize(p);}}finish{statement};
 if(s.prepare(db,query.c_str(),queryLength,&statement,nullptr))throw Error("收藏資料庫無法讀取。");
 if(!statement){if(!args.empty())throw Error("保存內容格式不正確。");return J::array();}
 for(size_t i=0;i<args.size();i++){
  const auto&a=args[i];const int index=databaseIntLength(i+1);int rc;
  if(a.is_null())rc=s.bindNull(statement,index);
  else if(a.is_number_integer())rc=s.bindInt(statement,index,a.get<int64_t>());
  else if(a.is_boolean())rc=s.bindInt(statement,index,a.get<bool>());
  else if(a.is_string()){const auto&str=a.get_ref<const std::string&>();rc=s.bindText(statement,index,str.data(),databaseIntLength(str.size()),reinterpret_cast<void(*)(void*)>(-1));}
  else throw Error("保存內容格式不正確。");
  if(rc)throw Error("保存內容未能完成。");
 }
 J result=J::array();int rc;
 while((rc=s.step(statement))==100){
  J row=J::object();
  for(int c=0;c<s.columns(statement);c++){
   const auto name=s.name(statement,c);
   if(!name)throw Error("資料庫讀取記憶體不足。");
   const int type=s.type(statement,c);
   if(type==5)row[name]=nullptr;
   else if(type==1)row[name]=s.integer(statement,c);
   else{
    const auto p=s.text(statement,c);
    // SQLite documents NULL and zero byte counts on conversion OOM.
    // Inspect the connection immediately, before another SQLite call.
    if(s.errorCode(db)==7)throw Error("資料庫讀取記憶體不足。");
    const int count=s.bytes(statement,c);
    if(s.errorCode(db)==7 || count<0 || (!p && count))throw Error("資料庫內容未能讀取。");
    row[name]=p?std::string(reinterpret_cast<const char*>(p),static_cast<size_t>(count)):std::string();
   }
  }
  result.push_back(std::move(row));
 }
 if(rc!=101)throw Error("資料關聯或內容檢查未通過，已保留原資料。");
 const int final=s.finalize(statement);statement=nullptr;
 if(final)throw Error("資料庫讀取未能完成。");
 return result;
}
void Db::execute(const std::string&q,const J&a){rows(q,a);}void Db::exec(const std::string&text){size_t from=0;while(from<text.size()){auto end=text.find(';',from);auto q=text.substr(from,end==std::string::npos?end:end-from);if(q.find_first_not_of(" \n\r\t")!=std::string::npos)execute(q);if(end==std::string::npos)break;from=end+1;}}
void Db::transaction(const std::function<void()>&fn){execute("BEGIN IMMEDIATE");try{fn();execute("COMMIT");}catch(...){try{execute("ROLLBACK");}catch(...){}throw;}}int64_t Db::lastId(){return sql().last(db);}
void Db::backup(const fs::path&dest,Tick tick){Db target(dest);auto&s=sql();void*b=s.backupInit(target.db,"main",db,"main");if(!b)throw Error("備份尚未完成。");try{int code;do{if(tick)tick();code=s.backupStep(b,128);if(code==5||code==6)throw Error("資料正在使用中，請稍後備份。");}while(code==0);if(code!=101)throw Error("備份尚未完成。");int final=s.backupFinish(b);b=nullptr;if(final)throw Error("備份尚未完成。");}catch(...){if(b)s.backupFinish(b);throw;}}
}
