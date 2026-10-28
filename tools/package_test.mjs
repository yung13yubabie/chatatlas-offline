import assert from 'node:assert/strict';
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {ROOT,review} from './check_public_tree.mjs';
import {writeZip,readZip,crc32} from './zip_archive.mjs';
assert.equal(crc32(Buffer.from('123456789')),0xcbf43926);
const entries=new Map([['測試/one.txt',Buffer.from('synthetic only')],['zero',Buffer.alloc(0)]]),zip=writeZip(entries),read=readZip(zip);
for(const[n,b]of entries)assert.deepEqual(read.get(n),b);assert.deepEqual(zip,writeZip(entries));
for(const name of ['../x','/x','a/../b','a\\b','a//b'])assert.throws(()=>writeZip(new Map([[name,Buffer.from('x')]])));
const broken=Buffer.from(zip);broken[35]^=1;assert.throws(()=>readZip(broken));
assert(review(new Map([['private/raw.txt',Buffer.from('synthetic')]]),false).some(([,r])=>r==='directory not approved'));
for(const file of ['tools/package_release.mjs','tools/fetch_zig.mjs','native/build.mjs','tools/reader_test.cjs']){const text=fs.readFileSync(ROOT+'/'+file,'utf8');assert(!/(?:execFileSync|spawnSync|run)\s*\(\s*['"]python/.test(text));}
for(const wf of ['checks.yml','windows-build.yml','capture-release-ui.yml']){const workflow=fs.readFileSync(ROOT+'/.github/workflows/'+wf,'utf8');assert(!/setup-python|\bpython(?:3)?\b/.test(workflow));}
for(const wf of ['checks.yml','windows-build.yml'])for(const m of fs.readFileSync(ROOT+'/.github/workflows/'+wf,'utf8').matchAll(/uses:\s*([^\s]+)@([^\s]+)/g))assert.match(m[2],/^[0-9a-f]{40}$/);
const fetchSource=fs.readFileSync(ROOT+'/tools/fetch_zig.mjs','utf8');
assert.match(fetchSource,/execFileSync\('tar',\['-tf',archive\],\{encoding:'utf8',maxBuffer:16\*1024\*1024\}\)/);
const largeListing=execFileSync(process.execPath,['-e',"process.stdout.write('synthetic-entry\\n'.repeat(150000))"],{encoding:'utf8',maxBuffer:16*1024*1024});
assert(largeListing.length>1024*1024);


// The runner context belongs in step env, not job-level env.
function checkJobEnvContexts(workflow){
 let jobEnv=false;
 for(const line of workflow.split('\n')){
  if(/^    env:\s*$/.test(line)){jobEnv=true;continue;}
  if(jobEnv&&line.trim()&&line.search(/\S/)<=4)jobEnv=false;
  if(jobEnv)assert(!/\$\{\{\s*runner\./.test(line),'runner context must be in step env');
 }
}
for(const wf of ['checks.yml','windows-build.yml','capture-release-ui.yml'])checkJobEnvContexts(fs.readFileSync(ROOT+'/.github/workflows/'+wf,'utf8'));
assert.throws(()=>checkJobEnvContexts('    env:\n      QA: ${{ runner.temp }}\n'));

for(const name of fs.readdirSync(ROOT+'/docs/screenshots').filter(n=>n.endsWith('.png'))){const key='docs/screenshots/'+name,bytes=fs.readFileSync(ROOT+'/'+key);assert(!review(new Map([[key,bytes]]),false).some(([p])=>p===key));const corrupt=Buffer.from(bytes);corrupt[corrupt.length-1]^=1;assert(review(new Map([[key,corrupt]]),false).some(([p,r])=>p===key&&r==='unreviewed documentation image'));}
assert(review(new Map([['docs/screenshots/unreviewed.png',Buffer.from([137,80,78,71,13,10,26,10])]]),false).some(([p])=>p==='docs/screenshots/unreviewed.png'));



assert(review(new Map([['docs/ChatAtlasData/index.html',Buffer.from('artificial fixture')]]),false).some(([,r])=>r==='private or generated directory'));
console.log('Native package regression checks passed, including >1 MiB toolchain listing buffer.');

const readerSource=fs.readFileSync(ROOT+'/tools/reader_test.cjs','utf8');
assert(!/\bpython(?:3)?\b|PYTHONPATH|line_vault/.test(readerSource),'Reader regression must use native fixtures, not historical language runtime');
