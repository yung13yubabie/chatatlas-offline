#include "line_media.hpp"
#include "file.hpp"
#include "line_process.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#ifdef _WIN32
#include <winhttp.h>
#include <bcrypt.h>
#include "line_obs.hpp"
#endif

namespace atlas {
namespace {
constexpr uint64_t httpLimit=1024ULL*1024*1024;
void erase(void* value,size_t count)noexcept{auto bytes=static_cast<volatile unsigned char*>(value);while(count--)*bytes++=0;}
void erase(std::string& value)noexcept{if(!value.empty())erase(value.data(),value.size());value.clear();}
template<class T>struct Wipe{T& value;~Wipe(){erase(value.data(),value.size()*sizeof(value[0]));}};
std::array<unsigned char,32> hashBytes(Sha256& hash){
 auto encoded=hash.finish();std::array<unsigned char,32> bytes{};
 auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c-'a'+10;};
 for(size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<unsigned char>((digit(encoded[2*i])<<4)|digit(encoded[2*i+1]));erase(encoded);return bytes;
}
class Hmac {
 Sha256 inner;std::array<unsigned char,64> outer{};
 public:
 Hmac(const unsigned char* key,size_t length){std::array<unsigned char,64> padded{};Wipe wipe{padded};if(length>64){Sha256 hash;hash.update(key,length);auto shortened=hashBytes(hash);std::copy(shortened.begin(),shortened.end(),padded.begin());erase(shortened.data(),shortened.size());}else std::copy(key,key+length,padded.begin());for(size_t i=0;i<64;++i){outer[i]=padded[i]^0x5c;padded[i]^=0x36;}inner.update(padded.data(),padded.size());}
 void update(const void* data,size_t count){inner.update(data,count);}
 std::array<unsigned char,32> finish(){auto digest=hashBytes(inner);Wipe wipe{digest};Sha256 hash;hash.update(outer.data(),outer.size());hash.update(digest.data(),digest.size());return hashBytes(hash);}
 ~Hmac(){erase(outer.data(),outer.size());erase(&inner,sizeof inner);}
};
bool sameBytes(const unsigned char* a,const unsigned char* b,size_t length){unsigned char mismatch=0;for(size_t i=0;i<length;++i)mismatch|=a[i]^b[i];return mismatch==0;}
class MediaAuthenticator {
 Hmac hmac;Sha256 chunk;size_t used=0;bool video;
 void flush(){auto digest=hashBytes(chunk);hmac.update(digest.data(),digest.size());erase(digest.data(),digest.size());chunk=Sha256();used=0;}
 public:
 MediaAuthenticator(const unsigned char* key,bool isVideo):hmac(key,32),video(isVideo){}
 void update(const void* data,size_t count){if(!video){hmac.update(data,count);return;}auto bytes=static_cast<const unsigned char*>(data);while(count){auto take=std::min<size_t>(count,131072-used);chunk.update(bytes,take);used+=take;bytes+=take;count-=take;if(used==131072)flush();}}
 std::array<unsigned char,32> finish(){if(video&&used)flush();return hmac.finish();}
};
std::string text(const J& value,const char* name,const std::string& fallback=""){
 if(!value.contains(name)||value[name].is_null())return fallback;
 if(!value[name].is_string())throw CapabilityError("LINE 附件欄位格式不相容。");
 return value[name].get<std::string>();
}
std::string lower(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
std::string identity(const std::string& value){if(value.empty()||value.size()>4096||std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;}))throw CapabilityError("LINE 附件缺少有效的訊息識別。");return value;}
std::string bindingKey(const std::string& chat,const std::string& message,bool thumbnail){return key({identity(chat),identity(message),thumbnail?"thumbnail":"full"});}
std::string escapePath(const std::string& input){static constexpr char digits[]="0123456789ABCDEF";std::string output;for(unsigned char c:input){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')output+=static_cast<char>(c);else{output+='%';output+=digits[c>>4];output+=digits[c&15];}}return output;}
J metadata(const J& row){
 if(!row.contains("_contentMetadata")||row["_contentMetadata"].is_null())return J::object();
 if(row["_contentMetadata"].is_object())return row["_contentMetadata"];
 if(!row["_contentMetadata"].is_string()||row["_contentMetadata"].get_ref<const std::string&>().size()>4*1024*1024)throw CapabilityError("LINE 附件資訊格式不相容。");
 try{auto value=J::parse(row["_contentMetadata"].get_ref<const std::string&>(),[](int depth,J::parse_event_t,J&){if(depth>32)throw CapabilityError("LINE 附件資訊巢狀層數過多。");return true;});return value.is_object()?value:J::object();}catch(const J::exception&){return J::object();}
}
bool encryptedMetadata(const J& meta){
 for(auto name:{"ENC_KM","e2eeNext","keyMaterial"})if(meta.contains(name))return true;
 return false;
}
struct MetadataWipe{J& value;~MetadataWipe(){for(auto name:{"ENC_KM","keyMaterial","DOWNLOAD_URL","PREVIEW_URL"})if(value.contains(name)&&value[name].is_string())erase(value[name].get_ref<std::string&>());}};
struct Material {std::string encoded,name;bool encrypted=false;J size=nullptr;~Material(){erase(encoded);}};
Material mediaMaterial(const J& row,J& meta){
 Material out;out.encrypted=encryptedMetadata(meta);std::string bodyKey;
 auto raw=text(row,"_text");if(raw.size()>4*1024*1024)throw CapabilityError("LINE 媒體內容過大。");
 auto first=raw.find_first_not_of(" \t\r\n");
 if(first!=std::string::npos&&raw[first]=='{'){
  try{auto payload=J::parse(raw,[](int depth,J::parse_event_t,J&){if(depth>32)throw CapabilityError("LINE 媒體內容巢狀層數過多。");return true;});
   if(payload.is_object()&&payload.contains("keyMaterial")){out.encrypted=true;if(payload["keyMaterial"].is_string()){bodyKey=payload["keyMaterial"];erase(payload["keyMaterial"].get_ref<std::string&>());}if(payload.contains("fileName")&&payload["fileName"].is_string())out.name=payload["fileName"];if(payload.contains("fileSize")&&payload["fileSize"].is_number_integer()&&payload["fileSize"].get<int64_t>()>=0)out.size=payload["fileSize"];}
  }catch(const J::exception&){if(raw.find("keyMaterial")!=std::string::npos)out.encrypted=true;}
 }
 erase(raw);
 // The official Chrome path and beeper/line handler both pass actual ENC_KM
 // directly to base64 media-key derivation. It is never the database key.
 if(!bodyKey.empty())out.encoded=std::move(bodyKey);
 else for(auto name:{"keyMaterial","ENC_KM"})if(meta.contains(name)&&meta[name].is_string()&&!meta[name].get_ref<const std::string&>().empty()){out.encoded=meta[name];break;}
 if(!out.encoded.empty())try{auto derived=deriveLineMediaKeys(out.encoded);erase(derived.data(),derived.size());out.encrypted=true;}catch(const DownloadUnavailable&){erase(out.encoded);out.encrypted=true;}
 return out;
}

J expectedSize(const J& meta){
 // Encrypted OBS FILE_SIZE may include an HMAC/ciphertext envelope. Never
 // apply it as the size of the decrypted original selected by the user.
 if(encryptedMetadata(meta)||!meta.contains("FILE_SIZE")||meta["FILE_SIZE"].is_null())return nullptr;
 const auto& value=meta["FILE_SIZE"];
 if(value.is_number_integer()&&value.get<int64_t>()>=0)return value;
 if(value.is_string()){
  const auto& raw=value.get_ref<const std::string&>();int64_t size=0;
  auto result=std::from_chars(raw.data(),raw.data()+raw.size(),size);
  if(!raw.empty()&&result.ec==std::errc()&&result.ptr==raw.data()+raw.size()&&size>=0)return size;
 }
 return nullptr;
}
struct Inspection {Digest digest;std::string extension;};
Inspection inspect(const fs::path& file,const std::string& kind,Control& control){
 control.check();auto ext=lower(utf8(file.extension()));
 if(ext==".eimg"||ext==".e2ee")throw DownloadUnavailable("這是 LINE 加密快取，不能當作已取得的附件原檔。");
 VerifiedInput input(file);Sha256 hash;uint64_t count=0;std::array<unsigned char,64> head{},tail{};size_t headSize=0,tailSize=0;
 std::array<char,65536> buffer{};
 while(input){control.check();input.read(buffer.data(),buffer.size());auto n=static_cast<size_t>(input.gcount());
  if(count>static_cast<uint64_t>(INT64_MAX)-n)throw CapabilityError("附件超過可保存的檔案大小。");
  if(headSize<head.size()){auto take=std::min(n,head.size()-headSize);std::memcpy(head.data()+headSize,buffer.data(),take);headSize+=take;}
  if(n>=tail.size()){std::memcpy(tail.data(),buffer.data()+n-tail.size(),tail.size());tailSize=tail.size();}
  else if(n){auto retain=std::min(tailSize,tail.size()-n);std::memmove(tail.data(),tail.data()+tailSize-retain,retain);std::memcpy(tail.data()+retain,buffer.data(),n);tailSize=retain+n;}
  hash.update(buffer.data(),n);count+=n;
 }
 if(input.bad()||count!=input.size())throw IntegrityError("附件在讀取期間改變，未保存副本。");
 if(!count)throw DownloadUnavailable("附件檔案沒有內容。");
 auto starts=[&](const char* bytes,size_t length){return headSize>=length&&std::memcmp(head.data(),bytes,length)==0;};
 std::string detected=".bin";bool image=false,video=false;
 if(starts("\x89PNG\r\n\x1a\n",8)&&headSize>=24&&std::memcmp(head.data()+12,"IHDR",4)==0){
  const bool width=head[16]||head[17]||head[18]||head[19],height=head[20]||head[21]||head[22]||head[23];
  if(width&&height&&tailSize>=12&&std::memcmp(tail.data()+tailSize-8,"IEND",4)==0){detected=".png";image=true;}
 }else if(starts("\xff\xd8\xff",3)&&count>=12&&tailSize>=2&&tail[tailSize-2]==255&&tail[tailSize-1]==217){detected=".jpg";image=true;}
 else if((starts("GIF87a",6)||starts("GIF89a",6))&&headSize>=10&&tailSize&&tail[tailSize-1]==0x3b){detected=".gif";image=true;}
 else if(starts("RIFF",4)&&headSize>=16&&std::memcmp(head.data()+8,"WEBP",4)==0){detected=".webp";image=true;}
 else if(starts("RIFF",4)&&headSize>=12&&std::memcmp(head.data()+8,"WAVE",4)==0)detected=".wav";
 else if(starts("OggS",4))detected=".ogg";
 else if(starts("ID3",3))detected=".mp3";
 else if(headSize>=12&&std::memcmp(head.data()+4,"ftyp",4)==0){
  if(std::memcmp(head.data()+8,"heic",4)==0||std::memcmp(head.data()+8,"heix",4)==0||std::memcmp(head.data()+8,"heif",4)==0||std::memcmp(head.data()+8,"mif1",4)==0||std::memcmp(head.data()+8,"avif",4)==0)image=true;
  else{detected=".mp4";video=true;}
 }
 if((starts("RIFF",4)&&headSize>=12&&std::memcmp(head.data()+8,"AVI ",4)==0)||starts("\x1a\x45\xdf\xa3",4))video=true;
 if(kind=="video"&&!video)throw DownloadUnavailable("影片不是可辨識的 MP4、WebM 或 AVI 格式，未當作影片原檔保存。");
 if((kind=="image"||kind=="sticker")&&!image)throw DownloadUnavailable("圖片不是可辨識的完整圖片格式；加密快取或錯誤頁面不能當作原圖。");
 return {{hash.finish(),count,0},detected};
}
void expected(const J& a,const Digest& digest){
 if(!a.at("expected_size").is_null()&&a.at("expected_size").get<uint64_t>()!=digest.size)throw IntegrityError("選取檔案的大小與 LINE 附件資訊不符。");
 if(!a.at("expected_sha256").is_null()&&a.at("expected_sha256").get<std::string>()!=digest.hash)throw IntegrityError("附件內容與已核對的來源不符。");
}
std::string downloadKey(const std::string& chat,const J& descriptor){return key({chat,text(descriptor,"item_source_id"),text(descriptor,"source_id"),text(descriptor,"variant"),text(descriptor,"version")});}
#ifdef _WIN32
std::wstring wideUrl(const std::string& value){return std::wstring(value.begin(),value.end());}
struct Internet {HINTERNET handle=nullptr;explicit Internet(HINTERNET value):handle(value){if(!handle)throw DownloadUnavailable("LINE 附件連線無法啟動。");}~Internet(){WinHttpCloseHandle(handle);}operator HINTERNET()const{return handle;}};
fs::path downloadUrl(const std::string& url,const J& descriptor,const fs::path& stage,Control& control,bool ciphertext,const std::shared_ptr<LineSession>& session,bool privateResource,const std::string& messageId,std::string& cachedToken){
 control.check();const auto normalized=normalizeLineMediaUrl(url);if(normalized.empty())throw DownloadUnavailable("LINE 未提供可用的附件下載來源。");
 const auto slash=normalized.find('/',8);const auto host=normalized.substr(8,slash-8),target=normalized.substr(slash);
 Internet httpSession(WinHttpOpen(L"ChatAtlas native media/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
 if(!WinHttpSetTimeouts(httpSession,5000,5000,5000,1000))throw DownloadUnavailable("附件連線等候設定失敗。");
 Internet connection(WinHttpConnect(httpSession,wideUrl(host).c_str(),INTERNET_DEFAULT_HTTPS_PORT,0));
 Internet request(WinHttpOpenRequest(connection,L"GET",wideUrl(target).c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
 DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
 if(!WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof policy))throw DownloadUnavailable("附件連線限制設定失敗。");
 DWORD disable=WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
 if(!WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disable,sizeof disable))throw DownloadUnavailable("附件連線限制設定失敗。");
 std::wstring headers;Wipe wipeHeaders{headers};
 if(privateResource){
  if(host!="obs.line-apps.com"||!session||!session->current())throw DownloadUnavailable("無法核對目前 LINE 的附件連線，請重新連接 LINE。");
  if(cachedToken.empty())cachedToken=acquireLineObsToken(session,control);const auto& application=session->application();
  if(cachedToken.empty()||cachedToken.size()>8192||application.empty()||application.size()>1024||std::any_of(cachedToken.begin(),cachedToken.end(),[](unsigned char c){return c<=32||c>=127;})||std::any_of(application.begin(),application.end(),[](unsigned char c){return (c<32&&c!='\t')||c>=127;}))throw DownloadUnavailable("LINE 附件連線資訊不相容，未傳送下載要求。");
  headers=L"X-Line-Access: "+wideUrl(cachedToken)+L"\r\nX-Line-Application: "+wideUrl(application)+L"\r\nX-Talk-Meta: "+wideUrl(lineMediaTalkMeta(messageId))+L"\r\n";
 }
 control.check();if(!WinHttpSendRequest(request,headers.empty()?WINHTTP_NO_ADDITIONAL_HEADERS:headers.c_str(),static_cast<DWORD>(headers.size()),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr)){control.check();throw DownloadUnavailable("LINE 附件目前無法下載；可能已過期或需要 LINE 登入授權。");}
 control.check();DWORD status=0,statusSize=sizeof status;
 if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&statusSize,WINHTTP_NO_HEADER_INDEX)||status!=200){if(privateResource&&(status==401||status==403))erase(cachedToken);throw DownloadUnavailable("LINE 附件目前無法下載；可能已過期、無權限或尚未完成編碼。");}
 std::array<wchar_t,80> length{};DWORD lengthSize=static_cast<DWORD>(sizeof length);
 if(WinHttpQueryHeaders(request,WINHTTP_QUERY_CONTENT_LENGTH,WINHTTP_HEADER_NAME_BY_INDEX,length.data(),&lengthSize,WINHTTP_NO_HEADER_INDEX)){
  std::wstring raw(length.data());if(raw.empty()||!std::all_of(raw.begin(),raw.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';}))throw DownloadUnavailable("附件大小資訊不相容。");
  uint64_t total=0;for(auto c:raw){if(total>httpLimit/10)throw DownloadUnavailable("網路附件超過 1 GiB，請由 LINE 另存原檔後補入。");total=total*10+(c-L'0');}
  if(total>httpLimit)throw DownloadUnavailable("網路附件超過 1 GiB，請由 LINE 另存原檔後補入。");
  if(!ciphertext&&!descriptor["expected_size"].is_null()&&total!=descriptor["expected_size"].get<uint64_t>())throw IntegrityError("附件下載大小與 LINE 資訊不符。");
 }
 auto output=within(stage,"line-download.bin");bool owned=false;
 try{{ExclusiveFile file(output);owned=true;std::array<char,65536> bytes{};uint64_t total=0;auto started=std::chrono::steady_clock::now();
   for(;;){control.check();if(std::chrono::steady_clock::now()-started>std::chrono::minutes(15))throw DownloadUnavailable("附件下載超過等候時間，可稍後重試。");DWORD count=0;
    if(!WinHttpReadData(request,bytes.data(),static_cast<DWORD>(bytes.size()),&count)){control.check();throw DownloadUnavailable("附件下載未完成，可稍後重試。");}
    if(!count)break;if(count>httpLimit-total)throw DownloadUnavailable("網路附件超過 1 GiB，請由 LINE 另存原檔後補入。");total+=count;
    if(!ciphertext&&!descriptor["expected_size"].is_null()&&total>descriptor["expected_size"].get<uint64_t>())throw IntegrityError("附件下載大小與 LINE 資訊不符。");file.write(bytes.data(),count);
   }file.sync();}
  if(!ciphertext){auto info=inspect(output,text(descriptor,"kind"),control);expected(descriptor,info.digest);}return output;
 }catch(...){if(owned){std::error_code ignored;fs::remove(output,ignored);}throw;}
}
#endif
}

std::string normalizeLineMediaUrl(const std::string& input){
 if(input.empty()||input.size()>8192||std::any_of(input.begin(),input.end(),[](unsigned char c){return c<=32||c>=127||c=='\\'||c=='#'||c=='"'||c=='<'||c=='>'; }))return {};
 std::string value=input;
 if(value.front()=='/'){if(value.size()<2||value[1]=='/')return {};value="https://obs.line-apps.com"+value;}
 if(value.rfind("https://",0)!=0)return {};
 auto end=value.find_first_of("/?",8);if(end==std::string::npos)end=value.size();auto host=lower(value.substr(8,end-8));
 if(host.ends_with(":443"))host.resize(host.size()-4);
 if(host!="obs.line-apps.com"&&host!="obs.line-scdn.net")return {};
 auto target=value.substr(end);if(target.empty()||target.front()=='?')target="/"+target;
 auto normalized=lower(target);if(normalized.find("%0d")!=std::string::npos||normalized.find("%0a")!=std::string::npos)return {};
 return "https://"+host+target;
}

std::array<unsigned char,76> deriveLineMediaKeys(const std::string& encoded){
 // LINE's FileEncryption material is a 32-byte random key, encoded in standard
 // base64. This is independent of the database passphrase and .eimg cache keys.
 if(encoded.size()!=44||encoded.back()!='=')throw DownloadUnavailable("LINE 訊息未提供可辨識的媒體解密資料。");
 auto digit=[](unsigned char c)->int{if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;};
 std::array<unsigned char,32> material{};Wipe wipeMaterial{material};size_t output=0;uint32_t value=0;int bits=0;
 for(size_t i=0;i<43;++i){int next=digit(static_cast<unsigned char>(encoded[i]));if(next<0)throw DownloadUnavailable("LINE 媒體解密資料格式不相容。");value=(value<<6)|static_cast<uint32_t>(next);bits+=6;if(bits>=8){bits-=8;if(output>=material.size())throw DownloadUnavailable("LINE 媒體解密資料格式不相容。");material[output++]=static_cast<unsigned char>(value>>bits);}}
 if(output!=32||bits!=2||(value&3))throw DownloadUnavailable("LINE 媒體解密資料格式不相容。");
 std::array<unsigned char,32> salt{};Hmac extract(salt.data(),salt.size());extract.update(material.data(),material.size());auto prk=extract.finish();Wipe wipePrk{prk};
 std::array<unsigned char,76> result{};std::array<unsigned char,32> previous{};Wipe wipePrevious{previous};size_t written=0;
 for(unsigned char round=1;written<result.size();++round){Hmac expand(prk.data(),prk.size());if(round>1)expand.update(previous.data(),previous.size());constexpr char info[]="FileEncryption";expand.update(info,sizeof info-1);expand.update(&round,1);previous=expand.finish();auto take=std::min(previous.size(),result.size()-written);std::copy_n(previous.begin(),take,result.begin()+written);written+=take;}
 return result;
}
void decryptLineMediaFile(const fs::path& ciphertext,const fs::path& destination,const std::string& encoded,Control& control,bool video){
 control.check();auto keys=deriveLineMediaKeys(encoded);Wipe wipeKeys{keys};
#ifdef _WIN32
 VerifiedInput input(ciphertext);if(input.size()<33||input.size()>httpLimit)throw DownloadUnavailable("LINE 媒體密文大小不相容。");
 const auto payloadSize=input.size()-32;MediaAuthenticator authenticate(keys.data()+32,video);MediaAuthenticator directAuthenticate(keys.data()+32,false);std::array<char,65536> bytes{};Wipe wipeBytes{bytes};uint64_t remaining=payloadSize;
 while(remaining){control.check();auto take=static_cast<size_t>(std::min<uint64_t>(remaining,bytes.size()));input.read(bytes.data(),take);if(static_cast<size_t>(input.gcount())!=take)throw IntegrityError("LINE 媒體密文未完整保存。");authenticate.update(bytes.data(),take);if(video)directAuthenticate.update(bytes.data(),take);remaining-=take;}
 std::array<unsigned char,32> tag{};Wipe wipeTag{tag};input.read(reinterpret_cast<char*>(tag.data()),tag.size());if(static_cast<size_t>(input.gcount())!=tag.size()||input.bad())throw IntegrityError("LINE 媒體驗證資料不完整。");auto actual=authenticate.finish();Wipe wipeActual{actual};auto directActual=directAuthenticate.finish();Wipe wipeDirectActual{directActual};
 if(!(sameBytes(actual.data(),tag.data(),tag.size())|(video&&sameBytes(directActual.data(),tag.data(),tag.size()))))throw IntegrityError("LINE 媒體完整性驗證失敗，未解密或保存原檔。");
 // Authentication is complete before any plaintext destination is created.
 struct Algorithm{BCRYPT_ALG_HANDLE handle=nullptr;~Algorithm(){if(handle)BCryptCloseAlgorithmProvider(handle,0);}}algorithm;
 if(BCryptOpenAlgorithmProvider(&algorithm.handle,BCRYPT_AES_ALGORITHM,nullptr,0)<0||BCryptSetProperty(algorithm.handle,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),sizeof BCRYPT_CHAIN_MODE_ECB,0)<0)throw DownloadUnavailable("Windows 原生媒體解密元件無法使用。");
 DWORD objectSize=0,resultSize=0;if(BCryptGetProperty(algorithm.handle,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof objectSize,&resultSize,0)<0||objectSize==0||objectSize>1024*1024)throw DownloadUnavailable("Windows 原生媒體解密元件不相容。");
 std::vector<unsigned char> keyObject(objectSize);Wipe wipeObject{keyObject};struct AesKey{BCRYPT_KEY_HANDLE handle=nullptr;~AesKey(){if(handle)BCryptDestroyKey(handle);}}aes;
 if(BCryptGenerateSymmetricKey(algorithm.handle,&aes.handle,keyObject.data(),objectSize,keys.data(),32,0)<0)throw DownloadUnavailable("Windows 原生媒體解密元件無法使用。");
 VerifiedInput second(ciphertext);if(second.size()!=input.size())throw IntegrityError("LINE 媒體在驗證後改變，未保存原檔。");
 std::array<unsigned char,65536> counters{},stream{};Wipe wipeCounters{counters};Wipe wipeStream{stream};std::array<unsigned char,16> counter{};Wipe wipeCounter{counter};std::copy_n(keys.begin()+64,12,counter.begin());
 bool owned=false;try{{ExclusiveFile plain(destination);owned=true;MediaAuthenticator secondAuth(keys.data()+32,video);MediaAuthenticator secondDirect(keys.data()+32,false);remaining=payloadSize;
  while(remaining){control.check();auto take=static_cast<size_t>(std::min<uint64_t>(remaining,bytes.size()));second.read(bytes.data(),take);if(static_cast<size_t>(second.gcount())!=take)throw IntegrityError("LINE 媒體在解密期間改變。");secondAuth.update(bytes.data(),take);if(video)secondDirect.update(bytes.data(),take);auto blocks=(take+15)/16;
   for(size_t block=0;block<blocks;++block){std::copy(counter.begin(),counter.end(),counters.begin()+block*16);for(int i=15;i>=0;--i)if(++counter[static_cast<size_t>(i)]!=0)break;}
   ULONG encrypted=0;auto padded=static_cast<ULONG>(blocks*16);if(BCryptEncrypt(aes.handle,counters.data(),padded,nullptr,nullptr,0,stream.data(),padded,&encrypted,0)<0||encrypted!=padded)throw DownloadUnavailable("Windows 原生媒體解密未完成。");
   for(size_t i=0;i<take;++i)bytes[i]=static_cast<char>(static_cast<unsigned char>(bytes[i])^stream[i]);plain.write(bytes.data(),take);erase(bytes.data(),take);remaining-=take;
  }
  std::array<unsigned char,32> secondTag{};Wipe wipeSecondTag{secondTag};second.read(reinterpret_cast<char*>(secondTag.data()),secondTag.size());auto verified=secondAuth.finish();Wipe wipeVerified{verified};auto directVerified=secondDirect.finish();Wipe wipeDirectVerified{directVerified};if(second.gcount()!=32||second.bad()||!(sameBytes(verified.data(),tag.data(),32)|(video&&sameBytes(directVerified.data(),tag.data(),32)))||!sameBytes(secondTag.data(),tag.data(),32))throw IntegrityError("LINE 媒體在解密期間改變，未保存原檔。");plain.sync();}
 }catch(...){if(owned){std::error_code ignored;fs::remove(destination,ignored);}throw;}
#else
 (void)ciphertext;(void)destination;(void)video;throw DownloadUnavailable("E2EE 媒體解密僅適用於 Windows 原生程式。");
#endif
}

LineMediaResolver::LineMediaResolver(const fs::path& selectedDatabase,const fs::path& mediaRoot):database(absoluteNormalizedPath(selectedDatabase)){
 noLinks(database);if(!mediaRoot.empty()){root=absoluteNormalizedPath(mediaRoot);noLinks(root);if(!fs::is_directory(root))throw CapabilityError("指定的附件來源不是一般本機資料夾。");}
}
std::string lineMediaObjectUrl(const std::string& service,const std::string& messageId,bool preview){
 if(service!="talk"&&service!="g2")throw CapabilityError("LINE 附件服務類型不相容。");identity(messageId);return "https://obs.line-apps.com/r/"+service+"/m/"+escapePath(messageId)+(preview?"/preview":"");
}
std::string lineMediaTalkMeta(const std::string& messageId){
 identity(messageId);std::string thrift;auto byte=[&](unsigned value){thrift.push_back(static_cast<char>(value));};byte(11);byte(0);byte(4);auto size=static_cast<uint32_t>(messageId.size());for(int shift:{24,16,8,0})byte((size>>shift)&255);thrift+=messageId;for(unsigned value:{15,0,27,12,0,0,0,0,0})byte(value);return base64(J{{"message",base64(thrift)}}.dump());
}
void LineMediaResolver::setSession(std::shared_ptr<LineSession> value){erase(cachedObsToken);session=std::move(value);tokenTime={};}
void LineMediaResolver::setChatService(const std::string& chatId,const std::string& service){identity(chatId);if(service!="talk"&&service!="g2")throw CapabilityError("LINE 附件服務類型不相容。");chatServices[chatId]=service;}
LineMediaResolver::~LineMediaResolver(){erase(cachedObsToken);for(auto& [_,binding]:downloads){erase(binding.keyMaterial);erase(binding.url);}}
void LineMediaResolver::bindFile(const std::string& chatId,const std::string& messageId,const fs::path& original,bool thumbnail,Control* requested){
 Control fallback;auto& control=requested?*requested:fallback;control.check();noLinks(original);VerifiedInput input(original);control.check();
 selected[bindingKey(chatId,messageId,thumbnail)]=absoluteNormalizedPath(original);
}
void LineMediaResolver::bindRelativeFile(const std::string& chatId,const std::string& messageId,const std::string& relative,bool thumbnail,Control* requested){
 if(root.empty())throw CapabilityError("請先明確選取本機附件來源資料夾。");bindFile(chatId,messageId,within(root,relative),thumbnail,requested);
}
J LineMediaResolver::attachments(const std::string& chatId,const J& row,Control* requested){
 Control fallback;auto& control=requested?*requested:fallback;control.check();identity(chatId);
 if(!row.is_object()||!row.contains("_contentType")||!row["_contentType"].is_number_integer())throw CapabilityError("LINE 附件類型格式不相容。");
 const auto type=row["_contentType"].get<int64_t>();std::string kind;
 if(type==1)kind="image";else if(type==2)kind="video";else if(type==3)kind="audio";else if(type==7)kind="sticker";else if(type==14)kind="file";else return J::array();
 const auto message=identity(text(row,"_id"));auto meta=metadata(row);MetadataWipe wipeMeta{meta};
 if(row.contains("_contentInfo")&&!row["_contentInfo"].is_null()){
  auto info=metadata(J{{"_contentMetadata",row["_contentInfo"]}});MetadataWipe wipeInfo{info};
  if(!meta.contains("PREVIEW_URL")){auto preview=text(info,"PREVIEW_URL",text(info,"DOWNLOAD_URL"));if(!preview.empty())meta["PREVIEW_URL"]=std::move(preview);}
  if(!text(meta,"OID").empty()&&!text(meta,"SID").empty())for(auto field:{"keyMaterial","ENC_KM"})if(!meta.contains(field)&&info.contains(field)&&info[field].is_string())meta[field]=info[field];
 }
 auto material=mediaMaterial(row,meta);auto name=text(meta,"FILE_NAME",text(meta,"fileName"));
 if(name.empty())name=material.name;
 if(name.empty()&&type==14){auto candidate=text(row,"_text");auto first=candidate.find_first_not_of(" \t\r\n");if(candidate.size()<=4096&&first!=std::string::npos&&candidate[first]!='{'&&candidate[first]!='['&&candidate.find("keyMaterial")==std::string::npos&&candidate.find("ENC_KM")==std::string::npos&&candidate.find("e2eeNext")==std::string::npos)name=candidate;erase(candidate);}
 if(!material.encoded.empty()&&name.find(material.encoded)!=std::string::npos)name.clear();
 if(name.empty())name=kind+"-"+message+(kind=="image"||kind=="sticker"?".jpg":kind=="video"?".mp4":kind=="audio"?".ogg":".bin");name=safeName(name);
 auto fullUrl=(!material.encrypted||!material.encoded.empty())?normalizeLineMediaUrl(text(meta,"DOWNLOAD_URL")):std::string(),previewUrl=(!material.encrypted||!material.encoded.empty())?normalizeLineMediaUrl(text(meta,"PREVIEW_URL")):std::string();Wipe wipeFullUrl{fullUrl};Wipe wipePreviewUrl{previewUrl};bool privateFull=false,privatePreview=false;
 const auto oid=text(meta,"OID"),sid=text(meta,"SID");
 if(!material.encoded.empty()&&!oid.empty()&&std::set<std::string>{"emi","emv","ema","emf"}.count(sid)){
  const auto base="https://obs.line-apps.com/r/talk/"+sid+"/"+escapePath(identity(oid));
  if(fullUrl.empty()){fullUrl=base;privateFull=true;}if(previewUrl.empty()){previewUrl=base+"__ud-preview";privatePreview=true;}
 }else if(!material.encrypted&&oid.empty()){
  auto service=chatServices.find(chatId);if(service!=chatServices.end()){if(fullUrl.empty()){fullUrl=lineMediaObjectUrl(service->second,message);privateFull=true;}if(previewUrl.empty()&&(kind=="image"||kind=="video")){previewUrl=lineMediaObjectUrl(service->second,message,true);privatePreview=true;}}
 }

 auto pop=text(meta,"OBS_POP");Wipe wipePop{pop};if(!pop.empty()){if(pop.size()>4096)throw CapabilityError("LINE 附件路由資訊過大。");if(privateFull)fullUrl+="?p="+escapePath(pop);if(privatePreview)previewUrl+="?p="+escapePath(pop);}
 const bool previewSelected=selected.count(bindingKey(chatId,message,true));J result=J::array();
 for(bool thumbnail:{false,true}){
  if(thumbnail&&!previewSelected&&previewUrl.empty())continue;control.check();auto local=selected.find(bindingKey(chatId,message,thumbnail));
  J descriptor={{"source_id",message},{"item_source_id",message},{"name",name},{"kind",thumbnail?"image":kind},{"variant",thumbnail?"thumbnail":"full"},{"version","line-native-v1"},{"expected_size",thumbnail?J(nullptr):material.encrypted?material.size:expectedSize(meta)},{"expected_sha256",nullptr}};
  if(local!=selected.end())try{auto info=inspect(local->second,text(descriptor,"kind"),control);expected(descriptor,info.digest);descriptor["expected_size"]=info.digest.size;descriptor["expected_sha256"]=info.digest.hash;}catch(const Cancelled&){throw;}catch(const Error&){}
  validateAttachment(descriptor);auto index=downloadKey(chatId,descriptor);auto prior=downloads.find(index);if(prior!=downloads.end()){erase(prior->second.keyMaterial);erase(prior->second.url);}
  downloads[index]={local!=selected.end()?local->second:fs::path(),thumbnail?previewUrl:fullUrl,descriptor,material.encoded,thumbnail?privatePreview:privateFull,message};result.push_back(std::move(descriptor));
 }
 return result;
}
fs::path LineMediaResolver::copy(const std::string& chatId,const J& descriptor,const fs::path& stage,Control& control){
 control.check();auto found=downloads.find(downloadKey(chatId,descriptor));if(found==downloads.end()||found->second.descriptor!=descriptor)throw CapabilityError("附件與讀取的 LINE 訊息不符，請重新讀取來源。");
 noLinks(stage);if(!fs::is_directory(stage))throw CapabilityError("附件暫存位置不存在。");const auto& binding=found->second;
 if(!binding.file.empty()){
  auto info=inspect(binding.file,text(descriptor,"kind"),control);expected(descriptor,info.digest);auto output=within(stage,"line-original.bin");copyVerified(binding.file,output,info.digest.size,info.digest.hash,[&]{control.check();});return output;
 }
 if(binding.url.empty())throw DownloadUnavailable("本機資料庫未提供可用的附件原檔；可由 LINE 另存後補入。");
#ifdef _WIN32
 if(binding.privateResource){if(!session||!session->current()){erase(cachedObsToken);throw DownloadUnavailable("無法核對目前 LINE 的附件連線，請重新連接 LINE。");}if(cachedObsToken.empty()||std::chrono::steady_clock::now()-tokenTime>std::chrono::minutes(5)){erase(cachedObsToken);tokenTime=std::chrono::steady_clock::now();}}
 auto payload=downloadUrl(binding.url,descriptor,stage,control,!binding.keyMaterial.empty(),session,binding.privateResource,binding.messageId,cachedObsToken);
 if(!binding.keyMaterial.empty()){auto plain=within(stage,"line-decrypted.bin");decryptLineMediaFile(payload,plain,binding.keyMaterial,control,text(descriptor,"kind")=="video");auto info=inspect(plain,text(descriptor,"kind"),control);expected(descriptor,info.digest);return plain;}return payload;
#else
 throw DownloadUnavailable("網路附件下載僅適用於 Windows 原生程式。");
#endif
}
J pendingLineAttachments(Store& store,size_t limit){
 if(limit<1||limit>1000)throw Error("附件清單範圍需介於 1 至 1000。");
 return store.db.rows("SELECT a.id AS id,a.id AS attachment_id,c.id AS chat_id,c.source_id AS chat_source_id,c.title AS chat_title,i.id AS item_id,i.source_id AS item_source_id,i.source_id AS message_id,i.text AS text,i.text AS message_text,i.sent_at,a.name,a.kind,a.variant,a.status,a.reason,a.expected_size FROM attachments a JOIN chats c ON c.id=a.chat_id JOIN items i ON i.id=a.item_id WHERE c.account LIKE 'line-db:%' AND a.variant='full' AND a.status<>'saved' ORDER BY i.sent_at,a.id LIMIT ?",{static_cast<int64_t>(limit)});
}
void importLineAttachment(Store& store,const std::string& attachmentId,const fs::path& original,Control& control){
 control.check();auto rows=store.db.rows("SELECT a.*,c.account FROM attachments a JOIN chats c ON c.id=a.chat_id WHERE a.id=?",{attachmentId});
 if(rows.size()!=1||!text(rows[0],"account").starts_with("line-db:"))throw Error("這不是 LINE 本機資料庫的附件。");
 auto attachment=rows[0];if(attachment["variant"]!="full")throw Error("補入原檔請選擇原始附件列；縮圖不能冒充原檔。");
 J expectedDescriptor={{"expected_size",attachment["expected_size"]},{"expected_sha256",attachment["expected_hash"]}};
 auto info=inspect(original,text(attachment,"kind"),control);expected(expectedDescriptor,info.digest);
 if(attachment["status"]=="saved"&&store.reusable(attachmentId,[&]{control.check();}))return;
 auto relative="assets/"+info.digest.hash.substr(0,2)+"/"+info.digest.hash+info.extension;
 copyVerified(original,within(store.root,relative),static_cast<int64_t>(info.digest.size),info.digest.hash,[&]{control.check();});
 control.check();store.db.transaction([&]{store.saveBlob(attachmentId,relative,info.digest,false,[&]{control.check();});});
}
}
