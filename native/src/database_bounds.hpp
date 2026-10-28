#pragma once
#include "core.hpp"
#include <limits>
namespace atlas {
inline int databaseIntLength(size_t value){
 if(value>static_cast<size_t>(std::numeric_limits<int>::max()))throw Error("資料庫內容超過可處理長度。");
 return static_cast<int>(value);
}
// Unknown PRAGMAs succeed without rows on older SQLite versions. A successful
// execute alone is therefore not evidence that the connection was hardened.
inline bool databasePragmaCapability(const J& rows,const char* name,int expected){
 if(rows.is_array()&&rows.empty())return false; // Unsupported, not a successful protection.
 if(!rows.is_array()||rows.size()!=1||!rows[0].is_object()||rows[0].size()!=1||
    !rows[0].contains(name)||!rows[0][name].is_number_integer()||rows[0][name]!=expected)
  throw Error("資料庫元件未能啟用已支援的保護，未繼續開啟收藏。");
 return true;
}
}
