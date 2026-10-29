// Native-only packaging. No Python executable, module, or runtime is invoked.
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import {execFileSync} from 'node:child_process';
import {ROOT,sha,stagedFiles,review} from './check_public_tree.mjs';
import {writeZip,readZip} from './zip_archive.mjs';
import {lineCipherVersion,lineCipherSqliteVersion,lineCipherArchiveSha256,lineCipherFileHashes,lineCipherFlags} from './fetch_line_cipher.mjs';
const args=process.argv.slice(2);if(args.length!==2||args[0]!=='--output')throw Error('Usage: node tools/package_release.mjs --output directory');
const output=path.resolve(args[1]),source=stagedFiles(),findings=review(source);if(findings.length)throw Error('Public-source gate failed: '+JSON.stringify(findings));
const version=source.get('VERSION').toString('ascii').trim();if(!/^\d+\.\d+\.\d+-alpha\.\d+$/.test(version))throw Error('Only explicit alpha candidates are supported');
if(process.env.GITHUB_REF_TYPE==='tag'&&process.env.GITHUB_REF_NAME!=='v'+version)throw Error('Tag and VERSION mismatch');
function readRegular(p){if(!fs.lstatSync(p).isFile())throw Error('Not a regular file: '+path.basename(p));return fs.readFileSync(p);}
const binary=readRegular(path.join(ROOT,'native/out/ChatAtlas.exe')),provenanceBytes=readRegular(path.join(ROOT,'native/out/build-provenance.json')),provenance=JSON.parse(provenanceBytes);
if(binary.subarray(0,2).toString()!=='MZ'||provenance.version!==version||sha(binary)!==provenance.binary.sha256)throw Error('Binary/provenance mismatch');
for(const item of provenance.sourceInputs)if(!source.has(item.path)||sha(source.get(item.path))!==item.sha256)throw Error('Build differs from staged source: '+item.path);
const cipher=provenance.dependencies?.find(d=>d.name==="SQLite3MultipleCiphers");
if(!cipher||cipher.version!==lineCipherVersion||cipher.sqliteVersion!==lineCipherSqliteVersion||cipher.archiveSha256!==lineCipherArchiveSha256||cipher.linkage!=="static"||JSON.stringify(cipher.files)!==JSON.stringify(lineCipherFileHashes)||JSON.stringify(cipher.flags)!==JSON.stringify(lineCipherFlags))throw Error("Native cipher dependency provenance mismatch");
const noticeNames=['LICENSE','THIRD_PARTY_NOTICES.md',...source.keys()].filter((name,index,list)=>
  (name==='LICENSE'||name==='THIRD_PARTY_NOTICES.md'||/^licenses\/[A-Za-z0-9][A-Za-z0-9_.-]*\.txt$/.test(name)||['native/vendor/LICENSE-nlohmann.txt','native/vendor/LICENSE-hedley.txt'].includes(name))&&list.indexOf(name)===index).sort();
const noticeParts=[Buffer.from('ChatAtlas license and third-party notices\n\n')];
for(const name of noticeNames){if(!source.has(name))throw Error('Required notice source missing: '+name);noticeParts.push(Buffer.from('===== '+name+' =====\n'),source.get(name),Buffer.from('\n'));}
const notices=Buffer.concat(noticeParts),embedded=provenance.embeddedNotices;
if(!embedded||embedded.resourceId!==2||embedded.bytes!==notices.length||embedded.sha256!==sha(notices)||JSON.stringify(embedded.sourcePaths)!==JSON.stringify(noticeNames)||!binary.includes(notices))throw Error('Standalone EXE is missing its complete embedded license notices');
const files=new Map([['ChatAtlas/ChatAtlas.exe',binary],['ChatAtlas/BUILD_PROVENANCE.json',provenanceBytes]]);
for(const[name,bytes]of source)if(['LICENSE','SECURITY.md','THIRD_PARTY_NOTICES.md','VERSION'].includes(name)||name.startsWith('licenses/')||['native/vendor/LICENSE-nlohmann.txt','native/vendor/LICENSE-hedley.txt'].includes(name))files.set('ChatAtlas/'+name.replace('native/vendor/','licenses/'),bytes);
files.set('ChatAtlas/使用說明.txt',Buffer.from('請先解壓整包，再雙擊 ChatAtlas.exe。\nWindows x64 原生程式，不需 Python、Node.js 或另裝 .NET。\n設定、預設收藏與範例保存在 EXE 旁的 ChatAtlasData。請把整包放在可寫入的一般本機私人資料夾；不要從網路共用、網路磁碟或雲端同步位置執行。不會偷偷改存 AppData 或暫存資料夾。\n舊 AppData 資料不會自動搬移或刪除。手動另選收藏位置時，以選定位置為準。\n人工範例：開啟 人工範例/index.html，保留旁邊的資料夾。\n這是實驗版；真實 LINE 自動採集、Windows GUI 與多 GB 檔案尚未完成實機驗收。\nEXE 未簽章，請勿停用防毒或系統保護。沒有宣稱通過完整獨立安全審查。\n'));
const temporary=fs.mkdtempSync(path.join(os.tmpdir(),'chatatlas-package-synthetic-'));
try{
 const example=path.join(temporary,'archive'),cli=path.join(ROOT,'native/build/atlas-cli'+(process.platform==='win32'?'.exe':''));
 execFileSync(cli,['demo',example],{cwd:ROOT,stdio:'pipe'});execFileSync(cli,['check',example],{cwd:ROOT,stdio:'pipe'});
 const allow=/^(?:index\.html|assets\/[0-9a-f]{2}\/[0-9a-f]{64}\.[a-z0-9]+|downloads\/[0-9a-f]{64}\/(?:info\.json|[0-9]+\.js)|reader\/[0-9a-f]{32}\/[0-9a-f]{64}-[0-9]+\.js)$/;
 function walk(dir){for(const entry of fs.readdirSync(dir,{withFileTypes:true})){const p=path.join(dir,entry.name);if(entry.isSymbolicLink())throw Error('Synthetic output contains link');if(entry.isDirectory())walk(p);else if(entry.isFile()){const name=path.relative(example,p).split(path.sep).join('/');if(allow.test(name))files.set('ChatAtlas/人工範例/'+name,fs.readFileSync(p));}}}walk(example);
 if(!files.has('ChatAtlas/人工範例/index.html'))throw Error('Native synthetic reader missing');
}finally{fs.rmSync(temporary,{recursive:true,force:true});}
const sums=entries=>[...entries].sort(([a],[b])=>a<b?-1:1).map(([name,bytes])=>sha(bytes)+'  '+name+'\n').join('');
files.set('ChatAtlas/SHA256SUMS.txt',Buffer.from(sums(new Map([...files].map(([n,b])=>[n.slice(10),b])))));
const sourceFiles=new Map([...source].map(([name,bytes])=>['ChatAtlas-source/'+name,bytes]));sourceFiles.set('ChatAtlas-source/SOURCE_SHA256SUMS.txt',Buffer.from(sums(source)));
const sourceName='ChatAtlas-'+version+'-source.zip',binaryName='ChatAtlas-'+version+'-Windows-x64-candidate.zip',exeName='ChatAtlas-'+version+'-Windows-x64.exe',checklistName='ChatAtlas-'+version+'-CHECKLIST.txt';
const artifacts=new Map([[sourceName,writeZip(sourceFiles)],[binaryName,writeZip(files)],[exeName,binary],[checklistName,Buffer.from('ChatAtlas '+version+'\nNative EXE and synthetic example only; no end-user Python/Node/.NET installation.\nStandalone EXE is byte-identical to the ZIP executable and embeds the complete license notices.\nSource-index privacy gate and binary input hashes checked by packager.\nWindows runtime: consult exact-commit CI; packaging is not GUI or real LINE validation.\nAuthenticode signed: false. CFG: not enabled by current Zig linker.\nIndependent full security review: not claimed.\nBuild attestation: only valid when a separate GitHub attestation exists and verifies.\n')]]);
// Readback every member against exact intended bytes before any output is published.
for(const [name,expected]of [[sourceName,sourceFiles],[binaryName,files]]){const actual=readZip(artifacts.get(name));if(actual.size!==expected.size)throw Error('ZIP count mismatch');for(const[n,b]of expected)if(!actual.get(n)?.equals(b))throw Error('ZIP readback mismatch: '+n);}
if(!artifacts.get(exeName).equals(readZip(artifacts.get(binaryName)).get('ChatAtlas/ChatAtlas.exe'))||sha(artifacts.get(exeName))!==provenance.binary.sha256)throw Error('Standalone and ZIP executable bytes differ');
artifacts.set('SHA256SUMS.txt',Buffer.from(sums(artifacts)));
fs.mkdirSync(output,{recursive:true});if(fs.lstatSync(output).isSymbolicLink())throw Error('Output directory must not be a link');
for(const[name,bytes]of artifacts)fs.writeFileSync(path.join(output,name),bytes,{flag:'wx'});
console.log(JSON.stringify({version,files:[...artifacts].map(([name,b])=>({name,bytes:b.length,sha256:sha(b)})),sourceFiles:source.size,signed:false,securityCertification:false},null,2));
