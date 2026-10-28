"use strict";
window.VaultGraphModel = (() => {
 const stop=new Set(['我們','你們','他們','這個','那個','可以','已經','目前','如果','所以','以及','還是','沒有','一下','一起','這是','範例','人工','內容','資料','來源','附件','訊息','圖片','檔案','保存','下載','更新','完整','使用','提供','其他','自己','一個','就是','只有','需要','進行','不會','本次','仍會','這份','相同','請問','時候','如何','什麼','這裡','今天','明天','昨天','謝謝']);
 const hash=s=>{let h=2166136261;for(const ch of s){h^=ch.codePointAt(0);h=Math.imul(h,16777619);}return (h>>>0).toString(36);};
 const personKey=i=>'person:'+(i.sender_ref||i.chat_id+':name:'+i.sender_name);
 const label=i=>i.title||i.text?.replace(/\s+/g,' ').slice(0,28)||({message:'對話',album:'相簿',album_item:'相簿照片',note:'記事本',comment:'留言',thread:'討論串'}[i.kind]||'內容');
 const tags=i=>{const values=[...(Array.isArray(i.tags)?i.tags:[]),...Array.from((i.text||'').matchAll(/#([\p{L}\p{N}_\-]{2,24})/gu),m=>m[1])];return [...new Set(values.filter(x=>typeof x==='string'&&x.length>=2&&x.length<=32))].slice(0,12);};
 function terms(rows){
  const counts=new Map(),explicit=new Set();let segmenter;
  try{segmenter=new Intl.Segmenter('zh-Hant',{granularity:'word'});}catch{}
  for(const row of rows){if(['recalled','deleted'].includes(row.state))continue;
   const local=new Set(tags(row));for(const t of local)explicit.add(t);
   const text=(row.title+' '+row.text).normalize('NFKC');
   const words=segmenter?Array.from(segmenter.segment(text),x=>x.isWordLike?x.segment:''):text.match(/[A-Za-z][A-Za-z0-9_-]{2,24}/g)||[];
   for(let w of words){w=w.trim();if(w.length<2||w.length>18||stop.has(w)||/^\d+$/.test(w))continue;local.add(w);}
   for(const t of local){if(!counts.has(t))counts.set(t,new Set());counts.get(t).add(row.id);}
  }
  return [...counts].filter(([t,ids])=>explicit.size?explicit.has(t):(ids.size>=3&&t.length>=2&&!['大家','容易','一份','示意','參考','素材','回程','一起','準備','需要','整理'].includes(t))).sort((a,b)=>(explicit.has(b[0])-explicit.has(a[0]))||(b[1].size-a[1].size)||a[0].localeCompare(b[0],'zh-Hant')).slice(0,explicit.size?12:6).map(([term,ids])=>({term,ids,explicit:explicit.has(term)}));
 }
 function build(rows,chats,options={}){
  const totalRows=rows.length;chats=chats.slice(0,20);const chatIds=new Set(chats.map(c=>c.id));rows=rows.filter(i=>chatIds.has(i.chat_id));
  const nodes=new Map(),links=new Map(),source=new Map(rows.map(i=>[i.id,i]));
  const max=options.limit||120,types=new Set(options.types||['topic','person','source','file']);
  function node(id,type,title,items=[],extra={}){if(nodes.has(id))return nodes.get(id);if(nodes.size>=max&&type!=='chat')return null;const n={id,type,title,items:[...new Set(items)],...extra};nodes.set(id,n);return n;}
  function edge(a,b,type,label){if(!a||!b||a===b||!nodes.has(a)||!nodes.has(b))return;const id=[a,b,type].join('|');links.set(id,{id,source:a,target:b,type,label});}
  const roots=new Map();
  for(const c of chats){const ids=rows.filter(i=>i.chat_id===c.id).map(i=>i.id);if(ids.length||chats.length===1){const id='chat:'+c.id;roots.set(c.id,id);node(id,'chat',c.title,ids,{chatId:c.id});}}
  for(const c of chats)if(c.parent_id&&roots.has(c.parent_id)&&roots.has(c.id))edge(roots.get(c.parent_id),roots.get(c.id),'belongs','社群中的聊天室');
  if(types.has('topic'))for(const t of terms(rows)){
   const n=node('topic:'+t.term,'topic',t.term,[...t.ids],{explicit:t.explicit});if(!n)continue;
   for(const cid of new Set([...t.ids].map(id=>source.get(id)?.chat_id)))edge(roots.get(cid),n.id,'mentions',t.explicit?'使用標籤':'提到關鍵字');
  }
  if(types.has('person')){
   const people=new Map();
   for(const i of rows){if(!i.sender_name)continue;const pid=personKey(i);if(!people.has(pid))people.set(pid,{name:i.sender_name,ids:[],chatIds:new Set(),identified:!!i.sender_ref});people.get(pid).ids.push(i.id);people.get(pid).chatIds.add(i.chat_id);}
   for(const [id,p]of [...people].sort((a,b)=>b[1].ids.length-a[1].ids.length).slice(0,20)){const n=node(id,'person',p.name,p.ids,{identified:p.identified});if(n)for(const cid of p.chatIds)edge(roots.get(cid),id,'speaker','這裡的發言者');}
  }
  const anchors=rows.filter(i=>['note','album','thread'].includes(i.kind));
  const chosen=(options.detail?rows:(anchors.length?anchors:rows.slice(0,4))).slice().sort((a,b)=>{const rank=x=>({note:0,album:1,thread:2,album_item:3,comment:4,message:5}[x.kind]??6);return rank(a)-rank(b)||String(b.sent_at||'').localeCompare(a.sent_at||'');}).slice(0,options.detail?60:18);
  if(types.has('source'))for(const i of chosen){const n=node('source:'+i.id,'source',label(i),[i.id],{kind:i.kind,itemId:i.id});if(!n)continue;edge(roots.get(i.chat_id),n.id,'belongs','聊天室中的內容');}
  for(const i of chosen){const own='source:'+i.id;if(!nodes.has(own))continue;
   if(i.parent_id)edge('source:'+i.parent_id,own,'context','所屬內容');
   const reply=rows.find(r=>r.chat_id===i.chat_id&&r.source_id===i.reply_source_id);if(reply)edge('source:'+reply.id,own,'reply','引用回覆');
   const person=personKey(i);if(i.sender_name)edge(person,own,'author','發言');
   for(const n of nodes.values())if(n.type==='topic'&&n.items.includes(i.id))edge(n.id,own,'mentions',n.explicit?'使用標籤':'提到關鍵字');
  }
  if(types.has('file'))for(const i of rows)for(const a of i.attachments||[]){const n=node('file:'+a.id,'file',a.name,[i.id],{attachment:a,itemId:i.id});if(!n)continue;const parent=nodes.has('source:'+i.id)?'source:'+i.id:(i.parent_id&&nodes.has('source:'+i.parent_id)?'source:'+i.parent_id:roots.get(i.chat_id));edge(parent,n.id,'attachment','這裡保存的附件');for(const t of nodes.values())if(t.type==='topic'&&t.items.includes(i.id))edge(t.id,n.id,'mentions','相關內容的附件');}
  // Every displayed non-room node has an observed containing room as a fallback.
  for(const n of nodes.values())if(n.type!=='chat'&&![...links.values()].some(e=>e.source===n.id||e.target===n.id)){const row=source.get(n.items[0]);if(row)edge(roots.get(row.chat_id),n.id,'belongs','聊天室中的內容');}
  return {nodes:[...nodes.values()],edges:[...links.values()],totalItems:totalRows,limited:totalRows!==rows.length||chosen.length<rows.length||nodes.size>=max,source};
 }
 return {build,label,terms};
})();
