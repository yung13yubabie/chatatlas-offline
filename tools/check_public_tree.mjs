// Review exact staged public bytes. Report paths, never suspected secret contents.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
export const ROOT=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const suffix=new Set('.py .ps1 .cmd .js .cjs .css .html .md .toml .yml .yaml .cpp .hpp .h .mjs .rc .manifest .txt'.split(' '));
const names=new Set(['LICENSE','VERSION','.gitignore','.gitattributes','Makefile']);
const json=new Set(['package.json','package-lock.json']);
const tops=new Set(['.github','docs','licenses','native','src','tests','tools']);
const privateParts=new Set('private archives captures profiles.local output dist build out node_modules .venv .venv-build .toolchains ChatAtlasData Collections Example'.split(' '));
const patterns=[/-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----/i,/gh[pousr]_[A-Za-z0-9]{30,}/i,/github_pat_[A-Za-z0-9_]{30,}/i,/sk-[A-Za-z0-9_-]{24,}/i,/[A-Z0-9._%+-]+@(?!example\.(?:com|invalid)\b|users\.noreply\.github\.com\b)[A-Z0-9.-]+\.[A-Z]{2,}/i,/[A-Z]:\\Users\\[^\\\s]+\\/i,/\/(?:Users|home|workspace)\/[A-Za-z0-9_.-]+\//i,/AKIA[0-9A-Z]{16}/i,/xox[baprs]-[A-Za-z0-9-]{20,}/i,/AIza[0-9A-Za-z_-]{35}/i];
export const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
const copyrightHashes={
    "licenses/nlohmann-headers-notices.txt": "8df2f05fb2c7466803b046b37f729073c1843674ffe2383c61f07e0c60bbcde7",
    "native/vendor/nlohmann/detail/output/serializer.hpp": "4c079acb8f6936c4f787fd5706a52c77a1751dd34787a821705845698ec8540e",
    "native/vendor/nlohmann/thirdparty/hedley/hedley.hpp": "1547b367895f8be9f8283ae21f583182db572ad990236220002b632a15238712",
    "licenses/LLVM-libcxx.txt": "539dd7aed86e8a4f12cbdd0e6c50c189c7d74847e4fecc64ce2c6ee3a01da38b",
    "licenses/LLVM-libcxxabi.txt": "e2b35be49f7284a45b7baca8fc7b3ab7440e7902392b2528a457816b5bb2a15c",
    "licenses/LLVM-libunwind.txt": "b5efebcaca80879234098e52d1725e6d9eb8fb96a19fce625d39184b705f7b6d",
    "licenses/MinGW-w64-COPYING.txt": "99a69660981156c21336fdb5661f89341b013c94e4bf9e1c7467b4745718397f",
    "licenses/Runtime-source-notices.txt": "aab1e1465b8d2e2a71bc5d44fd4254801c7cd456be0db56f247ca73de8540212",
    "licenses/Zig-MIT.txt": "5c537d6853e005298a285d508cff9ac7192cea23576c840d485b2b586a7ff177",
};
// Reviewed artificial-data screenshots only; arbitrary binary documentation is rejected.
const documentationImages={
  "docs/screenshots/windows-native-alpha5.png": "d3c967a51a12ff03b80a22c3474b7fcea9e70e5ff7b2eb3df96ef2d946a9850b",
  "docs/screenshots/windows-history-alpha5.png": "c3f53bbb2b3c710d79b74c9ac3c321207277c63e4d82f5c315261a222f7be7e3",
  "docs/screenshots/offline-map-alpha5.png": "89cd1029e8c7ab29b22aca2735c576e69d695c7f8760ca5c23fda36998348ca4",
  "docs/screenshots/offline-reading-alpha5.png": "4a04699a6db2ddd2600b97d4077d4c5d9d5268118bb0374c12b15cb4a9e654bf",
  "docs/screenshots/offline-mobile-alpha5.png": "3d52c42f50e42b8f537a16467d573bbf3ddbab44ef2218339aad76bb1f447dd9",
  "docs/screenshots/windows-native-alpha4.png": "ad50d0f54be30e6f3ecd1167f8465601fe7a3584441c957d09118173fe737cfc",
  "docs/screenshots/offline-map-alpha4.png": "7cd8d5ac3d914be51a90b716e68ee2f10e92353791f6c0607780068ace660906",
  "docs/screenshots/offline-reading-alpha4.png": "c1e0299c9468c537c28c8929f565df09c2b7ee2b003fbc1bf3887b02caaf9ae0",
  "docs/screenshots/offline-mobile-alpha4.png": "50e01b27f56f67ace968db52590332f0e1dcb19dbfec88580cde74a5dd71745b",
  "docs/screenshots/offline-map-alpha2.png": "2919f0919eb48c0d7e36fbd5a90c6c90f9fd17268287b1ed5cf5c01448769710",
  "docs/screenshots/offline-map-alpha3.png": "3c98dfe51a17d7f32ee381183ebb01760ff81773340769596580518a8f93bd26",
  "docs/screenshots/offline-mobile-alpha2.png": "29908ce9faa25e432e4395ebb28b89071e4283495e30832dc71b0a52937317fc",
  "docs/screenshots/offline-mobile-alpha3.png": "21784e31851a1a4cb70955569d2089d1920055f242a170367105d6c79455b98f",
  "docs/screenshots/offline-reading-alpha2.png": "c1e0299c9468c537c28c8929f565df09c2b7ee2b003fbc1bf3887b02caaf9ae0",
  "docs/screenshots/offline-reading-alpha3.png": "0cc96610ea1ed329d2681605cdd857ca64c417435e37004741a550ee984429fd",
  "docs/screenshots/windows-native-alpha2.png": "c470da9caaedc8efa3b4339de9bd83ad55d8761d71e0a50a0bffe29c5cfba181",
  "docs/screenshots/windows-native-alpha3.png": "3310830183314ff8e90ae4999362e0bb6ed0591a2049651023d999b4bd4bcb72"
};
export function stagedFiles(){
 const result=new Map();
 for(const row of execFileSync('git',['ls-files','--stage','-z'],{cwd:ROOT}).toString('utf8').split('\0').filter(Boolean)){
  const tab=row.indexOf('\t'),[mode,,stage]=row.slice(0,tab).split(' '),name=row.slice(tab+1);
  if(!['100644','100755'].includes(mode)||stage!=='0')throw Error('Non-regular or unmerged staged file: '+name);
  result.set(name,execFileSync('git',['show',':'+name],{cwd:ROOT,maxBuffer:2*1024*1024}));
 }return result;
}
export function review(files,requireWorktree=true){
 const findings=[];
 for(const [name,bytes]of files){
  const parts=name.split('/'),base=parts.at(-1),fail=reason=>findings.push([name,reason]);
  if(path.posix.isAbsolute(name)||parts.includes('..')||name.includes('\\')){fail('unsafe path');continue;}
  if(parts.length>1&&!tops.has(parts[0])){fail('directory not approved');continue;}
  if(parts.some(p=>p.startsWith('.')&&!['.github','.gitignore','.gitattributes'].includes(p))){fail('hidden file or directory');continue;}
  if(parts.some(p=>privateParts.has(p))&&!name.startsWith('native/vendor/nlohmann/detail/output/')){fail('private or generated directory');continue;}
  if(requireWorktree){const p=path.join(ROOT,name);try{if(!fs.lstatSync(p).isFile()||!fs.readFileSync(p).equals(bytes)){fail('working bytes differ from staged bytes');continue;}}catch{fail('working source missing');continue;}}
  if(Object.hasOwn(documentationImages,name)){if(sha(bytes)!==documentationImages[name]||!bytes.subarray(0,8).equals(Buffer.from([137,80,78,71,13,10,26,10]))||bytes.length>4*1024*1024)fail('unreviewed documentation image');continue;}
  if(name==='native/src/app.ico'){if(!bytes.subarray(0,6).equals(Buffer.from([0,0,1,0,1,0]))||bytes.length>65536)fail('unexpected icon');continue;}
  if(!names.has(base)&&!json.has(name)&&!suffix.has(path.posix.extname(name).toLowerCase())){fail('file type not approved');continue;}
  if(base.toLowerCase().includes('local')||bytes.length>1024*1024){fail('local configuration or oversized file');continue;}
  let text;try{text=new TextDecoder('utf-8',{fatal:true}).decode(bytes);}catch{fail('not UTF-8 source');continue;}
  if(text.includes('\0')){fail('embedded NUL');continue;}
  if(patterns.some((p,i)=>!(i===4&&copyrightHashes[name]===sha(bytes))&&p.test(text)))fail('possible private data; inspect locally');
 }
 for(const name of ['LICENSE','VERSION','README.md','SECURITY.md','native/build.mjs','native/vendor/nlohmann/detail/output/binary_writer.hpp','native/vendor/nlohmann/detail/output/output_adapters.hpp','native/vendor/nlohmann/detail/output/serializer.hpp'])if(!files.has(name))findings.push([name,'required public source missing']);
 if(!files.size)findings.push(['','no staged source files']);return findings;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
 const files=stagedFiles(),findings=review(files);for(const [name,reason]of findings)console.error(name+': '+reason);console.log(`Reviewed ${files.size} staged files; ${findings.length} findings. Contents were not printed.`);process.exitCode=Number(findings.length>0);
}
