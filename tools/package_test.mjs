import assert from 'node:assert/strict';
import fs from 'node:fs';
import {ROOT,review} from './check_public_tree.mjs';
import {writeZip,readZip,crc32} from './zip_archive.mjs';
assert.equal(crc32(Buffer.from('123456789')),0xcbf43926);
const entries=new Map([['測試/one.txt',Buffer.from('synthetic only')],['zero',Buffer.alloc(0)]]),zip=writeZip(entries),read=readZip(zip);
for(const[n,b]of entries)assert.deepEqual(read.get(n),b);assert.deepEqual(zip,writeZip(entries));
for(const name of ['../x','/x','a/../b','a\\b','a//b'])assert.throws(()=>writeZip(new Map([[name,Buffer.from('x')]])));
const broken=Buffer.from(zip);broken[35]^=1;assert.throws(()=>readZip(broken));
assert(review(new Map([['private/raw.txt',Buffer.from('synthetic')]]),false).some(([,r])=>r==='directory not approved'));
for(const file of ['tools/package_release.mjs','tools/fetch_zig.mjs','native/build.mjs']){const text=fs.readFileSync(ROOT+'/'+file,'utf8');assert(!/(?:execFileSync|spawnSync|run)\s*\(\s*['"]python/.test(text));}
for(const wf of ['checks.yml','windows-build.yml']){const workflow=fs.readFileSync(ROOT+'/.github/workflows/'+wf,'utf8');assert(!/setup-python|\bpython(?:3)?\b|reader_test\.cjs/.test(workflow));}
for(const wf of ['checks.yml','windows-build.yml'])for(const m of fs.readFileSync(ROOT+'/.github/workflows/'+wf,'utf8').matchAll(/uses:\s*([^\s]+)@([^\s]+)/g))assert.match(m[2],/^[0-9a-f]{40}$/);
console.log('Native package regression checks passed.');
