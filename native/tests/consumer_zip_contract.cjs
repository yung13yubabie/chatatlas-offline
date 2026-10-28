// Inspect the native consumer ZIP, not the source ZIP.
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
 const crypto=require('node:crypto');assert.equal(crypto.createHash('sha256').update(files.get('ChatAtlas/ChatAtlas.exe')).digest('hex'),manifest.binary.sha256);
 const html=files.get('ChatAtlas/人工範例/index.html').toString('utf8');
 assert(html.includes("connect-src 'none'"));assert(html.includes("default-src 'none'"));
 assert(!/(?:src|href)\s*=\s*["'](?:https?:)?\/\//i.test(html),'External HTML dependency');
 assert(!/@import\s+(?:url\s*\()?\s*["']?(?:https?:)?\/\//i.test(html),'External CSS dependency');
 console.log('Native consumer ZIP has one EXE, no installer/runtime packages, matched binary hash and local-only HTML resource markup.');
})().catch(error=>{console.error(error.message);process.exitCode=1;});
