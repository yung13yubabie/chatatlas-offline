#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace atlas::zip_bounds {
// ZIP64 stores these values in 64 bits. Never let a counter wrap before encoding.
inline uint64_t add(uint64_t a,uint64_t b){
 if(b>std::numeric_limits<uint64_t>::max()-a)throw std::overflow_error("ZIP64 size limit");
 return a+b;
}
inline uint64_t memberEnd(uint64_t offset,uint64_t nameBytes,uint64_t size){
 if(nameBytes==0||nameBytes>65535)throw std::overflow_error("ZIP filename size limit");
 return add(add(add(offset,50),nameBytes),size);
}
inline uint64_t directoryEnd(uint64_t start,uint64_t directoryBytes){return add(add(start,directoryBytes),98);}
inline uint64_t directoryEntry(uint64_t nameBytes){
 if(nameBytes==0||nameBytes>65535)throw std::overflow_error("ZIP filename size limit");
 return add(74,nameBytes);
}
inline uint64_t copySize(uint64_t copied,uint64_t chunk,uint64_t expected){
 auto next=add(copied,chunk);
 if(next>expected)throw std::runtime_error("ZIP source grew during copy");
 return next;
}
inline uint16_t legacyCount(uint64_t value){return value>=0xffff?0xffff:static_cast<uint16_t>(value);}
inline uint32_t legacySize(uint64_t value){return value>=0xffffffff?0xffffffff:static_cast<uint32_t>(value);}
}
