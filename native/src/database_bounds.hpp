#pragma once
#include "core.hpp"
#include <limits>
namespace atlas {
inline int databaseIntLength(size_t value){
 if(value>static_cast<size_t>(std::numeric_limits<int>::max()))throw Error("資料庫內容超過可處理長度。");
 return static_cast<int>(value);
}
}
