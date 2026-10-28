// Development-only, checksum-pinned official toolchain. No app runtime downloads.
import fs from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {ROOT,sha} from './check_public_tree.mjs';
const windows=process.platform==='win32';if(!windows&&process.platform!=='linux')throw Error('Pinned fetch supports Linux/Windows x64 only');if(process.arch!=='x64')throw Error('Pinned fetch requires x64');
const name='zig-x86_64-'+(windows?'windows':'linux')+'-0.14.1',extension=windows?'.zip':'.tar.xz';
const expected=windows?'554f5378228923ffd558eac35e21af020c73789d87afeabf4bfd16f2e6feed2c':'24aeeec8af16c381934a6cd7d95c807a8cb2cf7df9fa40d359aa884195c4716c';
const root=path.join(ROOT,'.toolchains'),archive=path.join(root,name+extension);
fs.mkdirSync(root,{recursive:true});if(fs.lstatSync(root).isSymbolicLink())throw Error('Toolchain directory must not be a link');
const response=await fetch('https://ziglang.org/download/0.14.1/'+name+extension,{signal:AbortSignal.timeout(120000)});if(!response.ok)throw Error('Official toolchain download failed: '+response.status);
let size=0;const chunks=[];for await(const chunk of response.body){size+=chunk.length;if(size>100*1024*1024)throw Error('Toolchain size limit exceeded');chunks.push(chunk);}
const bytes=Buffer.concat(chunks);if(sha(bytes)!==expected)throw Error('Toolchain checksum mismatch; nothing extracted');fs.writeFileSync(archive,bytes,{flag:'wx'});
// Exact upstream bytes authenticated by pinned digest before extraction. Windows bsdtar handles ZIP.
const entries=execFileSync('tar',['-tf',archive],{encoding:'utf8',maxBuffer:16*1024*1024}).trim().split(/\r?\n/);
if(entries.some(p=>!p.startsWith(name+'/')||p.split('/').includes('..')||path.posix.isAbsolute(p)))throw Error('Unexpected toolchain archive path');
execFileSync('tar',['-xf',archive,'-C',root,...(windows?[]:['--no-same-owner'])],{stdio:'inherit'});
console.log(path.join(root,name,windows?'zig.exe':'zig'));
