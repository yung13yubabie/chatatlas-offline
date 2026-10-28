// Real Chromium file:// tests. No --disable-web-security or local HTTP server.
const {chromium}=require("playwright");
const fs=require("fs"),path=require("path"),os=require("os"),crypto=require("crypto"),assert=require("assert"),cp=require("child_process");
const root=path.resolve(__dirname,".."),base=fs.mkdtempSync(path.join(os.tmpdir(),"vault-browser-"));let dir=path.join(base,"離線 封存");fs.mkdirSync(dir);
const python=process.env.PYTHON||"python";
const env={...process.env,PYTHONPATH:path.join(root,"src")};
function demo(revision){cp.execFileSync(python,["-m","line_vault","demo",dir,"--revision",String(revision)],{env,stdio:"pipe"});}
(async()=>{let browser;
try{
 demo(1);browser=await chromium.launch({headless:true,...(process.env.CHROMIUM_EXECUTABLE?{executablePath:process.env.CHROMIUM_EXECUTABLE}:{} )});const context=await browser.newContext({acceptDownloads:true});const p=await context.newPage();
 const errors=[],external=[];p.on("pageerror",e=>errors.push(e.message));p.on("request",r=>{if(/^https?:|^wss?:/.test(r.url()))external.push(r.url());});
 await p.goto(require("url").pathToFileURL(path.join(dir,"index.html")).href);
 await p.locator("#view-reading").click();await p.locator(".card").first().waitFor();assert.equal(await p.locator(".card").count(),6);
 await p.locator(".filter-menu summary").click();await p.locator("#search").fill("記事本內文");await p.waitForFunction(()=>document.querySelectorAll(".card").length===1);
 await p.locator("#clear").click();await p.waitForFunction(()=>document.querySelectorAll(".card").length===6);
 await p.locator("#kind").selectOption("album");await p.waitForFunction(()=>document.querySelectorAll(".card").length===2);
 await p.getByRole("button",{name:"預覽",exact:true}).click();await p.waitForFunction(()=>document.querySelector("#preview-content img")?.naturalWidth===160);
 await p.locator("#close-preview").click();await p.locator("#clear").click();
 const link=p.getByRole("button",{name:"另存檔案",exact:true}).first();let checksum;
 for(let i=0;i<10;i++){
  const event=p.waitForEvent("download");await link.click();const d=await event;const b=fs.readFileSync(await d.path());const h=crypto.createHash("sha256").update(b).digest("hex");if(checksum)assert.equal(h,checksum);checksum=h;
 }
 await p.getByRole("button",{name:"查看引用訊息",exact:true}).first().click();assert(await p.evaluate(()=>document.activeElement.id.startsWith("item-")));
 await p.locator("#chats button").filter({hasText:"範例社群"}).click();await p.locator("#kind").selectOption("thread");await p.waitForFunction(()=>document.querySelectorAll(".card").length===2);
 await p.locator("#clear").click();await p.locator("#scope").selectOption("all");await p.waitForFunction(()=>document.querySelectorAll(".card").length===14);
 assert.equal(external.length,0);assert.deepEqual(errors,[]);
 demo(2);await p.reload();await p.locator("#view-reading").click();await p.locator(".card").first().waitFor();assert((await p.locator("#items").innerText()).includes("第二次更新"));
 const moved=path.join(base,"搬移後 封存");fs.renameSync(dir,moved);dir=moved;
 await p.goto(require("url").pathToFileURL(path.join(dir,"index.html")).href);await p.locator("#view-reading").click();await p.locator(".card").first().waitFor();
 const movedEvent=p.waitForEvent("download");await p.getByRole("button",{name:"另存檔案",exact:true}).first().click();
 const movedDownload=await movedEvent;assert.equal(crypto.createHash("sha256").update(fs.readFileSync(await movedDownload.path())).digest("hex"),checksum);
 const descriptor=await p.locator("#manifest").textContent();const manifest=JSON.parse(descriptor),file=Object.values(manifest.downloads).find(x=>x.sha256===checksum);
 assert(file);const part=path.join(dir,file.parts[0].path),original=fs.readFileSync(part);fs.writeFileSync(part,`window.VaultBytes.add("${file.sha256}",0,"YmFk");`);
 let badDownloads=0;const countBad=()=>badDownloads++;p.on("download",countBad);
 await p.getByRole("button",{name:"另存檔案",exact:true}).first().click();await p.waitForFunction(()=>document.querySelector("#download-notice").textContent.includes("無法完成另存"));assert.equal(badDownloads,0);p.off("download",countBad);fs.writeFileSync(part,original);
 const extra=JSON.parse(cp.execFileSync(python,["-c",`
import hashlib,json,sys
from pathlib import Path
from line_vault.store import Store
from line_vault.model import Item,Attachment
from line_vault.safeio import atomic_bytes
from line_vault.exporter import DOWNLOAD_CHUNK,export_html
result=[]
with Store(Path(sys.argv[1])) as store:
 cid=store.db.execute("SELECT id FROM chats WHERE source_id='example-project'").fetchone()[0]
 for name,payload in [('多段下載範例',bytes(range(256))*(DOWNLOAD_CHUNK*2//256)+b'END'),('空檔案範例',b'')]:
  sha=hashlib.sha256(payload).hexdigest();rel='assets/'+sha[:2]+'/'+sha+'.bin'
  atomic_bytes(store.root/rel,payload)
  with store.transaction():
   store.put_items(cid,[Item(name,'message',name)])
   aid=store.put_attachment(cid,Attachment(name,name,name+'.bin',expected_size=len(payload),expected_sha256=sha))
   store.save_blob(aid,rel,sha,len(payload))
  result.append({'name':name,'size':len(payload),'sha256':sha})
 export_html(store)
print(json.dumps(result))
`,dir],{env,encoding:"utf-8"}));
 await p.reload();await p.locator("#view-reading").click();await p.locator(".card").first().waitFor();
 for(const expected of extra){const next=p.waitForEvent("download");await p.locator("article").filter({has:p.locator("p.text",{hasText:expected.name})}).getByRole("button",{name:"另存檔案",exact:true}).click();const d=await next;const bytes=fs.readFileSync(await d.path());assert.equal(bytes.length,expected.size);assert.equal(crypto.createHash("sha256").update(bytes).digest("hex"),expected.sha256);}
 await p.setViewportSize({width:390,height:844});await p.screenshot({path:path.join(dir,"reader-mobile.png"),fullPage:true});
 assert(await p.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+2));
 assert.equal(external.length,0);assert.deepEqual(errors,[]);
 console.log("file:// reader: search, album preview, threads, parent navigation, no external requests, 10 identical downloads, latest-content reload, relocated folder, corrupted-part rejection, multichunk/empty downloads, mobile layout passed.");
}finally{if(browser)await browser.close();fs.rmSync(base,{recursive:true,force:true});}
})().catch(e=>{console.error(e);process.exitCode=1;});
