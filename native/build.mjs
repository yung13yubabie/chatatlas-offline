import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import {ensureLineCipher,lineCipherFlags} from '../tools/fetch_line_cipher.mjs';

const root=path.dirname(fileURLToPath(import.meta.url));
const repo=path.dirname(root), zig=process.env.ATLAS_ZIG||'zig';
const pinnedVersion='0.14.1';
function run(tool,args,options={}) {
  const result=spawnSync(tool,args,{cwd:root,stdio:'inherit',...options});
  if(result.error) throw result.error;
  if(result.status!==0) throw new Error(path.basename(tool)+' failed ('+result.status+')');
  return result;
}
const version=run(zig,['version'],{encoding:'utf8',stdio:'pipe'}).stdout.trim();
if(version!==pinnedVersion) throw new Error('Use Zig '+pinnedVersion+'; found '+version);
const compiler=JSON.parse(run(zig,['env'],{encoding:'utf8',stdio:'pipe'}).stdout);
const hash=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
fs.mkdirSync(path.join(root,'out'),{recursive:true});
run(process.execPath,['bundle.mjs']);
const noticeNames=['LICENSE','THIRD_PARTY_NOTICES.md',
  ...fs.readdirSync(path.join(repo,'licenses')).filter(name=>/^[A-Za-z0-9][A-Za-z0-9_.-]*\.txt$/.test(name)).map(name=>'licenses/'+name),
  'native/vendor/LICENSE-nlohmann.txt','native/vendor/LICENSE-hedley.txt'].sort();
const noticeParts=[Buffer.from('ChatAtlas license and third-party notices\n\n')];
for(const name of noticeNames){
  const file=path.join(repo,name);if(!fs.lstatSync(file).isFile()||fs.lstatSync(file).isSymbolicLink())throw Error('Notice input must be a regular source file');
  noticeParts.push(Buffer.from('===== '+name+' =====\n'),fs.readFileSync(file),Buffer.from('\n'));
}
const notices=Buffer.concat(noticeParts);
fs.writeFileSync(path.join(root,'out/THIRD_PARTY_NOTICES.txt'),notices);
const inputs=['src','include','vendor'].flatMap(name=>regularFiles(path.join(root,name)));
inputs.push(...regularFiles(path.join(repo,'src/line_vault/web')),
  path.join(repo,'src/line_vault/windows/uia_worker.ps1'),
  path.join(repo,'VERSION'),path.join(root,'build.mjs'),path.join(root,'bundle.mjs'),path.join(repo,'tools/fetch_line_cipher.mjs'));
for(const name of noticeNames){const file=path.join(repo,name);if(!inputs.includes(file))inputs.push(file);}
const initialInputs=inputs.map(p=>({path:path.relative(repo,p).split(path.sep).join('/'),sha256:hash(fs.readFileSync(p))})).sort((a,b)=>a.path.localeCompare(b.path,'en'));
run(zig,['rc','/i','src','/i',path.join(compiler.lib_dir,'libc/include/any-windows-any'),'/fo','out/app.res','src/app.rc']);
const dependency=await ensureLineCipher();
run(zig,['cc','-target','x86_64-windows-gnu','-std=c11','-O1',...lineCipherFlags,'-c',dependency.source,'-o','out/line-cipher.o']);
const sources=['app','platform','database','store','archive','zip','demo','bundle','windows_source','import','line_cipher','line_process','line_database','line_media','line_obs'].map(name=>'src/'+name+'.cpp');
run(zig,['c++','-target','x86_64-windows-gnu','-std=c++20','-O1','-g0','-fstack-protector-strong','-DUNICODE','-D_UNICODE','-municode','-Wl,--subsystem,windows','-Isrc','-Iinclude','-Ivendor','-I'+dependency.directory,...sources,'out/line-cipher.o','out/app.res','-lcomctl32','-lole32','-lshell32','-luuid','-lcomdlg32','-lgdi32','-ladvapi32','-lwintrust','-lcrypt32','-lwinhttp','-lbcrypt','-o','out/ChatAtlas.exe']);

function regularFiles(directory) {
  return fs.readdirSync(directory,{withFileTypes:true}).flatMap(entry=>{
    const p=path.join(directory,entry.name);
    if(entry.isSymbolicLink()) throw new Error('Build inputs must not be symbolic links');
    return entry.isDirectory()?regularFiles(p):[p];
  });
}
const sourceInputs=inputs.map(p=>({path:path.relative(repo,p).split(path.sep).join('/'),sha256:hash(fs.readFileSync(p))})).sort((a,b)=>a.path.localeCompare(b.path,'en'));
if(JSON.stringify(initialInputs)!==JSON.stringify(sourceInputs))throw Error("Build inputs changed during compilation; rerun after edits finish");
await ensureLineCipher(); // Recheck all pinned C dependency bytes after compilation.
const manifest={schema:1,version:fs.readFileSync(path.join(repo,'VERSION'),'utf8').trim(),
  compiler:{name:'Zig',version:pinnedVersion,target:'x86_64-windows-gnu'},
  hardening:{stackProtector:'strong',controlFlowGuard:false,authenticode:false},
  binary:{name:'ChatAtlas.exe',sha256:hash(fs.readFileSync(path.join(root,'out/ChatAtlas.exe')))},
  embeddedNotices:{resourceId:2,bytes:notices.length,sha256:hash(notices),sourcePaths:noticeNames},
  sourceInputs,dependencies:[{name:"SQLite3MultipleCiphers",version:dependency.version,sqliteVersion:dependency.sqliteVersion,archiveSha256:dependency.archiveSha256,files:dependency.files,flags:lineCipherFlags,linkage:"static"}],validation:{windowsRuntime:'not-tested',lineCapture:'not-validated',signed:false}};
fs.writeFileSync(path.join(root,'out/build-provenance.json'),JSON.stringify(manifest,null,2)+'\n');
console.log('Built out/ChatAtlas.exe and source-hash provenance. Windows runtime and LINE capture remain unverified.');
