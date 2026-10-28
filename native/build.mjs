import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';

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
fs.mkdirSync(path.join(root,'out'),{recursive:true});
run(process.execPath,['bundle.mjs']);
run(zig,['rc','/i','src','/i',path.join(compiler.lib_dir,'libc/include/any-windows-any'),'/fo','out/app.res','src/app.rc']);
const sources=['app','platform','database','store','archive','zip','demo','bundle','windows_source','import'].map(name=>'src/'+name+'.cpp');
run(zig,['c++','-target','x86_64-windows-gnu','-std=c++20','-O1','-g0','-fstack-protector-strong','-DUNICODE','-D_UNICODE','-municode','-Wl,--subsystem,windows','-Isrc','-Iinclude','-Ivendor',...sources,'out/app.res','-lcomctl32','-lole32','-lshell32','-luuid','-lcomdlg32','-lgdi32','-ladvapi32','-o','out/ChatAtlas.exe']);

const hash=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
function regularFiles(directory) {
  return fs.readdirSync(directory,{withFileTypes:true}).flatMap(entry=>{
    const p=path.join(directory,entry.name);
    if(entry.isSymbolicLink()) throw new Error('Build inputs must not be symbolic links');
    return entry.isDirectory()?regularFiles(p):[p];
  });
}
const inputs=['src','include','vendor'].flatMap(name=>regularFiles(path.join(root,name)));
inputs.push(...regularFiles(path.join(repo,'src/line_vault/web')),
  path.join(repo,'src/line_vault/windows/uia_worker.ps1'),
  path.join(repo,'VERSION'),path.join(root,'build.mjs'),path.join(root,'bundle.mjs'));
const sourceInputs=inputs.map(p=>({path:path.relative(repo,p).split(path.sep).join('/'),sha256:hash(fs.readFileSync(p))})).sort((a,b)=>a.path.localeCompare(b.path,'en'));
const manifest={schema:1,version:fs.readFileSync(path.join(repo,'VERSION'),'utf8').trim(),
  compiler:{name:'Zig',version:pinnedVersion,target:'x86_64-windows-gnu'},
  hardening:{stackProtector:'strong',controlFlowGuard:false,authenticode:false},
  binary:{name:'ChatAtlas.exe',sha256:hash(fs.readFileSync(path.join(root,'out/ChatAtlas.exe')))},
  sourceInputs,validation:{windowsRuntime:'not-tested',lineCapture:'not-validated',signed:false}};
fs.writeFileSync(path.join(root,'out/build-provenance.json'),JSON.stringify(manifest,null,2)+'\n');
console.log('Built out/ChatAtlas.exe and source-hash provenance. Windows runtime and LINE capture remain unverified.');
