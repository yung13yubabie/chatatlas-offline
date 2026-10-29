// Inspect the native consumer ZIP and the identical standalone release EXE.
const fs=require('node:fs'),assert=require('node:assert/strict'),path=require('node:path');
(async()=>{
 const {readZip}=await import('../../tools/zip_archive.mjs');
 if(process.argv.length!==3)throw Error('Usage: node native/tests/consumer_zip_contract.cjs Windows-candidate.zip');
 const files=readZip(fs.readFileSync(process.argv[2])),names=[...files.keys()];
 assert.deepEqual(names.filter(n=>/\.exe$/i.test(n)),['ChatAtlas/ChatAtlas.exe']);
 for(const name of names){
  assert(name.startsWith('ChatAtlas/'),'Unexpected consumer package root');
  assert(!/\.(?:py|pyc|pyo|pyd|ps1|cmd|bat|msi|dll)$/i.test(name),'Unexpected script, installer or runtime dependency: '+name);
  assert(!/(?:^|\/)(?:node_modules|\.git|\.venv|src|tools)(?:\/|$)/.test(name),'Developer files in consumer ZIP');
 }
 const manifest=JSON.parse(files.get('ChatAtlas/BUILD_PROVENANCE.json'));
 assert.match(manifest.version,/^\d+\.\d+\.\d+-alpha\.\d+$/,'Unexpected release version');
 const crypto=require('node:crypto'),sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex'),binary=files.get('ChatAtlas/ChatAtlas.exe');assert.equal(sha(binary),manifest.binary.sha256);
 const noticeNames=['LICENSE','THIRD_PARTY_NOTICES.md',...names.filter(name=>/^ChatAtlas\/licenses\/[A-Za-z0-9][A-Za-z0-9_.-]*\.txt$/.test(name)).map(name=>{
  const base=path.posix.basename(name);return ['LICENSE-nlohmann.txt','LICENSE-hedley.txt'].includes(base)?'native/vendor/'+base:'licenses/'+base;
 })].sort();
 const parts=[Buffer.from('ChatAtlas license and third-party notices\n\n')];
 for(const name of noticeNames){const packed='ChatAtlas/'+name.replace('native/vendor/','licenses/');assert(files.has(packed),'Missing required notice: '+packed);parts.push(Buffer.from('===== '+name+' =====\n'),files.get(packed),Buffer.from('\n'));}
 const notices=Buffer.concat(parts);assert.deepEqual(manifest.embeddedNotices,{resourceId:2,bytes:notices.length,sha256:sha(notices),sourcePaths:noticeNames},'Embedded license provenance differs');assert(binary.includes(notices),'Standalone EXE must include every full license notice');
 const exeName='ChatAtlas-'+manifest.version+'-Windows-x64.exe',raw=fs.readFileSync(path.join(path.dirname(process.argv[2]),exeName));assert(raw.equals(binary),'Standalone release EXE differs from ZIP executable');
 const checksumLines=fs.readFileSync(path.join(path.dirname(process.argv[2]),'SHA256SUMS.txt'),'utf8').trimEnd().split('\n');
 for(const [name,bytes]of [[exeName,raw],[path.basename(process.argv[2]),fs.readFileSync(process.argv[2])]]){const expected=sha(bytes)+'  '+name;assert.equal(checksumLines.filter(line=>line===expected).length,1,'Release checksum missing or duplicated: '+name);assert.equal(checksumLines.filter(line=>line.endsWith('  '+name)).length,1,'Ambiguous release checksum: '+name);}
 const html=files.get('ChatAtlas/人工範例/index.html').toString('utf8');
 assert(html.includes("connect-src 'none'"));assert(html.includes("default-src 'none'"));
 assert(!/(?:src|href)\s*=\s*["'](?:https?:)?\/\//i.test(html),'External HTML dependency');
 assert(!/@import\s+(?:url\s*\()?\s*["']?(?:https?:)?\/\//i.test(html),'External CSS dependency');
 console.log('Native consumer ZIP and standalone EXE match, contain complete embedded license notices, have verified release checksums and need no runtime packages.');
})().catch(error=>{console.error(error.message);process.exitCode=1;});
