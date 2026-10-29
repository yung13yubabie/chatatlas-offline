#include "line_cipher.hpp"
#include "database_bounds.hpp"
#include "file.hpp"
#include <sqlite3mc_amalgamation.h>
#include <algorithm>
#include <cstring>
#include <exception>

namespace atlas {
namespace {
void tickNow(const Tick& tick){if(tick)tick();}
struct Progress {
 Tick tick;std::exception_ptr failure;
 static int invoke(void* data) noexcept {auto& self=*static_cast<Progress*>(data);try{tickNow(self.tick);return 0;}catch(...){self.failure=std::current_exception();return 1;}}
};
int authorize(void*,int action,const char* first,const char* second,const char*,const char*) noexcept {
 if(action==SQLITE_ATTACH||action==SQLITE_DETACH)return SQLITE_DENY;
 if(action==SQLITE_PRAGMA){
  const auto is=[&](const char* name){return first&&sqlite3_stricmp(first,name)==0;};
  if(is("table_info")||is("table_xinfo")||is("index_list")||is("index_info")||is("index_xinfo")||is("foreign_key_list"))return SQLITE_OK;
  if(second)return SQLITE_DENY;
  if(!is("query_only")&&!is("trusted_schema")&&!is("cell_size_check")&&!is("cipher")&&!is("schema_version")&&!is("user_version")&&!is("integrity_check")&&!is("quick_check"))return SQLITE_DENY;
 }
 return SQLITE_OK;
}
bool keyFormat(const std::string& key){return key.size()==32&&std::all_of(key.begin(),key.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');});}
void verifyEncryptedHeader(const fs::path& path){
 char header[16]{};uint64_t length=0;
#ifdef _WIN32
 // A running LINE owns a write-capable SQLite handle. Sharing that existing
 // handle is necessary even though this connection itself never requests writes.
 requireLocalPrivatePath(path);const auto expected=expectedFilePath(path);
 HANDLE input=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(input==INVALID_HANDLE_VALUE)throw CapabilityError("LINE 本機資料庫無法讀取，請確認讀取權限與檔案使用狀態。");
 try{
  verifyFileHandle(input,expected);BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};DWORD count=0;
  if(GetFileType(input)!=FILE_TYPE_DISK||!GetFileInformationByHandle(input,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||!GetFileSizeEx(input,&size)||size.QuadPart<0||!ReadFile(input,header,sizeof header,&count,nullptr)||count!=sizeof header)throw CapabilityError("LINE 本機資料庫不完整或不是一般檔案。");
  length=static_cast<uint64_t>(size.QuadPart);
 }catch(...){CloseHandle(input);throw;}
 CloseHandle(input);
#else
 VerifiedInput input(path);length=input.size();input.read(header,sizeof header);
 if(input.gcount()!=sizeof header)throw CapabilityError("LINE 本機資料庫不完整。");
#endif
 if(length<512||length%512)throw CapabilityError("LINE 本機資料庫大小不完整，請確認選取的是有效的 .edb 資料庫。");
 if(!std::memcmp(header,"SQLite format 3\0",16))throw CapabilityError("選取的是一般 SQLite，並非支援的 LINE 加密資料庫。");
}
}
void LineCipher::preflight(const fs::path& path,Control* control){
 if(control)control->check();
#ifdef _WIN32
 requireLocalPrivatePath(path);
#endif
 noLinks(path);std::error_code error;
 if(!fs::is_regular_file(path,error))throw CapabilityError("找不到可讀取的 LINE 本機資料庫，請重新選擇 .edb 檔案。");
 verifyEncryptedHeader(path);
 if(control)control->check();
 const int cipher=sqlite3mc_cipher_index("aes128cbc");
 const auto name=cipher>0?sqlite3mc_cipher_name(cipher):nullptr;
 if(!name||std::strcmp(name,"aes128cbc"))throw CapabilityError("內建 LINE 資料庫解密元件不相容。");
 if(control)control->check();
}
LineCipher::LineCipher(const fs::path& path,const std::string& passphrase,Control* control,bool createForTest):fixture(createForTest){
 if(control)control->check();
 if(!keyFormat(passphrase))throw CapabilityError("LINE 資料庫金鑰格式不相容。");
 if(!fixture)preflight(path,control);
 else{
#ifdef _WIN32
  requireLocalPrivatePath(path);
#endif
  noLinks(path);if(fs::exists(path))throw CapabilityError("人工資料庫必須使用新的檔案位置。");
 }
 sqlite3* opened=nullptr;
 if(sqlite3_open_v2(utf8(path).c_str(),&opened,(fixture?SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE:SQLITE_OPEN_READONLY)|SQLITE_OPEN_NOMUTEX|SQLITE_OPEN_EXRESCODE,nullptr)!=SQLITE_OK){if(opened)sqlite3_close_v2(opened);throw CapabilityError("LINE 本機資料庫無法開啟，請確認檔案與讀取權限。");}
 handle=opened;
 try{
  if(!fixture&&sqlite3_db_readonly(opened,"main")!=1)throw CapabilityError("LINE 資料庫未能以唯讀模式開啟。");
  if(sqlite3_busy_timeout(opened,3000)!=SQLITE_OK)throw CapabilityError("LINE 資料庫讀取等候設定失敗。");
  const int cipher=sqlite3mc_cipher_index("aes128cbc");
  if(cipher<=0||sqlite3mc_config(opened,"cipher",cipher)!=cipher||sqlite3mc_config(opened,"cipher",-1)!=cipher)throw CapabilityError("內建 LINE 資料庫解密元件不相容。");
  if(sqlite3mc_config_cipher(opened,"aes128cbc","legacy",0)!=0)throw CapabilityError("LINE 資料庫加密格式不相容。");
  if(sqlite3_key(opened,passphrase.data(),static_cast<int>(passphrase.size()))!=SQLITE_OK)throw CapabilityError("LINE 資料庫金鑰無法套用。");
  auto check=[&]{if(control)control->check();};
  const auto memory=rows("PRAGMA memory_security=1",J::array(),check);
  if(memory.empty()||memory[0].begin().value()!="1")throw CapabilityError("LINE 解密元件記憶體保護設定失敗。");
  rows("SELECT count(*) AS n FROM sqlite_schema",J::array(),check);
  int ignored=0;
  if(sqlite3_db_config(opened,SQLITE_DBCONFIG_TRUSTED_SCHEMA,0,&ignored)!=SQLITE_OK||sqlite3_db_config(opened,SQLITE_DBCONFIG_DEFENSIVE,1,&ignored)!=SQLITE_OK)throw CapabilityError("LINE 資料庫保護設定失敗。");
  rows("PRAGMA cell_size_check=ON",J::array(),check);
  if(!fixture){rows("PRAGMA query_only=ON",J::array(),check);if(sqlite3_set_authorizer(opened,authorize,nullptr)!=SQLITE_OK)throw CapabilityError("LINE 資料庫唯讀保護設定失敗。");}
  check();
 }catch(...){sqlite3_close_v2(opened);handle=nullptr;throw;}
}
LineCipher::~LineCipher(){if(handle)sqlite3_close_v2(static_cast<sqlite3*>(handle));}
std::string LineCipher::version(){return sqlite3mc_version();}
J LineCipher::rows(const std::string& query,const J& supplied,Tick tick){
 tickNow(tick);if(!handle)throw CapabilityError("LINE 資料庫連線已關閉。");
 if(query.find('\0')!=std::string::npos)throw CapabilityError("LINE 資料庫查詢格式不正確。");
 const auto length=databaseIntLength(query.size());
 J args=supplied.is_array()?supplied:J::array({supplied});databaseIntLength(args.size());
 auto db=static_cast<sqlite3*>(handle);sqlite3_stmt* statement=nullptr;
 struct Final {sqlite3* db;sqlite3_stmt*& statement;~Final(){sqlite3_progress_handler(db,0,nullptr,nullptr);if(statement)sqlite3_finalize(statement);}} finish{db,statement};
 Progress progress{std::move(tick),{}};sqlite3_progress_handler(db,1000,Progress::invoke,&progress);
 const char* tail=nullptr;
 int result=sqlite3_prepare_v2(db,query.c_str(),length,&statement,&tail);
 if(progress.failure)std::rethrow_exception(progress.failure);
 if(result!=SQLITE_OK)throw CapabilityError("LINE 本機資料庫無法讀取，請確認版本、金鑰或檔案完整性。");
 if(tail&&std::any_of(tail,query.c_str()+query.size(),[](unsigned char c){return c!=' '&&c!='\t'&&c!='\r'&&c!='\n';}))throw CapabilityError("LINE 資料庫查詢只能包含單一指令。");
 if(!statement){if(!args.empty())throw CapabilityError("LINE 查詢參數格式不正確。");return J::array();}
 if(!fixture&&!sqlite3_stmt_readonly(statement))throw CapabilityError("LINE 本機資料庫只允許唯讀查詢。");
 if(sqlite3_bind_parameter_count(statement)!=static_cast<int>(args.size()))throw CapabilityError("LINE 查詢參數數量不正確。");
 for(size_t index=0;index<args.size();++index){
  tickNow(progress.tick);const auto& value=args[index];int status=SQLITE_MISUSE;const auto at=static_cast<int>(index+1);
  if(value.is_null())status=sqlite3_bind_null(statement,at);
  else if(value.is_boolean())status=sqlite3_bind_int(statement,at,value.get<bool>());
  else if(value.is_number_integer()&&databaseSignedInteger(value))status=sqlite3_bind_int64(statement,at,value.get<int64_t>());
  else if(value.is_number_float())status=sqlite3_bind_double(statement,at,value.get<double>());
  else if(value.is_string()){const auto& text=value.get_ref<const std::string&>();status=sqlite3_bind_text(statement,at,text.data(),databaseIntLength(text.size()),SQLITE_TRANSIENT);}
  else if(value.is_binary()){const auto& bytes=value.get_binary();status=bytes.empty()?sqlite3_bind_zeroblob(statement,at,0):sqlite3_bind_blob(statement,at,bytes.data(),databaseIntLength(bytes.size()),SQLITE_TRANSIENT);}
  if(status!=SQLITE_OK)throw CapabilityError("LINE 查詢參數格式不正確。");
 }
 J output=J::array();
 while(true){
  tickNow(progress.tick);result=sqlite3_step(statement);
  if(progress.failure)std::rethrow_exception(progress.failure);
  if(result!=SQLITE_ROW)break;
  J row=J::object();
  for(int column=0;column<sqlite3_column_count(statement);++column){
   tickNow(progress.tick);const auto name=sqlite3_column_name(statement,column);if(!name)throw CapabilityError("LINE 資料庫讀取記憶體不足。");
   switch(sqlite3_column_type(statement,column)){
    case SQLITE_NULL:row[name]=nullptr;break;
    case SQLITE_INTEGER:row[name]=sqlite3_column_int64(statement,column);break;
    case SQLITE_FLOAT:row[name]=sqlite3_column_double(statement,column);break;
    case SQLITE_BLOB:{const auto size=sqlite3_column_bytes(statement,column);const auto bytes=static_cast<const unsigned char*>(sqlite3_column_blob(statement,column));if(sqlite3_errcode(db)==SQLITE_NOMEM||(!bytes&&size))throw CapabilityError("LINE 資料庫讀取記憶體不足。");row[name]=J::binary(bytes?std::vector<unsigned char>(bytes,bytes+size):std::vector<unsigned char>{});break;}
    default:{const auto bytes=sqlite3_column_text(statement,column);if(sqlite3_errcode(db)==SQLITE_NOMEM)throw CapabilityError("LINE 資料庫讀取記憶體不足。");const auto size=sqlite3_column_bytes(statement,column);if(sqlite3_errcode(db)==SQLITE_NOMEM||(!bytes&&size))throw CapabilityError("LINE 資料庫內容無法讀取。");row[name]=bytes?std::string(reinterpret_cast<const char*>(bytes),size):std::string();break;}
   }
  }
  output.push_back(std::move(row));
 }
 if(result!=SQLITE_DONE)throw CapabilityError("LINE 資料庫讀取未完成，請確認版本、金鑰或檔案完整性。");
 const int final=sqlite3_finalize(statement);statement=nullptr;if(final!=SQLITE_OK)throw CapabilityError("LINE 資料庫讀取未完成。");
 tickNow(progress.tick);return output;
}
}
