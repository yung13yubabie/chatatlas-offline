#include "line_obs.hpp"
#include <iostream>
#include <stdexcept>
using namespace atlas;
namespace {
int checks=0;
void require(bool condition,const char* text){++checks;if(!condition)throw std::runtime_error(text);}
template<class F>void rejects(F action,const char* text){bool refused=false;try{action();}catch(const Error&){refused=true;}require(refused,text);}
std::string unhex(std::string_view input){std::string result;for(size_t at=0;at<input.size();at+=2)result.push_back(char(std::stoul(std::string(input.substr(at,2)),nullptr,16)));return result;}
std::string hex(std::string_view input){static constexpr char digits[]="0123456789abcdef";std::string result;for(unsigned char c:input){result.push_back(digits[c>>4]);result.push_back(digits[c&15]);}return result;}
std::string reply(std::string_view token){
 auto request=lineObsTokenRequest();std::string result=request.substr(0,31);result[1]=char(0x41);result.push_back(char(8));result.push_back(0);result.push_back(char(token.size()));result.append(token);result.push_back(0);return result;
}
void short16(std::string& bytes,size_t value){bytes.push_back(char(value>>8));bytes.push_back(char(value));}
std::string framed(std::string_view data,std::string_view status="200"){
 std::string fields;short16(fields,1);short16(fields,4);fields+="x-lc";short16(fields,status.size());fields+=status;
 std::string result(1,char(7));short16(result,fields.size());result+=fields;result+=data;return result;
}
}
int main(){try{
 const auto rpc=unhex("8221001b61637175697265456e63727970746564416363657373546f6b656e250400");
 require(lineObsTokenRequest()==rpc,"compact featureType2 RPC matches primary protocol byte fixture");
 require(lineLegyHash("")==0x02cc5d05,"xxhash32 empty primary-code vector");
 require(lineLegyHash("a")==0x550d7456,"xxhash32 one-byte primary-code vector");
 require(lineLegyHash("abc")==0x32d153ff,"xxhash32 short primary-code vector");
 require(lineLegyHash("The quick brown fox jumps over the lazy dog")==0xe85ea4de,"xxhash32 long primary-code vector");
 const auto plaintext=unhex("07002400020006782d6c70717300032f53340004782d6c74000d666978747572652d746f6b656e8221001b61637175697265456e63727970746564416363657373546f6b656e250400");
 require(lineLegyPlaintext("fixture-token",rpc)==plaintext,"LEG Y request header framing matches independent Node fixture");
 auto parsed=parseLineLegyReply(plaintext);require(parsed.headers.at("x-lt")=="fixture-token"&&parsed.headers.at("x-lpqs")=="/S4"&&parsed.body==rpc,"LEG Y bounded header decoder preserves binary RPC body");
 const auto sealed=unhex("83e8d35e890389ee3eb031d4421f58a9a88b6aaa6bc215ce24055620c9ae8aa8dc058a9b8880c9d486654e03ce2f872dc90303d8e2163389f845be2858604a3d650f4a7ac9161f2aaa2c14a07032b5bd9a032b89");
 const std::string key="0123456789abcdef";auto mac=lineLegyMac(key,std::string_view(sealed).substr(0,sealed.size()-4));
 require(hex(std::string_view(reinterpret_cast<const char*>(mac.data()),mac.size()))=="9a032b89","LEG Y MAC matches pinned primary-code Node vector");
 require(parseLineObsTokenReply(reply("fixture-obs-token"))=="fixture-obs-token","compact success field0 extracts only OBS token");
 require(parseLineObsTokenReply(reply(std::string("descriptor")+char(0x1e)+"fixture-obs-token"))=="fixture-obs-token","OBS token record separator follows CHRLINE result convention");
 auto extra=reply("fixture-obs-token");extra.pop_back();extra.append("\x29\x21\x01\x02\x00",5);require(parseLineObsTokenReply(extra)=="fixture-obs-token","bounded compact skip handles unknown boolean collections");
 auto duplicate=reply("fixture-obs-token");duplicate.pop_back();duplicate.append("\x08\x00\x01x\x00",5);rejects([&]{parseLineObsTokenReply(duplicate);},"duplicate success fields refused");
 auto exception=rpc.substr(0,31);exception[1]=char(0x41);exception.append("\x1c\x00\x00",3);rejects([&]{parseLineObsTokenReply(exception);},"service exception refused without exposing response text");
 auto wrong=reply("fixture-obs-token");wrong[1]=char(0x21);rejects([&]{parseLineObsTokenReply(wrong);},"non-reply message refused");
 wrong=reply("fixture-obs-token");wrong[2]=1;rejects([&]{parseLineObsTokenReply(wrong);},"mismatched sequence refused");
 rejects([&]{parseLineObsTokenReply(reply("bad\ntoken"));},"header injection token refused");
 rejects([&]{parseLineObsTokenReply(reply(""));},"empty token refused");
 wrong=reply("fixture-obs-token");wrong.push_back('x');rejects([&]{parseLineObsTokenReply(wrong);},"unconsumed wire tail refused");
 for(size_t size=0;size<rpc.size();++size)rejects([&]{parseLineObsTokenReply(std::string_view(rpc).substr(0,size));},"truncated compact input safely refused");
 auto wire=framed(reply("fixture-obs-token"));auto unpacked=parseLineLegyReply(wire);require(parseLineObsTokenReply(unpacked.body)=="fixture-obs-token","success LEG Y response envelope chains into compact reply parser");
 rejects([&]{parseLineLegyReply(framed("rejected","403"));},"inner LINE error status refused");
 rejects([&]{parseLineLegyReply(std::string(1,char(7))+std::string(4,char(255)));},"out-of-bound LEG Y lengths refused");
 rejects([&]{lineLegyPlaintext("bad\r\ntoken",rpc);},"request token header controls refused");
 rejects([&]{lineLegyMac("short",rpc);},"MAC key size checked");
 Control control;rejects([&]{acquireLineObsToken({},control);},"missing live verified session does not trigger network access");
 Control stopped;stopped.stopped=true;bool cancelled=false;try{acquireLineObsToken({},stopped);}catch(const Cancelled&){cancelled=true;}require(cancelled,"cancelled authorization stops before session or network access");
#ifdef _WIN32
 require(sealLineLegy(key,plaintext)==sealed,"CNG AES128CBC plus protocol MAC matches independent Node primary-code fixture");
 require(openLineLegy(key,sealed)==plaintext,"CNG verifies MAC and decrypts the known response fixture");
 for(size_t at:{size_t(0),sealed.size()-1}){auto corrupt=sealed;corrupt[at]^=1;rejects([&]{openLineLegy(key,corrupt);},"tampered ciphertext or MAC rejected before plaintext is returned");}
 rejects([&]{openLineLegy(key,std::string_view(sealed).substr(0,sealed.size()-1));},"missing response MAC bytes rejected");
 auto wrapped=wrapLineLegyKey(key);require(wrapped.starts_with("0008")&&wrapped.size()==348,"CNG imports pinned LINE RSA key and wraps a native ephemeral transport key with OAEP-SHA1");
 require(wrapLineLegyKey(key)!=wrapped,"RSA OAEP uses fresh random padding");
#else
 rejects([&]{sealLineLegy(key,plaintext);},"non-Windows system never substitutes an external crypto runtime");
#endif
 std::cout<<checks<<" native OBS/LEG Y protocol checks passed; synthetic bytes only, no live LINE request.\n";return 0;
 }catch(const std::exception& error){std::cerr<<"Native OBS protocol test failed: "<<error.what()<<"\n";return 1;}}
