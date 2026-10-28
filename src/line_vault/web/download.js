"use strict";
window.VaultDownloads = (catalog, notify) => {
 const waiting=new Map();let busy=false,aborted=false,urls=[];
 const hex=bytes=>Array.from(new Uint8Array(bytes),b=>b.toString(16).padStart(2,"0")).join("");
 const stop=()=>{if(aborted)throw new DOMException("Cancelled","AbortError");};
 window.VaultBytes={add:(sha,index,value)=>{const item=waiting.get(sha+":"+index);if(item)item.receive(value);}};
 async function part(sha,index,entry){
  stop();if(!/^downloads\/[a-f0-9]{64}\/\d+\.js$/.test(entry.path)||entry.path!==`downloads/${sha}/${index}.js`)throw Error("Invalid path");
  const encoded=await new Promise((resolve,reject)=>{
   const key=sha+":"+index,script=document.createElement("script");let received=false;
   const timer=setTimeout(()=>finish(Error("Missing part")),15000);
   const finish=error=>{clearTimeout(timer);waiting.delete(key);script.remove();if(error)reject(error);};
   waiting.set(key,{cancel:()=>finish(new DOMException("Cancelled","AbortError")),receive:value=>{if(received||typeof value!=="string"||value.length>1048576||!/^[A-Za-z0-9+/]*={0,2}$/.test(value)){finish(Error("Invalid part"));return;}received=true;resolve(value);}});
   script.src=entry.path;script.onload=()=>finish(received?null:Error("Missing data"));script.onerror=()=>finish(Error("Missing file"));document.head.appendChild(script);
  });
  stop();const raw=atob(encoded),bytes=Uint8Array.from(raw,c=>c.charCodeAt(0));
  if(bytes.length!==entry.size||!window.crypto?.subtle||hex(await window.crypto.subtle.digest("SHA-256",bytes))!==entry.sha256)throw Error("Integrity check failed");
  stop();return bytes;
 }
 async function saveFile(attachment){
  const entry=catalog[attachment.download];
  if(!entry||entry.sha256!==attachment.download||!Array.isArray(entry.parts)||!entry.parts.length)throw Error("No download data");
  if(!Number.isSafeInteger(entry.size)||entry.size<0)throw Error("Invalid size");
  if(typeof window.crypto?.subtle?.digest!=="function"){const error=Error();error.cryptoRequired=true;throw error;}
  let output=null;const chunks=[];let size=0;
  if(entry.size>64*1024*1024){
   if(typeof window.showSaveFilePicker!=="function"){const error=Error();error.browserRequired=true;throw error;}
   const handle=await window.showSaveFilePicker({suggestedName:attachment.name});
   stop();output=await handle.createWritable();
  }
  try{
   for(let n=0;n<entry.parts.length;n++){
    stop();notify(`正在準備 ${attachment.name}（${n+1} / ${entry.parts.length}）`);
    const bytes=await part(entry.sha256,n,entry.parts[n]);size+=bytes.length;if(size>entry.size||(!output&&size>64*1024*1024))throw Error("Wrong size");
    if(output)await output.write(bytes);else chunks.push(bytes);
   }
   stop();if(size!==entry.size)throw Error("Wrong size");
   if(output){await output.close();output=null;}
   else{
    const blob=new Blob(chunks,{type:"application/octet-stream"});
    if(hex(await window.crypto.subtle.digest("SHA-256",await blob.arrayBuffer()))!==entry.sha256)throw Error("Wrong checksum");
    stop();const url=URL.createObjectURL(blob),link=document.createElement("a");link.href=url;link.download=attachment.name;
    document.body.appendChild(link);link.click();link.remove();urls.push(url);
    while(urls.length>4)URL.revokeObjectURL(urls.shift());
    setTimeout(()=>{URL.revokeObjectURL(url);urls=urls.filter(x=>x!==url);},60000);
   }
  }catch(error){if(output)await output.abort().catch(()=>{});throw error;}
 }
 return {
  busy:()=>busy,
  cancel:()=>{if(!busy)return;aborted=true;for(const item of [...waiting.values()])item.cancel();notify("正在停止另存，已完成的檔案仍會保留。");},
  save:async attachments=>{
   if(busy){notify("正在另存檔案，請等目前操作完成。");return;}
   if(attachments.length>1&&attachments.some(a=>catalog[a.download]?.size>64*1024*1024)){notify("選取內容含大檔案，請分別按各檔案的「另存檔案」，或用桌面程式製作 ZIP。");return;}
   busy=true;aborted=false;let count=0;
   try{for(const a of attachments){stop();await saveFile(a);count++;}notify(`已準備 ${count} 個檔案，請查看瀏覽器的下載項目或選定位置。`);}
   catch(error){notify(error.name==="AbortError"?"另存已取消，已完成的檔案仍會保留。":error.cryptoRequired?"這個瀏覽器無法驗證檔案，請用新版 Edge 或 Chrome，或在桌面程式檢查後取用附件。":error.browserRequired?"這份檔案較大，請用新版 Edge 或 Chrome 開啟此頁後另存；原檔仍在附件資料夾。":"無法完成另存，請確認整個封存資料夾都在，並在桌面程式執行檔案檢查。");}
   finally{busy=false;}
  }
 };
};
