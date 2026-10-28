// Real Chromium file:// tests. No --disable-web-security or local HTTP server.
const {chromium}=require("playwright");
const fs=require("fs"),path=require("path"),os=require("os"),crypto=require("crypto"),assert=require("assert"),cp=require("child_process");
const root=path.resolve(__dirname,".."),base=fs.mkdtempSync(path.join(process.env.RUNNER_TEMP||os.tmpdir(),"vault-browser-"));let dir=path.join(base,"離線 封存");fs.mkdirSync(dir);
const suffix=process.platform==="win32"?".exe":"",nativeCli=process.env.ATLAS_NATIVE_CLI||path.join(root,"native","build","atlas-cli"+suffix),readerFixture=process.env.ATLAS_READER_FIXTURE||path.join(root,"native","build","reader-fixture"+suffix);
function demo(revision){cp.execFileSync(nativeCli,["demo",dir,String(revision)],{stdio:"pipe"});}
async function filters(p){if(await p.locator(".filter-menu").getAttribute("open")===null)await p.locator(".filter-menu summary").click();}
async function project(p){await p.locator("#chats button").filter({hasText:"山徑企劃小組"}).click();await p.waitForFunction(()=>document.querySelector("#title").textContent==="山徑企劃小組");}
(async()=>{let browser;
try{
 demo(1);browser=await chromium.launch({headless:true,...(process.env.CHROMIUM_EXECUTABLE?{executablePath:process.env.CHROMIUM_EXECUTABLE}:{} )});const context=await browser.newContext({acceptDownloads:true});const p=await context.newPage();
 const errors=[],external=[];p.on("pageerror",e=>errors.push(e.message));p.on("request",r=>{if(/^https?:|^wss?:/.test(r.url()))external.push(r.url());});
 await p.goto(require("url").pathToFileURL(path.join(dir,"index.html")).href);
 await project(p);await p.locator("#view-reading").click();await p.waitForFunction(()=>document.querySelectorAll(".card").length===10);assert.equal(await p.locator(".card").count(),10);
 await filters(p);await p.locator("#search").fill("行前準備與補給備忘");await p.waitForFunction(()=>document.querySelectorAll(".card").length===1);
 await filters(p);await p.locator("#clear").click();await p.waitForFunction(()=>document.querySelectorAll(".card").length===10);
 await filters(p);await p.locator("#kind").selectOption("album");await p.waitForFunction(()=>document.querySelectorAll(".card").length===2);
 await p.keyboard.press("Escape");await p.getByRole("button",{name:"預覽",exact:true}).click();await p.waitForFunction(()=>document.querySelector("#preview-content img")?.naturalWidth===160);
 await p.locator("#close-preview").click();await filters(p);await p.locator("#clear").click();
 await p.keyboard.press("Escape");const link=p.getByRole("button",{name:"另存檔案",exact:true}).first();let checksum;
 for(let i=0;i<10;i++){
  const event=p.waitForEvent("download");await link.click();const d=await event;const b=fs.readFileSync(await d.path());const h=crypto.createHash("sha256").update(b).digest("hex");if(checksum)assert.equal(h,checksum);checksum=h;
 }
 await p.getByRole("button",{name:"查看引用訊息",exact:true}).first().click();assert(await p.evaluate(()=>document.activeElement.id.startsWith("item-")));
 await filters(p);await p.locator("#kind").selectOption("thread");await p.waitForFunction(()=>document.querySelectorAll(".card").length===2);
 await filters(p);await p.locator("#clear").click();await p.locator("#chats button").filter({hasText:"週末山行社群"}).click();await p.waitForFunction(()=>document.querySelectorAll(".card").length===4);
 await filters(p);await p.locator("#clear").click();await filters(p);await p.locator("#scope").selectOption("all");await p.waitForFunction(()=>document.querySelectorAll(".card").length===19);
 assert.equal(external.length,0);assert.deepEqual(errors,[]);
 demo(2);await p.reload();await project(p);await p.locator("#view-reading").click();await p.locator(".card").first().waitFor();assert((await p.locator("#items").innerText()).includes("已更新集合安排"));
 const moved=path.join(base,"搬移後 封存");fs.renameSync(dir,moved);dir=moved;
 await p.goto(require("url").pathToFileURL(path.join(dir,"index.html")).href);await project(p);await p.locator("#view-reading").click();await p.locator(".card").first().waitFor();
 const movedEvent=p.waitForEvent("download");await p.getByRole("button",{name:"另存檔案",exact:true}).first().click();
 const movedDownload=await movedEvent;assert.equal(crypto.createHash("sha256").update(fs.readFileSync(await movedDownload.path())).digest("hex"),checksum);
 const descriptor=await p.locator("#manifest").textContent();const manifest=JSON.parse(descriptor),file=Object.values(manifest.downloads).find(x=>x.sha256===checksum);
 assert(file);const part=path.join(dir,file.parts[0].path),original=fs.readFileSync(part);fs.writeFileSync(part,`window.VaultBytes.add("${file.sha256}",0,"YmFk");`);
 await p.reload();await project(p);await p.locator("#view-reading").click();await p.locator(".card").first().waitFor();
 let badDownloads=0;const countBad=()=>badDownloads++;p.on("download",countBad);
 await p.getByRole("button",{name:"另存檔案",exact:true}).first().click();await p.waitForFunction(()=>document.querySelector("#download-notice").textContent.includes("無法完成另存"));assert.equal(badDownloads,0);p.off("download",countBad);fs.writeFileSync(part,original);
 const extra=JSON.parse(cp.execFileSync(readerFixture,[dir],{encoding:"utf-8"}));
 assert.equal(extra.length,3);assert(extra.find(x=>x.name==="多段下載範例").parts>=3);assert.equal(extra.find(x=>x.name==="空檔案範例").size,0);
 await p.reload();await project(p);await p.locator("#view-reading").click();await p.locator(".card").first().waitFor();
 for(const expected of extra){const next=p.waitForEvent("download");const card=p.locator("#item-"+expected.item_id);if(expected.content_omitted){assert.equal(await card.locator("p.text").textContent(),"未保存對話文字");assert(!(await card.innerText()).includes("此訊息已收回"));assert(!(await card.innerText()).includes("此內容已刪除"));assert((await card.locator(".attachment-reason").innerText()).includes("人工來源暫時無法提供"));}await card.getByRole("button",{name:"另存檔案",exact:true}).click();const d=await next;const bytes=fs.readFileSync(await d.path());assert.equal(bytes.length,expected.size);assert.equal(crypto.createHash("sha256").update(bytes).digest("hex"),expected.sha256);}
 await p.setViewportSize({width:390,height:844});await p.screenshot({path:path.join(dir,"reader-mobile.png"),fullPage:true});
 assert(await p.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth+2));
 // Each synthetic case preserves the page's exact inline script/CSP; only JSON metadata changes.
 const htmlPath=path.join(dir,'index.html'),originalPage=fs.readFileSync(htmlPath,'utf8'),casePath=path.join(dir,'capture-state-case.html');
 const jsonMarker=/<script type="application\/json" id="manifest">([\s\S]*?)<\/script>/;
 const match=originalPage.match(jsonMarker);assert(match);const baseManifest=JSON.parse(match[1]);assert(baseManifest.artificial_data);
 try{for(const [status,coverage,expected]of [['complete','visible_only','僅限當時可讀範圍'],['interrupted','unknown','上次整理曾中斷'],['complete_with_issues','source_range','上次整理有待補項目']]){
  const nextManifest=JSON.parse(JSON.stringify(baseManifest));for(const chat of nextManifest.chats)chat.last_job={status,coverage,finished_at:'2026-10-28T00:00:00Z',counts:{downloaded:2,reused:3,failed:1,unavailable:0}};
  const json=JSON.stringify(nextManifest).replace(/</g,'\\u003c');fs.writeFileSync(casePath,originalPage.replace(jsonMarker,()=>'<script type="application/json" id="manifest">'+json+'</script>'));
  await p.goto(require('url').pathToFileURL(casePath).href);await p.waitForFunction(()=>document.querySelector('#content').getAttribute('aria-busy')==='false');assert((await p.locator('#coverage').innerText()).includes(expected));assert((await p.locator('#coverage').innerText()).includes('沿用附件 3'));
  if(status!=='complete')assert((await p.locator('#notice').innerText()).includes('回桌面程式再次整理／補存'));
 }}finally{fs.rmSync(casePath,{force:true});}
 assert.equal(external.length,0);assert.deepEqual(errors,[]);
 console.log("file:// reader: search, album preview, threads, parent navigation, no external requests, 10 identical downloads, latest-content reload, relocated folder, corrupted-part rejection, multichunk/empty downloads, omitted-text attachment label/download, mobile layout passed.");
}finally{if(browser)await browser.close();fs.rmSync(base,{recursive:true,force:true});}
})().catch(e=>{console.error(e);process.exitCode=1;});
