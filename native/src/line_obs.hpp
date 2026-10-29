#pragma once
#include "core.hpp"
#include "line_process.hpp"
#include <array>
#include <map>

namespace atlas {
// Protocol references are pinned in line_obs.cpp. These helpers never log or
// persist credentials and are also exercised with synthetic wire fixtures.
std::string lineObsTokenRequest();
std::string parseLineObsTokenReply(std::string_view);
uint32_t lineLegyHash(std::string_view,uint32_t seed=0);
std::array<unsigned char,4> lineLegyMac(std::string_view key,std::string_view ciphertext);
std::string lineLegyPlaintext(std::string_view token,std::string_view body);
struct LineLegyReply {
 std::map<std::string,std::string> headers;std::string body;
 LineLegyReply()=default;~LineLegyReply();
 LineLegyReply(const LineLegyReply&)=delete;LineLegyReply& operator=(const LineLegyReply&)=delete;
 LineLegyReply(LineLegyReply&&)=default;
};
LineLegyReply parseLineLegyReply(std::string_view);
// The key must be 16 bytes. CNG implementations are available on Windows only.
std::string sealLineLegy(std::string_view key,std::string_view plaintext);
std::string openLineLegy(std::string_view key,std::string_view ciphertext);
std::string wrapLineLegyKey(std::string_view key);
// Returns only an actual observed OBS token or the authenticated API result.
// The caller owns the returned secret and must erase it with eraseLineKey().
std::string acquireLineObsToken(const std::shared_ptr<LineSession>&,Control&);
}
