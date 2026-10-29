#include "line_media.hpp"
#include "file.hpp"
#include "demo_image.hpp"
#include <iostream>
using namespace atlas;
namespace {
size_t checks=0;
void expect(bool value,const char* message){++checks;if(!value)throw Error(message);}
template<class F>void reject(F fn,const char* message){bool rejected=false;try{fn();}catch(const Error&){rejected=true;}expect(rejected,message);}
template<class F>void cancelled(F fn){bool rejected=false;try{fn();}catch(const Cancelled&){rejected=true;}expect(rejected,"Stopped operation must cancel");}
void write(const fs::path& file,const unsigned char* bytes,size_t size){ExclusiveFile out(file);out.write(bytes,size);out.sync();}
const unsigned char jpeg[]={255,216,255,224,0,16,74,70,73,70,0,1,1,0,0,1,0,1,0,0,255,219,0,67,0,6,4,5,6,5,4,6,6,5,6,7,7,6,8,10,16,10,10,9,9,10,20,14,15,12,16,23,20,24,24,23,20,22,22,26,29,37,31,26,27,35,28,22,22,32,44,32,35,38,39,41,42,41,25,31,45,48,45,40,48,37,40,41,40,255,192,0,11,8,0,1,0,1,1,1,17,0,255,196,0,20,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,255,196,0,20,16,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,218,0,8,1,1,0,0,63,0,42,63,255,217};
const char* mediaMaterial="AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=";
const char* derivedHex="54480d998f8e329e5a0a33fdc64f6aee8d9b0edb7a2792f8e52c33d59b1b72e0682c748ebd8593d84ae2974fe79b5448cc7dbf05c419f4b554d14d3f26e2553670679ffec9e08b4e8f569eb1";
const unsigned char videoPatternTag[]={14,188,159,135,236,25,166,131,249,102,240,122,56,218,43,244,91,105,184,30,81,18,52,11,230,99,171,114,34,190,195,3};
const char* videoPlainHash="c376ccea16039623815cbaef0d8b07a334ee47d0a76d9e998f5f0e711e413da1";
const unsigned char encryptedJpeg[]={170,184,249,51,171,249,28,67,95,185,36,219,183,218,104,11,4,175,247,115,20,67,49,2,157,192,59,54,244,186,19,3,61,122,40,131,82,200,34,206,143,164,43,36,14,85,26,85,155,121,8,176,209,120,126,120,231,216,9,17,185,210,59,240,7,88,88,122,175,93,224,193,164,1,180,136,0,2,95,170,184,232,168,25,220,75,187,179,229,146,5,152,214,18,252,185,45,146,196,188,65,231,137,128,162,36,177,13,221,193,86,223,233,69,106,163,243,120,11,226,185,19,135,95,233,18,124,252,201,172,201,33,250,250,89,47,228,51,89,141,122,191,26,225,125,88,116,200,128,171,153,96,202,32,172,0,112,179,87,229,19,249,23,41,40,212,152,21,233,201,154,208,95,132,185,178,1,22,254,95,230,116,247,81,42,60,143,139,10,34,112,244};
J row(const char* id,int type,J metadata=J::object()){
 return {{"_id",id},{"_chatId","synthetic-chat"},{"_contentType",type},{"_contentMetadata",metadata.dump()},{"_text",type==14?"人工文字檔.txt":""}};
}
struct MediaSource:Source{
 LineMediaResolver resolver;J raw;std::string chatId="synthetic-chat";
 explicit MediaSource(const fs::path& database,J messages):resolver(database),raw(std::move(messages)){account="line-db:synthetic-media-account";}
 J chats()override{return J::array({{{"source_id",chatId},{"title","人工圖片與附件"},{"kind","group"}}});}
 Batch read(const J&,const std::set<std::string>&,const std::string&,const std::string&,Control& control)override{
  Batch out;out.coverage="local_only";int ordinal=0;
  for(const auto& item:raw){control.check();const auto id=item["_id"].get<std::string>();out.items.push_back({{"source_id",id},{"kind","message"},{"text","人工訊息"},{"sent_at","2026-10-01T00:00:00Z"},{"ordinal",ordinal++}});for(auto& attachment:resolver.attachments(chatId,item,&control))out.attachments.push_back(attachment);}
  return out;
 }
 fs::path download(const J&,const J& attachment,const fs::path& stage,Control& control)override{return resolver.copy(chatId,attachment,stage,control);}
};
}
int main(int argc,char** argv){try{
 expect(argc==2,"Provide an empty owned fixture directory");auto base=absoluteNormalizedPath(path8(argv[1]));expect(fs::is_directory(base)&&fs::is_empty(base),"Fixture starts empty");Control control;
 expect(lineMediaObjectUrl("talk","12345")=="https://obs.line-apps.com/r/talk/m/12345","Plain media uses protocol message identity endpoint");expect(lineMediaObjectUrl("g2","12345",true)=="https://obs.line-apps.com/r/g2/m/12345/preview","Community preview uses known g2 namespace");expect(lineMediaObjectUrl("talk","id/with?syntax")=="https://obs.line-apps.com/r/talk/m/id%2Fwith%3Fsyntax","Message ID cannot alter endpoint path or query");expect(lineMediaTalkMeta("12345")=="eyJtZXNzYWdlIjoiQ3dBRUFBQUFCVEV5TXpRMUR3QWJEQUFBQUFBQSJ9","X-Talk-Meta matches independent binary Thrift field4/field27 vector");
 auto derived=deriveLineMediaKeys(mediaMaterial);const char* hex="0123456789abcdef";std::string encoded;for(auto byte:derived){encoded+=hex[byte>>4];encoded+=hex[byte&15];}expect(encoded==derivedHex,"HKDF-SHA256 FileEncryption vector matches independent Node crypto result");
 reject([&]{deriveLineMediaKeys("not-a-media-key");},"Malformed media key rejected");
#ifdef _WIN32
 auto ciphertextFile=base/"synthetic-e2ee.bin",plaintextFile=base/"synthetic-plain.jpg";write(ciphertextFile,encryptedJpeg,sizeof encryptedJpeg);decryptLineMediaFile(ciphertextFile,plaintextFile,mediaMaterial,control);expect(readText(plaintextFile)==std::string(reinterpret_cast<const char*>(jpeg),sizeof jpeg),"Native CNG AES256CTR decrypts independently authenticated JPEG vector");
 auto tampered=base/"synthetic-tampered.bin",rejectedPlain=base/"never-published.jpg";std::vector<unsigned char> corrupted(std::begin(encryptedJpeg),std::end(encryptedJpeg));corrupted.back()^=1;write(tampered,corrupted.data(),corrupted.size());reject([&]{decryptLineMediaFile(tampered,rejectedPlain,mediaMaterial,control);},"Wrong HMAC rejects media before decryption");expect(!fs::exists(rejectedPlain),"Authentication failure creates no plaintext file");
 auto videoCipher=base/"synthetic-video-cipher.bin",videoPlain=base/"synthetic-video-plain.bin";std::vector<unsigned char> videoPattern(131075);for(size_t i=0;i<videoPattern.size();++i)videoPattern[i]=static_cast<unsigned char>(i%251);videoPattern.insert(videoPattern.end(),std::begin(videoPatternTag),std::end(videoPatternTag));write(videoCipher,videoPattern.data(),videoPattern.size());decryptLineMediaFile(videoCipher,videoPlain,mediaMaterial,control,true);expect(digest(videoPlain).hash==videoPlainHash,"Video 128KiB chunk-HMAC and AESCTR agree with independent two-chunk vector");
 auto directVideo=base/"synthetic-direct-video.bin";decryptLineMediaFile(ciphertextFile,directVideo,mediaMaterial,control,true);expect(digest(directVideo).hash==digest(plaintextFile).hash,"Direct full-cipher video MAC variant is verified, not ignored");videoPattern[131072]^=1;auto changedVideo=base/"synthetic-video-tampered.bin";write(changedVideo,videoPattern.data(),videoPattern.size());auto neverVideo=base/"never-video.bin";reject([&]{decryptLineMediaFile(changedVideo,neverVideo,mediaMaterial,control,true);},"Video chunk-boundary tampering fails authentication");expect(!fs::exists(neverVideo),"Rejected video publishes no plaintext");
 auto changedKey=std::string(mediaMaterial);changedKey[0]='B';reject([&]{decryptLineMediaFile(ciphertextFile,rejectedPlain,changedKey,control);},"Wrong real-shaped key rejects authentication");expect(!fs::exists(rejectedPlain),"Wrong key creates no plaintext file");
#endif
 auto database=base/"synthetic.edb";atomicText(database,"Synthetic path marker; real encrypted DB tested separately");const auto databaseHash=digest(database).hash;
 auto originals=privateDir(base/"originals"),stage=privateDir(base/"stage");auto jpg=originals/"real-pixel.jpg",png=originals/"real-picture.png",file=originals/"人工文字檔.txt";
 write(jpg,jpeg,sizeof jpeg);write(png,demo_png,sizeof demo_png);atomicText(file,"人工附件內容\n保持原檔不變\n");auto jpegHash=digest(jpg).hash,pngHash=digest(png).hash,fileHash=digest(file).hash;
 for(const auto& value:std::vector<std::string>{"https://obs.line-apps.com/r/official/image?token=synthetic","https://OBS.LINE-SCDN.NET:443/test","/r/official/test"})expect(!normalizeLineMediaUrl(value).empty(),"Actual public OBS endpoint accepted");
 expect(normalizeLineMediaUrl("/r/official/test")=="https://obs.line-apps.com/r/official/test","Metadata relative path uses actual OBS host");
 for(const auto& value:std::vector<std::string>{"http://obs.line-apps.com/a","https://example.invalid/a","https://obs.line-apps.com.example.invalid/a","https://obs.line-apps.com@127.0.0.1/a","https://127.0.0.1/a","https://obs.line-apps.com:444/a","https://obs.line-apps.com./a","https://obs.line-apps.com/a#fragment","https://obs.line-apps.com/a%0d%0aHeader","https://obs.line-apps.com/a\\b","//example.invalid/a","file:///secret","https://obs.line-apps.com/\nsecret","https://obs.line-apps.com/ space"})expect(normalizeLineMediaUrl(value).empty(),"Untrusted URL rejected without a request");
 LineMediaResolver resolver(database,originals);
 for(unsigned status:{0u,202u,301u,401u,403u,404u,409u,410u,429u,500u,503u}){
  auto reason=lineMediaFailureReason(status);expect(!reason.empty()&&reason.size()<512&&reason.find("http")==std::string::npos&&reason.find("附件")==std::string::npos,"Download failure explanations are fixed, specific user text without URLs or generic attachment jargon");
 }
 expect(lineMediaFailureReason(200).empty(),"A successful response has no invented failure reason");
 expect(lineMediaFailureReason(401).find("登入")!=std::string::npos&&lineMediaFailureReason(403).find("權限")!=std::string::npos,"Login and permission failures explain what the user can check");
 expect(lineMediaFailureReason(404)==lineMediaFailureReason(410)&&lineMediaFailureReason(404).find("可能")!=std::string::npos,"Missing remote files are possible expiration, not a fabricated definite diagnosis");
 expect(lineMediaFailureReason(500).find("過期")==std::string::npos&&lineMediaFailureReason(429).find("頻繁")!=std::string::npos,"Temporary or throttled downloads are not mislabeled expired originals");
 {
  auto named=row("real-unicode-name",14,{{"FILE_NAME","  \"C:\\Users\\synthetic\\季度 報告.pdf\"  "}});
  auto descriptors=resolver.attachments("friendly-name-chat",named);
  expect(descriptors[0]["name"]=="季度 報告.pdf"&&!descriptors[0]["name_generated"].get<bool>(),"An original Unicode filename is retained without private source folders or quoting");
  auto alternate=row("alternate-name",14,{{"FILE_NAME","   "},{"fileName","旅程照片.zip"}});
  expect(resolver.attachments("friendly-name-chat",alternate)[0]["name"]=="旅程照片.zip","Whitespace primary filename does not hide a known lowercase metadata filename");
  auto traversal=row("path-name",14,{{"FILE_NAME","../../report.txt"}});
  expect(resolver.attachments("friendly-name-chat",traversal)[0]["name"]=="report.txt","Filename metadata never creates a path or displays traversal components");
  auto originalExtensionless=row("original-extensionless",14,{{"FILE_NAME","LICENSE"}});
  auto extensionless=resolver.attachments("friendly-name-chat",originalExtensionless);
  expect(extensionless[0]["name"]=="LICENSE"&&!extensionless[0]["name_generated"].get<bool>(),"A real original filename is not replaced merely because it has no suffix");
  const auto firstId="u0123456789abcdef0123456789abcdef",secondId="uabcdef0123456789abcdef0123456789";
  auto first=row(firstId,1);first["_createdTime"]=0;auto second=row(secondId,1);second["_createdTime"]=0;
  auto firstDescriptors=resolver.attachments("friendly-name-chat",first),secondDescriptors=resolver.attachments("friendly-name-chat",second);
  expect(firstDescriptors[0]["name"]=="1970-01-01_08-00-00-000_圖片"&&firstDescriptors[0]["name_generated"].get<bool>(),"Unnamed media uses actual message time and a readable kind, with no guessed file extension");
  expect(secondDescriptors[0]["name"]=="1970-01-01_08-00-00-000_圖片_2"&&firstDescriptors[0]["source_id"]!=secondDescriptors[0]["source_id"],"Two files from the same millisecond have short distinct labels and retain separate source identities");
  expect(resolver.attachments("friendly-name-chat",first)[0]["name"]==firstDescriptors[0]["name"],"Re-reading the same message keeps its assigned readable filename");
  expect(firstDescriptors[0]["name"].get<std::string>().find(firstId)==std::string::npos&&secondDescriptors[0]["name"].get<std::string>().find(secondId)==std::string::npos,"User-visible generated names never reveal long LINE message identifiers");
  for(auto [type,label]:std::vector<std::pair<int,std::string>>{{2,"影片"},{3,"語音"},{7,"貼圖"},{14,"檔案"}}){auto media=row(("unnamed-"+std::to_string(type)).c_str(),type);media["_createdTime"]=86399999;media["_text"]="";auto descriptor=resolver.attachments("friendly-name-chat",media)[0];expect(descriptor["name"]=="1970-01-02_07-59-59-999_"+label,"Unnamed kinds and UTC+8 day rollover use real timestamp labels");}
  auto missingTime=row("unknown-date-private-id",1);auto unknown=resolver.attachments("friendly-name-chat",missingTime)[0];
  expect(unknown["name"]=="圖片_未標示時間","Missing timestamps are disclosed instead of invented from current time or an identifier");
  auto invalidTime=row("invalid-date",1);invalidTime["_createdTime"]=uint64_t(UINT64_MAX);expect(resolver.attachments("friendly-name-chat",invalidTime)[0]["name"].get<std::string>().find("未標示時間")!=std::string::npos,"Out-of-range timestamp metadata remains a readable unknown date");
  auto blank=row("blank-file-name",14,{{"FILE_NAME","\"   \""}});blank["_text"]="";blank["_createdTime"]=0;expect(resolver.attachments("friendly-name-chat",blank)[0]["name"]=="1970-01-01_08-00-00-000_檔案","An empty quoted filename cannot turn into the engineering word attachment");
  auto signedName=row("signed-url-name",14,{{"FILE_NAME","https://obs.line-apps.com/private?token=synthetic"}});signedName["_text"]="";signedName["_createdTime"]=1;expect(resolver.attachments("friendly-name-chat",signedName).dump().find("token=")==std::string::npos,"An accidental signed URL is never published as a filename");
  auto keyName=row("key-name",14,{{"FILE_NAME",mediaMaterial},{"ENC_KM",mediaMaterial}});keyName["_text"]="";keyName["_createdTime"]=2;auto safe=resolver.attachments("friendly-name-chat",keyName);
  expect(safe.dump().find(mediaMaterial)==std::string::npos&&safe[0]["name"].get<std::string>().find("檔案")!=std::string::npos,"A real-shaped key cannot leak through a supplied filename");
  const std::string invalidPrivate="SYNTHETIC_FILENAME_CREDENTIAL";
  auto invalidKeyName=row("invalid-key-name",14,{{"FILE_NAME",invalidPrivate},{"keyMaterial",invalidPrivate}});invalidKeyName["_text"]="";auto invalidSafe=resolver.attachments("friendly-name-chat",invalidKeyName);
  expect(invalidSafe.dump().find(invalidPrivate)==std::string::npos&&invalidSafe[0]["name_generated"].get<bool>(),"Malformed transport values stay private even after key derivation rejects them");
  auto invalidBodyName=row("invalid-body-name",14);invalidBodyName["_text"]=J{{"fileName",invalidPrivate},{"keyMaterial",invalidPrivate}}.dump();auto bodySafe=resolver.attachments("friendly-name-chat",invalidBodyName);
  expect(bodySafe.dump().find(invalidPrivate)==std::string::npos&&bodySafe[0]["name_generated"].get<bool>(),"A rejected encrypted payload cannot expose its private key as the file label");
  auto popName=row("routing-value-name",14,{{"FILE_NAME",invalidPrivate},{"OBS_POP",invalidPrivate}});popName["_text"]="";expect(resolver.attachments("friendly-name-chat",popName).dump().find(invalidPrivate)==std::string::npos,"Private download routing data is not accepted as a filename");
 }
 auto imageRow=row("image-message",1,{{"FILE_NAME","original.jpg"},{"FILE_SIZE",std::to_string(sizeof jpeg)}});
 auto missing=resolver.attachments("synthetic-chat",imageRow,&control);expect(missing.size()==1&&missing[0]["variant"]=="full"&&missing[0]["expected_sha256"].is_null(),"Missing original is a real pending attachment, not fake saved bytes");
 reject([&]{resolver.copy("synthetic-chat",missing[0],stage,control);},"Missing file remains unavailable");
 expect(!fs::exists(stage/"line-original.bin"),"Unavailable path writes no fabricated payload");try{resolver.copy("synthetic-chat",missing[0],stage,control);expect(false,"Missing source must fail");}catch(const DownloadUnavailable& e){expect(!e.publicReason.empty()&&e.publicReason==e.what()&&e.publicReason.find("附件")==std::string::npos,"Safe fixed unavailable reason remains available to the per-file result report");}
 resolver.bindFile("synthetic-chat","image-message",jpg,false,&control);auto bound=resolver.attachments("synthetic-chat",imageRow,&control);
 expect(bound[0]["expected_sha256"]==jpegHash&&bound[0]["expected_size"]==sizeof jpeg,"Explicit file binding obtains actual digest and size");
 auto copied=resolver.copy("synthetic-chat",bound[0],stage,control);expect(digest(copied).hash==jpegHash,"Bound JPEG copied as verified original");
 reject([&]{resolver.copy("wrong-chat",bound[0],stage,control);},"Attachment cannot cross chat binding");auto forged=bound[0];forged["name"]="different";reject([&]{resolver.copy("synthetic-chat",forged,stage,control);},"Changed descriptor rejected");
 resolver.bindRelativeFile("synthetic-chat","png-message","real-picture.png",false,&control);auto pngRow=row("png-message",1);auto pngAttachments=resolver.attachments("synthetic-chat",pngRow,&control);expect(pngAttachments[0]["expected_sha256"]==pngHash,"Explicit relative reference stays in selected root");expect(pngAttachments[0]["name"].get<std::string>().ends_with(".png")&&pngAttachments[0]["name"].get<std::string>().find("png-message")==std::string::npos,"A verified local original supplies its actual extension without exposing the message ID");
 resolver.bindFile("synthetic-chat","extended-image",png,false,&control);auto extended=resolver.attachments("synthetic-chat",row("extended-image",21));expect(extended.size()==1&&extended[0]["kind"]=="image"&&extended[0]["expected_sha256"]==pngHash,"Known extended-image messages use the same verified image handling without inventing a new download route");
 reject([&]{resolver.bindRelativeFile("synthetic-chat","escape","../synthetic.edb");},"Relative traversal rejected");reject([&]{resolver.bindRelativeFile("synthetic-chat","escape",utf8(database));},"Absolute path from a relative reference rejected");
 resolver.bindFile("synthetic-chat","image-message",png,true);auto preview=resolver.attachments("synthetic-chat",imageRow);expect(preview.size()==2&&preview[0]["variant"]=="full"&&preview[1]["variant"]=="thumbnail","Explicit thumbnail is separate from original");expect(preview[1]["name"]=="original_預覽.png"&&preview[0]["name"]=="original.jpg","A known PNG preview has an explicit preview filename and never replaces the original filename");
 auto network=row("public-image",1,{{"DOWNLOAD_URL","https://obs.line-apps.com/r/official/image?private-signature=synthetic"},{"PREVIEW_URL","https://obs.line-scdn.net/preview?token=synthetic"}});auto remote=resolver.attachments("synthetic-chat",network);
 expect(remote.size()==2&&remote[0]["variant"]=="full"&&remote[1]["variant"]=="thumbnail","Actual metadata original and preview retained separately");expect(remote.dump().find("token")==std::string::npos&&remote.dump().find("signature")==std::string::npos&&remote.dump().find("https://")==std::string::npos,"Signed URLs never leave resolver memory in descriptors");
 auto ciphertext=row("encrypted-image",1,{{"ENC_KM","synthetic-not-a-real-key"},{"DOWNLOAD_URL","https://obs.line-apps.com/r/official/cipher"},{"PREVIEW_URL","https://obs.line-scdn.net/preview"},{"FILE_SIZE","32"}});auto encrypted=resolver.attachments("synthetic-chat",ciphertext);expect(encrypted.size()==1&&encrypted[0]["expected_size"].is_null(),"Encrypted media not downloaded as original or given ciphertext byte count");reject([&]{resolver.copy("synthetic-chat",encrypted[0],stage,control);},"Ciphertext requires a real decrypted original");
 auto encryptedFile=originals/"cache.eimg";write(encryptedFile,jpeg,sizeof jpeg);resolver.bindFile("synthetic-chat","cache-message",encryptedFile);auto cache=resolver.attachments("synthetic-chat",row("cache-message",1));reject([&]{resolver.copy("synthetic-chat",cache[0],stage,control);},"Even magic-like eimg cache is never promoted to original");
 resolver.bindFile("synthetic-chat","wrong-image",file);auto badImage=resolver.attachments("synthetic-chat",row("wrong-image",1));reject([&]{resolver.copy("synthetic-chat",badImage[0],stage,control);},"Text/HTML/ciphertext cannot become an image");
 auto badSize=row("bad-size",14,{{"FILE_NAME","人工文字檔.txt"},{"FILE_SIZE",1}});resolver.bindFile("synthetic-chat","bad-size",file);auto bad=resolver.attachments("synthetic-chat",badSize);reject([&]{resolver.copy("synthetic-chat",bad[0],stage,control);},"Metadata size mismatch rejects explicit wrong file");
 auto infoOnly=row("thumbnail-info",1);infoOnly["_contentInfo"]=J{{"DOWNLOAD_URL","https://obs.line-scdn.net/thumbnail-info"}}.dump();auto infoAttachments=resolver.attachments("synthetic-chat",infoOnly);expect(infoAttachments.size()==2&&infoAttachments[1]["variant"]=="thumbnail","ContentInfo URL is thumbnail only, never original");
 auto keyed=row("e2ee-message",14,{{"DOWNLOAD_URL","https://obs.line-apps.com/r/explicit/encrypted"}});keyed["_text"]=J{{"keyMaterial",mediaMaterial},{"fileName","secret-content-safe-name.txt"},{"fileSize",160}}.dump();auto keyedAttachments=resolver.attachments("synthetic-chat",keyed);expect(keyedAttachments[0]["name"]=="secret-content-safe-name.txt"&&keyedAttachments[0]["expected_size"]==160,"Actual decrypted body media fields bind name and original size");expect(keyedAttachments.dump().find(mediaMaterial)==std::string::npos&&keyedAttachments.dump().find("keyMaterial")==std::string::npos,"Media keys never leak through fallback filename or descriptor");
 expect(resolver.attachments("synthetic-chat",row("text-only",0)).empty(),"Text messages do not fabricate attachments");
#ifndef _WIN32
 auto link=originals/"link.jpg";fs::create_symlink(jpg,link);reject([&]{resolver.bindFile("synthetic-chat","linked",link);},"Symlink media source rejected");
#endif
 {
  Store store(base/"collection");MediaSource source(database,J::array({imageRow,row("file-message",14,{{"FILE_NAME","人工文字檔.txt"},{"FILE_SIZE",digest(file).size}})}));
  auto first=collect(store,source,source.chats(),control,{"message"});expect(first["items"]==2&&first["unavailable"]==2&&first["downloaded"]==0,"Collector registers actual unavailable attachments for retry");
  auto pending=pendingLineAttachments(store);expect(pending.size()==2&&pending[0].contains("id")&&pending[0].contains("chat_title")&&pending[0].contains("name")&&pending[0].contains("item_source_id"),"Native pending UI gets stable selected-row identity and labels");
  auto beforeItems=store.db.rows("SELECT * FROM items ORDER BY id");std::string imageId,fileId;for(const auto& attachment:pending){if(attachment["kind"]=="image")imageId=attachment["id"];else fileId=attachment["id"];}
  reject([&]{importLineAttachment(store,imageId,file,control);},"Wrong original kind rejected before registration");expect(store.db.rows("SELECT status FROM attachments WHERE id=?",{imageId})[0]["status"]=="unavailable","Wrong file leaves pending row unchanged");
  importLineAttachment(store,imageId,jpg,control);importLineAttachment(store,fileId,file,control);
  expect(pendingLineAttachments(store).empty(),"Both real originals fill existing pending rows");expect(store.db.rows("SELECT id FROM attachments").size()==2,"Fill-in does not create duplicate attachment identities");expect(store.db.rows("SELECT * FROM items ORDER BY id")==beforeItems,"Filling originals leaves message rows unchanged");
  expect(store.reusable(imageId)&&store.reusable(fileId),"Filled originals reusable by existing content verification");exportHtml(store);expect(store.check().empty(),"Real image and file produce clean SQLite and offline HTML/download parts");
  auto blobs=store.db.rows("SELECT * FROM blobs ORDER BY id");expect(blobs.size()==2,"Actual attachment bytes stored");for(const auto& blob:blobs)expect(validParts(store,blob),"Offline reader download data is present");
  auto archive=makeZip(store,base/"filled.zip");expect(fs::is_regular_file(base/"filled.zip"),"Full ZIP can include genuine bound originals");
  importLineAttachment(store,imageId,jpg,control);expect(store.db.rows("SELECT * FROM blobs").size()==2,"Repeated same original does not duplicate blobs");
  reject([&]{importLineAttachment(store,"not-an-attachment",jpg,control);},"Unknown attachment ID rejected");reject([&]{pendingLineAttachments(store,1001);},"Pending list bounded");
 }
 {
  Store store(base/"automatic-local");MediaSource source(database,J::array({pngRow}));source.resolver.bindFile("synthetic-chat","png-message",png);auto result=collect(store,source,source.chats(),control,{"message"});expect(result["downloaded"]==1&&result["unavailable"]==0,"Direct bound original flows through collector and renderer");exportHtml(store);expect(store.check().empty(),"Directly bound PNG yields clean reader");
 }
 {
  Store store(base/"original-with-preview");auto both=imageRow;both["_contentMetadata"]=J{{"FILE_NAME","original.jpg"},{"FILE_SIZE",std::to_string(sizeof jpeg)},{"PREVIEW_URL","https://obs.line-scdn.net/never-requested"}}.dump();MediaSource source(database,J::array({both}));source.resolver.bindFile("synthetic-chat","image-message",jpg);
  auto result=collect(store,source,source.chats(),control,{"message"});expect(result["downloaded"]==1&&result["unavailable"]==0,"Saved original makes separately offered preview optional without a network request");expect(store.summary()["issues"]==0&&store.check().empty(),"Optional preview is not reported as missing-original issue");exportHtml(store);expect(store.check().empty(),"Reader publication excludes a redundant pending preview");makeZip(store,base/"original-preview.zip");expect(fs::is_regular_file(base/"original-preview.zip"),"A complete original can be backed up despite redundant thumbnail descriptor");
 }
 {
  Store store(base/"preview-only");MediaSource source(database,J::array({row("preview-message",1)}));source.resolver.bindFile("synthetic-chat","preview-message",png,true);auto result=collect(store,source,source.chats(),control,{"message"});expect(result["downloaded"]==1&&result["unavailable"]==1,"Thumbnail alone never counts as original");expect(!store.check().empty(),"Preview-only collection still reports missing original");reject([&]{makeZip(store,base/"preview-incomplete.zip");},"Full backup refuses preview-only image");expect(pendingLineAttachments(store).size()==1&&pendingLineAttachments(store)[0]["variant"]=="full","Fill-in list contains original row only, not unfillable preview");
 }
 Control stopped;stopped.stopped=true;cancelled([&]{resolver.bindFile("synthetic-chat","stop",jpg,false,&stopped);});cancelled([&]{resolver.attachments("synthetic-chat",imageRow,&stopped);});cancelled([&]{resolver.copy("synthetic-chat",bound[0],stage,stopped);});
 {Store store(base/"cancelled-fill");MediaSource source(database,J::array({imageRow}));collect(store,source,source.chats(),control,{"message"});auto attachment=pendingLineAttachments(store)[0];auto before=store.db.rows("SELECT * FROM attachments");cancelled([&]{importLineAttachment(store,attachment["id"],jpg,stopped);});expect(store.db.rows("SELECT * FROM attachments")==before,"Cancelled original fill-in preserves registration");}
 expect(digest(database).hash==databaseHash&&digest(jpg).hash==jpegHash&&digest(png).hash==pngHash&&digest(file).hash==fileHash,"LINE source and original files never modified");
 std::cout<<checks<<" native LINE media checks passed; actual LINE cache/HTTP not accessed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Native LINE media fixture failed: "<<e.what()<<"\n";return 1;}}
