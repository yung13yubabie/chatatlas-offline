// Synthetic downloader contract checks; no browser, network, personal data or large allocation.
const assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs'),{webcrypto,createHash}=require('node:crypto');
const source=fs.readFileSync('src/line_vault/web/download.js','utf8');
const sha=b=>createHash('sha256').update(b).digest('hex');
const bytes=Buffer.from('Artificial attachment only'),hash=sha(bytes);
function setup(options={}){
 const messages=[],state={clicks:0,links:[],appends:0,removed:0,aborts:0,writes:0,closed:0},timers=new Map();let id=0;
 const entry={sha256:hash,size:bytes.length,parts:[{path:`downloads/${hash}/0.js`,size:bytes.length,sha256:hash}],...options.entry};
 const window={crypto:options.crypto===undefined?webcrypto:options.crypto};
 const stream={write:async()=>{state.writes++;if(options.writeError)throw Error('write failure');},close:async()=>{state.closed++;},abort:async()=>{state.aborts++;}};
 if(options.picker)window.showSaveFilePicker=options.picker===true?async()=>({createWritable:async()=>stream}):options.picker;
 const document={createElement:tag=>({remove(){state.removed++;},click(){state.clicks++;state.links.push({href:this.href,name:this.download,target:this.target,rel:this.rel});},tag}),body:{appendChild(){}},head:{appendChild(script){state.appends++;if(options.pending)return;queueMicrotask(()=>{window.VaultBytes.add(hash,0,(options.payload||bytes).toString('base64'));script.onload();});}}};
 const context={window,document,DOMException,Uint8Array,Map,Blob,atob,URL:{createObjectURL:()=> 'blob:synthetic',revokeObjectURL(){}},setTimeout:(fn,delay)=>{if(delay===0&&options.direct){queueMicrotask(fn);return ++id;}timers.set(++id,fn);return id;},clearTimeout:n=>timers.delete(n)};
 vm.runInNewContext(source,context);const api=window.VaultDownloads(options.emptyCatalog?{}:{[hash]:entry},m=>messages.push(m),{directFileDownloads:options.direct===true});return {api,state,messages,entry,timers,attachment:{download:hash,name:'artificial.txt',...(options.direct?{status:'saved',path:'assets/aa/'+'a'.repeat(64)+'.png'}:{})}};
}
async function run(options){const h=setup(options);await h.api.save([h.attachment]);assert.equal(h.api.busy(),false);return h;}
(async()=>{
 let h=await run();assert.equal(h.state.clicks,1);
 for(const crypto of [null,{}, {subtle:{}}]){h=await run({crypto});assert.equal(h.state.clicks,0);assert.equal(h.state.appends,0);assert.match(h.messages.at(-1),/無法驗證/);}
 h=await run({payload:Buffer.from('corrupted')});assert.equal(h.state.clicks,0);
 h=await run({entry:{size:1}});assert.equal(h.state.clicks,0);
 h=await run({entry:{size:-1}});assert.equal(h.state.appends,0);
 h=await run({entry:{size:64*1024*1024+1}});assert.match(h.messages.at(-1),/檔案較大/);assert.equal(h.state.appends,0);
 h=await run({entry:{parts:[{path:'https://example.invalid/payload.js',size:bytes.length,sha256:hash}]}});assert.equal(h.state.appends,0);
 h=setup({pending:true});let pending=h.api.save([h.attachment]);assert.equal(h.api.busy(),true);h.api.cancel();await pending;assert.equal(h.timers.size,0);assert.equal(h.state.removed,1);assert.equal(h.state.clicks,0);assert.match(h.messages.at(-1),/已取消/);
 // Cancel exactly during final whole-file digest: no download may be committed.
 let calls=0;h=setup({crypto:{subtle:{digest:async(...args)=>{const result=await webcrypto.subtle.digest(...args);if(++calls===2)h.api.cancel();return result;}}}});await h.api.save([h.attachment]);assert.equal(h.state.clicks,0);assert.match(h.messages.at(-1),/已取消/);
 // A declared large size selects streaming, but fixtures remain only a few bytes.
 h=await run({picker:true,entry:{size:64*1024*1024+1}});assert.equal(h.state.writes,1);assert.equal(h.state.aborts,1);assert.equal(h.state.closed,0);
 h=await run({picker:true,writeError:true,entry:{size:64*1024*1024+1}});assert.equal(h.state.aborts,1);assert.equal(h.state.closed,0);
 h=await run({picker:async()=>{throw new DOMException('Cancelled','AbortError');},entry:{size:64*1024*1024+1}});assert.match(h.messages.at(-1),/已取消/);assert.equal(h.state.appends,0);
 h=await run({direct:true,emptyCatalog:true,crypto:null});assert.equal(h.state.clicks,1);assert.equal(h.state.appends,0);assert.equal(h.state.links[0].target,'_blank');assert.equal(h.state.links[0].rel,'noopener');assert.match(h.messages.at(-1),/已開啟.*未重新計算/,'Opening preserved local files does not claim hash verification or renamed downloading');
 h=setup({direct:true,emptyCatalog:true});h.attachment.path='https://example.invalid/file.png';await h.api.save([h.attachment]);assert.equal(h.state.clicks,0);
 h=setup({direct:true,emptyCatalog:true});h.attachment.path='assets/aa/../'+'a'.repeat(64)+'.png';await h.api.save([h.attachment]);assert.equal(h.state.clicks,0);
 h=setup({direct:true,emptyCatalog:true});h.attachment.status='pending';await h.api.save([h.attachment]);assert.equal(h.state.clicks,0);
 h=setup({direct:true,emptyCatalog:true});pending=h.api.save([h.attachment,{...h.attachment,name:'second.png'}]);h.api.cancel();await pending;assert.equal(h.state.clicks,1);assert.match(h.messages.at(-1),/已取消/);
 h=await run({direct:true,crypto:null});assert.equal(h.state.clicks,0);assert.match(h.messages.at(-1),/無法驗證/,'An existing verified download entry never falls through to unverified opening');
 console.log('Downloader contract: 20 scenarios passed (synthetic VM; not browser acceptance).');
})().catch(e=>{console.error(e);process.exitCode=1;});
