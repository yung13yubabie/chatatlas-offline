"use strict";
(() => {
 const manifest=JSON.parse(document.getElementById('manifest').textContent),$=id=>document.getElementById(id),presentation=window.VaultGraphModel.presentation;
 const kinds={message:'對話',album:'相簿',album_item:'相簿照片',note:'記事本',comment:'留言',thread:'討論串'};
 const chatKinds={direct:'個人對話',group:'群組',community:'社群',subchat:'社群聊天室'};
 const types={chat:'聊天室',topic:'關鍵字',person:'人物',source:'訊息',file:'圖片或檔案'};
 const states={available:'已保存原檔',preview:'僅有預覽',pending:'等待保存',failed:'保存失敗',unavailable:'未取得'};
 const fileKinds={image:'圖片',video:'影片',audio:'語音',file:'一般檔案',sticker:'貼圖'};
 manifest.chats=manifest.chats.map((c,index)=>{const fallback='未命名'+(c.kind==='direct'?'對話':'聊天室')+' '+(index+1),title=presentation.displayName(c.title,fallback,c.name_resolved),unresolved=c.name_resolved===false||title===fallback||/^未命名(?:對話|群組|聊天室|社群)/.test(title);return {...c,title:title+(unresolved?'（名稱待確認）':'')};});
 const data=new Map(),loading=new Map(),selected=new Map(),chatTitles=new Map(manifest.chats.map(c=>[c.id,c.title]));
 const chatPageSize=8,readingPageSize=60,filePageSize=24;
 let chat=manifest.chats[0]?.id||null,page=0,filePage=0,chatPage=0,version=0,current=[],filtered=[],view='reading',focusNode=null,mapModel=null,mapDirty=true,contextKey='',visibleFiles=[];
 const el=(tag,text,cls)=>{const n=document.createElement(tag);if(text!==undefined)n.textContent=text;if(cls)n.className=cls;return n;};
 const localPath=p=>typeof p==='string'&&/^assets\/[a-f0-9]{2}\/[a-f0-9]{64}\.(png|jpg|gif|webp|wav|ogg|mp3|mp4|bin)$/.test(p)?p:null;
 const date=t=>t&&Number.isFinite(Date.parse(t))?new Date(t).toLocaleString('zh-TW',{timeZone:'Asia/Taipei',year:'numeric',month:'numeric',day:'numeric',hour:'2-digit',minute:'2-digit'}):'未標示時間';
 const size=n=>n==null?'大小未提供':n>=1048576?(n/1048576).toFixed(1)+' MB':n>=1024?(n/1024).toFixed(1)+' KB':n+' B';
 // Capture presentation helpers: plain text only, not an assertion of full LINE coverage.
 const unfinished=new Set(['running','stopped','failed','interrupted','complete_with_issues']);
 function captureSummary(chats){
  if(chats.length!==1){const pending=chats.filter(c=>!c.last_job||unfinished.has(c.last_job.status)).length,unknown=chats.filter(c=>!c.last_job||!['local_only','visible_only','source_range'].includes(c.last_job.coverage)).length;return chats.length+' 個聊天室'+(pending?' · '+pending+' 個尚未完成或有需處理項目':'')+(unknown?' · '+unknown+' 個來源範圍未確認':' · 範圍依各聊天室紀錄');}
  const job=chats[0]?.last_job;if(!job)return '尚無整理紀錄，已保存訊息仍可閱讀。';
  const statuses={complete:'上次整理完成',complete_with_issues:'上次整理有需處理項目',stopped:'上次整理已停止，已保存內容保留',failed:'上次整理未完成',interrupted:'上次整理曾中斷',running:'上次整理尚未完成'};
  const ranges={local_only:'僅限本機來源提供的內容',visible_only:'僅限當時可讀範圍',source_range:'依所選來源範圍，不代表完整歷史'};
  const parts=[statuses[job.status]||'上次整理狀態待確認',ranges[job.coverage]||'來源範圍尚未確認'];
  if(job.finished_at&&Number.isFinite(Date.parse(job.finished_at)))parts.push(date(job.finished_at));
  const counts=job.counts||{};for(const [key,label]of [['downloaded','新增檔案'],['reused','沿用檔案'],['failed','待重試'],['unavailable','未取得']])if(Number.isSafeInteger(counts[key])&&counts[key]>=0)parts.push(label+' '+counts[key]);
  return parts.join(' · ');
 }
 function attachmentReason(a){if(a.status==='saved')return '';return typeof a.reason==='string'&&a.reason.trim()?a.reason.trim().replaceAll('附件','檔案').slice(0,300):a.status==='preview'?'目前只有預覽版本，原檔尚未取得。':a.status==='unavailable'?'此次未取得原檔，可回程式重新嘗試。':a.status==='failed'?'保存時發生錯誤，可回程式重試。':'此檔案尚未完成保存，可繼續整理。';}
 // End capture presentation helpers.
 const direct=manifest.direct_file_downloads===true;
 const downloads=window.VaultDownloads(manifest.downloads||{},message=>{$('download-notice').textContent=message;$('cancel-download').hidden=!downloads.busy();},{directFileDownloads:direct});
 const graph=window.VaultGraphView($('knowledge-map'),showNode);
 if(manifest.artificial_data){$('sample-label').hidden=false;document.title='聊藏 · 人工聊天記錄範例';}
 if(direct)$('files-description').textContent='已保存的本機檔案可在新分頁開啟。若要用畫面顯示的檔名另存，請回桌面程式更新閱讀頁。';
 function presentRows(rows,id){return rows.map(i=>{const resolved=i.sender_name_resolved??i.metadata?.sender_name_resolved,name=presentation.displayName(i.sender_name,i.kind==='message'||i.kind==='thread'?'名稱待確認':kinds[i.kind]||'系統訊息',resolved);return {...i,chat_id:i.chat_id||id,sender_ref:i.sender_ref||(presentation.technicalName(i.sender_name)?'unresolved:'+id+':'+i.sender_name:undefined),sender_name:name+(name!=='名稱待確認'&&(resolved===false||/^未知成員[（(]/.test(name))?'（名稱待確認）':''),title:presentation.displayName(i.title,'',i.name_resolved),attachments:(i.attachments||[]).map((a,index)=>({...a,name:presentation.fileName(a,i,index),generated_name:false,name_is_original:true}))};});}
 window.VaultData={add:(id,rows)=>{if(!data.has(id))data.set(id,[]);data.get(id).push(...presentRows(rows,id));}};
 function load(id){
  if(loading.has(id))return loading.get(id);const c=manifest.chats.find(c=>c.id===id);if(!c)return Promise.reject(Error('Unknown chat'));data.set(id,[]);
  const job=(async()=>{for(const src of c.chunks){if(!/^reader\/[a-f0-9]{32}\/[a-f0-9]{64}-\d+\.js$/.test(src))throw Error('Invalid local data');await new Promise((ok,fail)=>{const s=el('script');s.src=src;s.onload=ok;s.onerror=fail;document.head.appendChild(s);});}return data.get(id);})();loading.set(id,job);return job;
 }
 function setView(next){
  view=next;for(const [id,section,key]of [['view-reading','reading-section','reading'],['view-files','files-section','files'],['view-map','map-section','map']]){$(id).setAttribute('aria-selected',String(key===view));$(id).tabIndex=key===view?0:-1;$(section).hidden=key!==view;}
  if(view==='reading')drawReading();if(view==='files')drawFiles();if(view==='map')drawMap();
 }
 function nav(){
  const n=$('chats');n.replaceChildren();$('chat-count').textContent=manifest.chats.length;
  const query=$('chat-search').value.trim().toLocaleLowerCase(),kind=$('chat-kind').value;
  const chats=manifest.chats.filter(c=>(!query||c.title.toLocaleLowerCase().includes(query))&&(kind==='all'||c.kind===kind));
  const pages=Math.max(1,Math.ceil(chats.length/chatPageSize));chatPage=Math.min(Math.max(0,chatPage),pages-1);
  $('chat-results').textContent=query||kind!=='all'?'符合 '+chats.length+' 個，共 '+manifest.chats.length+' 個':'';
  for(const c of chats.slice(chatPage*chatPageSize,(chatPage+1)*chatPageSize)){
   const b=el('button');b.append(el('span',c.title,'chat-label'),el('span',c.count+' 筆紀錄 · '+(chatKinds[c.kind]||'聊天室'),'chat-meta'));b.className=c.id===chat&&$('scope').value!=='all'?'active':'';b.setAttribute('aria-pressed',String(c.id===chat&&$('scope').value!=='all'));
   b.onclick=()=>{chat=c.id;$('scope').value='chat';focusNode=null;page=0;filePage=0;setView('reading');nav();render();};n.append(b);
  }
  if(!chats.length)n.append(el('p','沒有符合條件的聊天室。','sidebar-empty'));
  $('chat-page').textContent=(chatPage+1)+' / '+pages;$('chat-previous').disabled=chatPage===0;$('chat-next').disabled=chatPage+1>=pages;
  const all=$('scope').value==='all';$('all-chats').classList.toggle('active',all);$('all-chats').setAttribute('aria-pressed',String(all));
 }
 function downloadable(a){return !!localPath(a.path)&&['saved','preview'].includes(a.status)&&!!manifest.downloads?.[a.download];}
 function locallyOpenable(a){return direct&&!!localPath(a.path)&&['saved','preview'].includes(a.status)&&!downloadable(a);}
 const fileAction=a=>locallyOpenable(a)?a.status==='preview'?'開啟本機預覽':'開啟本機原檔':a.status==='preview'?'另存預覽':'另存檔案';
 function fileStatus(a){if(a.status==='saved')return downloadable(a)||locallyOpenable(a)?'available':'failed';if(a.status==='preview')return localPath(a.path)?'preview':'failed';return ['pending','failed','unavailable'].includes(a.status)?a.status:'pending';}
 function canPreview(a){const p=localPath(a.path);return p&&['saved','preview'].includes(a.status)&&((['image','sticker'].includes(a.kind)&&/\.(png|jpg|gif|webp)$/.test(p))||(a.kind==='video'&&p.endsWith('.mp4'))||(a.kind==='audio'&&/\.(wav|ogg|mp3)$/.test(p)));}
 function preview(a){const box=$('preview-content');box.replaceChildren();const p=localPath(a.path);if(!canPreview(a))return;let media;if(['image','sticker'].includes(a.kind)){media=el('img');media.alt=a.name;}else{media=el(a.kind==='video'?'video':'audio');media.controls=true;}media.src=p;media.onerror=()=>box.replaceChildren(el('p','無法預覽此檔案，請嘗試另存或回程式檢查。'));box.append(media);$('preview').showModal();}
 function counts(){$('selected-count').textContent=selected.size?'已選 '+selected.size+' 個檔案':'尚未選取檔案';$('save-selected').disabled=!selected.size;$('clear-file-selection').disabled=!selected.size;}
 async function save(a){await downloads.save([a]);$('cancel-download').hidden=true;}
 function checkbox(a){const cb=el('input');cb.type='checkbox';cb.checked=selected.has(a.id);cb.setAttribute('aria-label','選取 '+a.name);cb.onchange=()=>{if(cb.checked)selected.set(a.id,a);else selected.delete(a.id);counts();};return cb;}
 function displayReason(a){return a.status==='saved'&&fileStatus(a)==='failed'?'保存紀錄存在，但本機檔案或另存資料需檢查。請回程式檢查或重新產生閱讀頁。':attachmentReason(a);}
 function attachment(a){
  const row=el('div',undefined,'attachment');if(canPreview(a)&&['image','sticker'].includes(a.kind)){const img=el('img');img.src=localPath(a.path);img.alt=a.name;img.loading='lazy';img.onerror=()=>img.remove();row.append(img);}
  const info=el('div',undefined,'file-info'),status=fileStatus(a);info.append(el('div',a.name,'filename'),el('div',(a.saved_before?'先前保存 · ':'')+(states[status]||'待確認')+' · '+size(a.size),'status status-'+status));const reason=displayReason(a);if(reason)info.append(el('p',reason,'attachment-reason'));row.append(info);
  if(canPreview(a)){const b=el('button','預覽','secondary');b.onclick=()=>preview(a);row.append(b);}if(downloadable(a)||locallyOpenable(a)){const b=el('button',fileAction(a),'save-file');b.onclick=()=>save(a);row.append(b);}return row;
 }
 function meta(i){const line=el('div',undefined,'meta');line.append(el('span',i.sender_name||kinds[i.kind]||'系統訊息','name'),el('span',kinds[i.kind]||'訊息','type'));if(i.sent_at)line.append(el('time',date(i.sent_at)));if($('scope').value==='all'&&chatTitles.has(i.chat_id))line.append(el('span',chatTitles.get(i.chat_id),'origin'));return line;}
 const bodyText=i=>i.state==='recalled'?'此訊息已收回':i.state==='deleted'?'此內容已刪除':i.content_omitted===true?'未保存對話文字':/^\[LINE 類型 18\]$/.test(i.text||'')?'系統訊息（詳細文字尚未取得）':/^\[LINE 類型 \d+\]$/.test(i.text||'')?'此訊息的文字內容尚未取得。':i.text;
 function card(i){
  const c=el('article',undefined,'card');c.id='item-'+i.id;c.append(meta(i));if(i.title)c.append(el('h3',i.title));
  if(i.parent_id){const p=current.find(r=>r.id===i.parent_id);if(p){const b=el('button','查看所屬'+(kinds[p.kind]||'訊息'),'secondary');b.onclick=()=>jump(p.id);c.append(b);}else c.append(el('p','所屬訊息尚未保存。','context'));}
  if(i.reply_source_id){const r=current.find(r=>r.chat_id===i.chat_id&&r.source_id===i.reply_source_id);if(r){const b=el('button','查看引用訊息','secondary');b.onclick=()=>jump(r.id);c.append(b);}else c.append(el('p','引用的原訊息尚未保存。','context'));}
  c.append(el('p',bodyText(i),'text'));for(const a of i.attachments)c.append(attachment(a));return c;
 }
 function showNode(node,overview=false){
  if(!node){const first=mapModel?.nodes.find(n=>n.type==='chat');if(first)return showNode(first,true);graph.select(null);return;}
  graph.select(overview?null:node.id);const panel=$('node-detail');panel.replaceChildren();panel.append(el('span',types[node.type]||'訊息','detail-type'),el('h3',node.title));
  const subtitle=node.type==='topic'?'含有此關鍵字的已保存訊息。':node.type==='person'?'來自這位成員的已保存訊息。':node.type==='file'?'這個檔案的保存狀態與相關訊息。':'每個節點都能回到已保存的原文。';panel.append(el('p',subtitle,'detail-subtitle'));
  if(node.attachment)panel.append(attachment(node.attachment));const related=node.items.map(id=>current.find(i=>i.id===id)).filter(Boolean);panel.append(el('div',related.length+' 則相關訊息','detail-count'));
  for(const i of related.slice(0,3)){const entry=el('div',undefined,'detail-entry');entry.append(meta(i));if(i.title)entry.append(el('h4',i.title));entry.append(el('p',bodyText(i),'text'));const b=el('button','閱讀原文 ↗','detail-open');b.onclick=()=>jump(i.id);entry.append(b);panel.append(entry);}
  if(related.length){const b=el('button','閱讀全部相關內容','secondary');b.onclick=()=>{focusNode={title:node.title,ids:new Set(node.items)};page=0;setView('reading');};panel.append(b);}
 }
 function drawMap(){
  if(!mapDirty)return;mapDirty=false;const chats=manifest.chats.filter(c=>$('scope').value==='all'||c.id===chat);mapModel=window.VaultGraphModel.build(filtered,chats);graph.set(mapModel);$('map-size').textContent=mapModel.limited?'地圖僅顯示部分節點；訊息紀錄保留全部已保存內容':'';$('map-empty').hidden=filtered.length>0;
  const first=mapModel.nodes.find(n=>n.type==='chat');if(first)showNode(first,true);else $('node-detail').replaceChildren(el('p','清除篩選或選擇其他聊天室。','detail-subtitle'));
 }
 function drawReading(){
  const rows=focusNode?filtered.filter(i=>focusNode.ids.has(i.id)):filtered,pages=Math.max(1,Math.ceil(rows.length/readingPageSize));page=Math.max(0,Math.min(page,pages-1));$('reading-label').textContent=focusNode?'關於「'+focusNode.title+'」':'已保存的完整訊息';$('clear-node').hidden=!focusNode;
  const box=$('items');box.replaceChildren();for(const i of rows.slice(page*readingPageSize,(page+1)*readingPageSize))box.append(card(i));if(!rows.length)box.append(el('div','沒有符合條件的訊息，請調整日期或清除篩選。','empty'));$('page').textContent='第 '+(page+1)+' / '+pages+' 頁 · 共 '+rows.length+' 筆';$('previous').disabled=page===0;$('next').disabled=page+1>=pages;
 }
 function scopedFiles(){return filtered.flatMap(i=>i.attachments.map(a=>({i,a})));}
 function matchingFiles(){const status=$('file-status').value,kind=$('file-kind').value;return scopedFiles().filter(({a})=>(status==='all'||fileStatus(a)===status)&&(kind==='all'||a.kind===kind));}
 function drawFileSummary(files){
  const summary=$('file-summary');summary.replaceChildren();for(const [key,label]of [['all','全部檔案'],...Object.entries(states)]){const count=key==='all'?files.length:files.filter(({a})=>fileStatus(a)===key).length,b=el('button',label+' '+count,'status-chip status-'+key);b.setAttribute('aria-pressed',String($('file-status').value===key));b.onclick=()=>{$('file-status').value=key;filePage=0;drawFiles();};summary.append(b);}
 }
 function drawFiles(){
  const box=$('file-grid');box.replaceChildren();drawFileSummary(scopedFiles());visibleFiles=matchingFiles();$('file-selection-toolbar').hidden=direct&&!scopedFiles().some(({a})=>downloadable(a));const pages=Math.max(1,Math.ceil(visibleFiles.length/filePageSize));filePage=Math.max(0,Math.min(filePage,pages-1));
  for(const {i,a}of visibleFiles.slice(filePage*filePageSize,(filePage+1)*filePageSize)){
   const c=el('article',undefined,'file-card'),cover=el('div',undefined,'file-cover'),status=fileStatus(a);c.dataset.status=status;
   if(canPreview(a)&&['image','sticker'].includes(a.kind)){const img=el('img');img.src=localPath(a.path);img.alt=a.name;img.loading='lazy';cover.append(img);cover.onclick=()=>preview(a);}else cover.append(el('span',fileKinds[a.kind]||'檔案','file-kind-label'));
   cover.append(el('span',states[status]||'待確認','file-badge status-'+status));
   const body=el('div',undefined,'file-card-body');body.append(el('h3',a.name),el('p',date(i.sent_at)+' · '+size(a.size),'file-origin'),el('p',(chatTitles.get(i.chat_id)||'聊天室')+' · '+i.sender_name,'file-origin'));const reason=displayReason(a);if(reason)body.append(el('p',reason,'attachment-reason'));
   const actions=el('div',undefined,'file-card-actions');if(downloadable(a))actions.append(checkbox(a));if(canPreview(a)){const b=el('button','預覽','secondary');b.onclick=()=>preview(a);actions.append(b);}if(downloadable(a)||locallyOpenable(a)){const b=el('button',fileAction(a),'save-file');b.onclick=()=>save(a);actions.append(b);}else actions.append(el('span','原檔尚不可另存','file-unavailable'));const message=el('button','查看訊息','text-button');message.onclick=()=>jump(i.id);actions.append(message);body.append(actions);c.append(cover,body);box.append(c);
  }
  if(!visibleFiles.length)box.append(el('div','沒有符合此狀態與類型的檔案。可切回「全部狀態」查看。','empty'));$('file-page').textContent='第 '+(filePage+1)+' / '+pages+' 頁 · 共 '+visibleFiles.length+' 個';$('file-previous').disabled=filePage===0;$('file-next').disabled=filePage+1>=pages;$('select-file-page').disabled=!visibleFiles.slice(filePage*filePageSize,(filePage+1)*filePageSize).some(({a})=>downloadable(a));$('select-file-filtered').disabled=!visibleFiles.some(({a})=>downloadable(a));counts();
 }
 function isKind(i,kind){if(kind==='all')return true;if(kind==='files')return i.attachments.length>0;if(kind==='issues')return i.attachments.some(a=>!['available','preview'].includes(fileStatus(a)));let p=i,seen=new Set();while(p&&!seen.has(p.id)){seen.add(p.id);if(p.kind===kind)return true;p=current.find(x=>x.id===p.parent_id);}return false;}
 function drawStats(){
  const files=scopedFiles(),counts=[[(filtered.filter(i=>i.kind==='message'||i.kind==='thread').length),' 則訊息'],[files.filter(({a})=>['available','preview'].includes(fileStatus(a))).length,' 個本機檔案'],[files.filter(({a})=>!['available','preview'].includes(fileStatus(a))).length,' 個需處理']];$('stats').replaceChildren();for(const [count,label]of counts){const item=el('div',undefined,'stat');item.append(el('strong',String(count)),el('span',label));$('stats').append(item);}
 }
 function drawNotice(chats){
  const files=scopedFiles(),issues=files.filter(({a})=>!['available','preview'].includes(fileStatus(a))).length,stopped=chats.some(c=>['stopped','interrupted','running'].includes(c.last_job?.status));
  let notice=stopped?'上次整理已停止或中斷，已保存的訊息與本機檔案保留在這裡。可回桌面程式繼續整理。':'';
  if(issues)notice+=(notice?' ':'')+issues+' 個檔案需要處理；請到「圖片、影片與檔案」查看名稱、狀態與原因。';
  else if(!notice&&chats.some(c=>c.last_job?.status==='failed'))notice='上次整理未完成，本頁顯示當時已保存的訊息。可回桌面程式重試。';
  else if(!notice&&chats.some(c=>c.last_job?.status==='complete_with_issues'))notice='上次整理紀錄有需處理項目；本頁顯示目前已保存的訊息與檔案。可回桌面程式查看整理紀錄。';
  if(direct)notice+=(notice?' ':'')+'已保存的本機原檔仍可開啟；若要用顯示檔名另存，請回桌面程式更新閱讀頁。';$('notice').textContent=notice;
 }
 async function render(){
  const turn=++version;$('notice').textContent='';$('content').setAttribute('aria-busy','true');$('filter-summary').textContent='正在載入訊息…';
  const ids=$('scope').value==='all'?manifest.chats.map(c=>c.id):chat?[chat]:[],key=ids.join('|'),all=$('scope').value==='all',c=manifest.chats.find(c=>c.id===chat);
  $('title').textContent=all?'全部聊天室':c?.title||'沒有聊天室';$('section-label').textContent=all?'聊天總覽':'聊天記錄';
  if(key!==contextKey){contextKey=key;current=[];filtered=[];mapDirty=true;drawStats();$('coverage').textContent=captureSummary(manifest.chats.filter(c=>ids.includes(c.id)));$('items').replaceChildren(el('p','正在載入此範圍的已保存訊息…','empty'));$('file-grid').replaceChildren();if(view==='map')drawMap();}
  try{
   for(let offset=0;offset<ids.length;offset+=4){await Promise.all(ids.slice(offset,offset+4).map(load));if(turn!==version)return;}
   current=ids.flatMap(id=>data.get(id)||[]);const q=$('search').value.trim().toLocaleLowerCase(),sender=$('sender').value.trim().toLocaleLowerCase(),kind=$('kind').value;
   filtered=current.filter(i=>{const text=[i.text,i.title,i.sender_name,...i.attachments.map(a=>a.name)].join(' ').toLocaleLowerCase(),day=i.sent_at&&Number.isFinite(Date.parse(i.sent_at))?new Date(i.sent_at).toLocaleDateString('en-CA',{timeZone:'Asia/Taipei'}):'';return (!q||text.includes(q))&&(!sender||i.sender_name.toLocaleLowerCase().includes(sender))&&isKind(i,kind)&&(!$('since').value||day&&day>=$('since').value)&&(!$('until').value||day&&day<=$('until').value);});
   const chats=manifest.chats.filter(c=>ids.includes(c.id));$('coverage').textContent=captureSummary(chats);drawStats();drawNotice(chats);mapDirty=true;drawReading();drawFiles();if(view==='map')drawMap();nav();
  }catch{if(turn===version){current=[];filtered=[];mapDirty=true;drawStats();drawReading();drawFiles();if(view==='map')drawMap();$('notice').textContent='部分訊息檔案讀不到。請確認整份保存資料夾都在，或回桌面程式重新產生閱讀頁。';}}
  finally{if(turn===version){$('content').setAttribute('aria-busy','false');updateFilterSummary();}}
 }
 async function jump(id){const target=current.find(i=>i.id===id);if(!target)return;for(const key of ['search','sender','since','until'])$(key).value='';$('kind').value='all';focusNode=null;await render();page=Math.floor(filtered.findIndex(i=>i.id===id)/readingPageSize);setView('reading');const n=document.getElementById('item-'+id);n?.setAttribute('tabindex','-1');n?.scrollIntoView({block:'center'});n?.focus({preventScroll:true});}
 function updateFilterSummary(){const count=['search','sender','since','until'].filter(id=>$(id).value.trim()).length+Number($('kind').value!=='all');$('filter-count').textContent=count;$('filter-count').hidden=!count;$('reset-filters').hidden=!count;const scope=$('scope').value==='all'?'全部聊天室':'目前聊天室';$('filter-summary').textContent=count?scope+' · '+count+' 項篩選 · '+filtered.length+' 筆結果':scope+' · 全部已保存訊息';}
 function clearFilters(){for(const id of ['search','sender','since','until'])$(id).value='';$('kind').value='all';focusNode=null;page=0;filePage=0;render();}
 const tabs=[$('view-reading'),$('view-files'),$('view-map')],views=['reading','files','map'];
 for(const [index,tab]of tabs.entries())tab.addEventListener('keydown',event=>{let next=index;if(event.key==='ArrowRight')next=(index+1)%tabs.length;else if(event.key==='ArrowLeft')next=(index+tabs.length-1)%tabs.length;else if(event.key==='Home')next=0;else if(event.key==='End')next=tabs.length-1;else return;event.preventDefault();setView(views[next]);tabs[next].focus();});
 document.addEventListener('keydown',event=>{const editing=/INPUT|TEXTAREA|SELECT/.test(document.activeElement?.tagName||'');if(event.key==='/'&&!editing&&!event.ctrlKey&&!event.metaKey&&!event.altKey){event.preventDefault();$('search').focus();}if(event.key==='Escape'){const open=document.querySelector('details[open]');if(open){open.open=false;open.querySelector('summary')?.focus();}}});
 document.addEventListener('click',event=>{for(const details of document.querySelectorAll('details[open]'))if(!details.contains(event.target))details.open=false;});
 $('reset-filters').onclick=clearFilters;$('clear-map-selection').onclick=()=>{showNode(null);$('knowledge-map').querySelector('[tabindex="0"]')?.focus();};$('preview').addEventListener('close',()=>{$('preview-content').replaceChildren();});
 for(const [id,key]of [['view-map','map'],['view-reading','reading'],['view-files','files']])$(id).onclick=()=>setView(key);
 for(const id of ['search','sender','since','until','kind','scope'])$(id).addEventListener('input',()=>{page=0;filePage=0;focusNode=null;render();});
 for(const id of ['chat-search','chat-kind'])$(id).addEventListener('input',()=>{chatPage=0;nav();});
 for(const id of ['file-status','file-kind'])$(id).addEventListener('input',()=>{filePage=0;drawFiles();});
 $('chat-previous').onclick=()=>{chatPage--;nav();};$('chat-next').onclick=()=>{chatPage++;nav();};
 $('all-chats').onclick=()=>{$('scope').value='all';focusNode=null;page=0;filePage=0;setView('map');render();};$('clear').onclick=clearFilters;$('clear-node').onclick=()=>{focusNode=null;page=0;drawReading();};$('previous').onclick=()=>{page--;drawReading();};$('next').onclick=()=>{page++;drawReading();};$('file-previous').onclick=()=>{filePage--;drawFiles();};$('file-next').onclick=()=>{filePage++;drawFiles();};
 $('select-file-page').onclick=()=>{for(const {a}of visibleFiles.slice(filePage*filePageSize,(filePage+1)*filePageSize))if(downloadable(a))selected.set(a.id,a);drawFiles();};$('select-file-filtered').onclick=()=>{for(const {a}of visibleFiles)if(downloadable(a))selected.set(a.id,a);drawFiles();};$('clear-file-selection').onclick=()=>{selected.clear();drawFiles();};
 $('zoom-in').onclick=()=>graph.zoom(1.2);$('zoom-out').onclick=()=>graph.zoom(1/1.2);$('map-center').onclick=()=>graph.center();$('close-preview').onclick=()=>{$('preview').close();$('preview-content').replaceChildren();};$('cancel-download').onclick=()=>downloads.cancel();$('save-selected').onclick=async()=>{await downloads.save([...selected.values()]);$('cancel-download').hidden=true;};nav();setView('reading');render();
})();
