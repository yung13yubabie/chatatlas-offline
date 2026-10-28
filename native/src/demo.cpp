#include "core.hpp"
#include "demo_image.hpp"
namespace atlas {
DemoSource::DemoSource(int v):revision(v){account="synthetic-native-account";}
J DemoSource::chats(){return J::array({{{"source_id","project"},{"title","山徑企劃小組"},{"kind","group"}},{{"source_id","community"},{"title","週末山行社群"},{"kind","community"}},{{"source_id","photo-room"},{"title","攝影討論室"},{"kind","subchat"},{"parent_source_id","community"}}});}
Batch DemoSource::read(const J&chat,const std::set<std::string>&kinds,const std::string&since,const std::string&until,Control&control){control.check();Batch b;b.coverage="source_range";auto item=[&](std::string id,std::string kind,std::string content,std::string who,std::string title="",std::string parent="",std::string reply="",int minute=0){J i={{"source_id",id},{"kind",kind},{"text",content},{"title",title},{"sender_name",who},{"sender_id",who.empty()?J(nullptr):J("member-"+who)},{"sent_at","2026-10-24T09:"+(minute<10?std::string("0"):std::string())+std::to_string(minute)+":00+08:00"},{"ordinal",b.items.size()}};if(!parent.empty())i["parent_source_id"]=parent;if(!reply.empty())i["reply_source_id"]=reply;if(kinds.count(kind))b.items.push_back(i);};
 auto cid=chat["source_id"].get<std::string>();if(cid=="project"){
 item("m1","message",revision==1?"這週的 #北稜試走，我們把路線、補給和拍攝計畫整理在一起。全程約 12 公里，行程以當天狀況調整。":"#北稜試走 已更新集合安排，請以記事本的最新版為準。","阿嵐");
 item("m2","message","#補給 清單已整理，先準備水、行動糧與備用保暖層。清單也附在這則訊息。","小葉","","","m1",3);
 item("m3","message","#拍攝計畫 想記錄林間光線與稜線的變化。我把構圖草圖收進相簿，大家可以先看。","阿辰","","","m1",6);
 item("m4","message","#北稜試走 如果午後天氣轉差，就提早回程。#補給 和安全裝備都按完整行程準備。","阿嵐","","","m2",8);
 item("album","album","拍攝與路線的人工示意素材。","阿辰","北稜試走 · 路線示意");
 item("image","album_item","#拍攝計畫 林間段的光線與色調參考。這張圖片是人工示意。","阿辰","路線色調參考","album");
 item("note","note","#北稜試走\n集合：早上 07:00。\n攜帶：飲水、行動糧、頭燈與保暖層。\n#補給 分開放在容易取用的位置。\n這是人工建立的企劃範例，不是真實行程建議。","小葉","行前準備與補給備忘");
 item("comment","comment","#補給 也可以留一份緊急備用，避免把預備量提早吃完。","阿嵐","","note");
 item("thread","thread","","阿辰","拍攝分工討論","m3");
 item("reply","message","我負責 #拍攝計畫 的稜線段，回程再補森林近景。","阿辰","","thread","",11);
 }else if(cid=="community"){
 item("m1","message","把 #北稜試走 的準備心得放在這裡，歡迎大家補充。","山友小禾");
 item("m2","message","#補給 最實用的方式，是把常用的那一份放在容易拿到的位置。","小葉","","","m1",5);
 item("note","note","這份 #補給 筆記整理了社群裡常見的準備問題，實際需求仍要依自己的行程決定。","山友小禾","社群的行前準備筆記");
 item("comment","comment","#北稜試走 的照片可以放進攝影討論室，之後比較容易找到。","阿辰","","note");
 }else{
 item("m1","message","這裡集中整理 #拍攝計畫、示意圖片和構圖想法。","阿辰");
 item("album","album","#拍攝計畫 人工建立的素材集合。","阿辰","林間光線 · 素材收藏");
 item("image","album_item","#北稜試走 的 #拍攝計畫 色調參考。","阿辰","林間色調參考","album");
 item("note","note","#拍攝計畫\n1. 林間逆光\n2. 稜線剪影\n3. 手部取用 #補給 的細節\n這份清單只供人工介面示範。","小葉","鏡頭與素材清單");
 item("comment","comment","#補給 的手部細節可以放在片段之間，讓故事多一點生活感。","阿嵐","","note");
 }
 std::string file="人工範例補給清單\n飲水、行動糧、頭燈、保暖層。\n這不是實際行程指引。\n";std::set<std::string>present;for(auto&i:b.items)present.insert(i["source_id"].get<std::string>());if(present.count("m2")&&cid=="project")b.attachments.push_back({{"source_id","checklist"},{"item_source_id","m2"},{"name","行前補給清單.txt"},{"kind","file"},{"expected_size",file.size()},{"expected_sha256",sha(file)}});if(present.count("image"))b.attachments.push_back({{"source_id","illustration"},{"item_source_id","image"},{"name","路線色調參考.png"},{"kind","image"},{"expected_size",demo_png_size},{"expected_sha256",sha(std::string_view(reinterpret_cast<const char*>(demo_png),demo_png_size))}});return b;}
fs::path DemoSource::download(const J&,const J&a,const fs::path&dest,Control&c){c.check();downloads++;std::string bytes=a["kind"]=="image"?std::string(reinterpret_cast<const char*>(demo_png),demo_png_size):"人工範例補給清單\n飲水、行動糧、頭燈、保暖層。\n這不是實際行程指引。\n";auto p=dest/"payload.bin";atomicText(p,bytes);return p;}
}
