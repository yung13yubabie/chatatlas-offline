// Scoped release regression: no installer hooks and only Windows system imports.
// This is not a proof that every possible execution has no network activity.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const sources=fs.readdirSync(path.join(root,'native/src')).filter(n=>/\.(?:cpp|hpp)$/.test(n)&&n!=='bundle.cpp').map(n=>[n,fs.readFileSync(path.join(root,'native/src',n),'utf8')]);
const all=sources.map(([,s])=>s).join('\n');
for(const re of [/\b(?:URLDownloadToFile|InternetOpen|WSAStartup)\w*\s*\(/,/\b(?:winget|choco|pip|npm)\s+(?:install|add)\b/i,/\b(?:msiexec|curl\.exe|wget\.exe|python\.exe|node\.exe)\b/i])assert(!re.test(all),'Unexpected runtime installer/network hook: '+re);
assert.equal((all.match(/\bCreateProcessW\s*\(/g)||[]).length,1,'Review any new child-process launch');
for(const [name,source]of sources)if(name!=='line_media.cpp'&&name!=='line_obs.cpp')assert(!/\bWinHttp\w*\s*\(/.test(source),'Network access is limited to explicit LINE media retrieval');
const media=sources.find(([n])=>n==='line_media.cpp')?.[1]||'';
if(media)assert(media.includes('WINHTTP_OPTION_REDIRECT_POLICY_NEVER')&&media.includes('WINHTTP_DISABLE_COOKIES')&&media.includes('WINHTTP_DISABLE_AUTHENTICATION'),'LINE media downloads cannot silently redirect or use ambient cookies/authentication');
const bridge=sources.find(([n])=>n==='windows_source.cpp')[1];
assert(bridge.includes('GetSystemDirectoryW')&&bridge.includes('WindowsPowerShell/v1.0/powershell.exe'),'Bridge must use Windows system PowerShell');
const database=sources.find(([n])=>n==='database.cpp')[1];
assert(database.includes('LoadLibraryExW(L"winsqlite3.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)'),'SQLite must be the Windows system component');
assert.equal((all.match(/\bLoadLibrary(?:Ex)?[AW]\s*\(/g)||[]).length,1,'Review any new dynamic library load');
const app=sources.find(([n])=>n==='app.cpp')[1];
assert(!/\b(?:CreateProcessW|Bridge\s+\w+|WindowsSource\s+\w+)\b/.test(app.slice(app.indexOf('int WINAPI wWinMain'))),'Startup must not launch bridge/installers');
const exe=process.argv[2]||path.join(root,'native/out/ChatAtlas.exe');
if(!fs.existsSync(exe)){console.log('No-install source contract passed; PE import check not run (provide EXE path).');process.exit(0);}
const b=fs.readFileSync(exe),u16=o=>b.readUInt16LE(o),u32=o=>b.readUInt32LE(o);
assert.equal(b.subarray(0,2).toString(),'MZ');const pe=u32(0x3c);assert.equal(u32(pe),0x4550);assert.equal(u16(pe+4),0x8664);
const opt=pe+24;assert.equal(u16(opt),0x20b);const sections=[];
for(let i=0,s=opt+u16(pe+20);i<u16(pe+6);i++,s+=40)sections.push({rva:u32(s+12),size:Math.max(u32(s+8),u32(s+16)),raw:u32(s+20)});
function offset(rva){const s=sections.find(s=>rva>=s.rva&&rva<s.rva+s.size);assert(s,'RVA outside section');const p=s.raw+rva-s.rva;assert(p<b.length);return p;}
function string(p){const end=b.indexOf(0,p);assert(end>=p&&end-p<256);return b.subarray(p,end).toString('ascii');}
assert.equal(u32(opt+112+13*8),0,'Review any delay-loaded imports');
const imports=[];for(let p=offset(u32(opt+112+8));u32(p)||u32(p+12)||u32(p+16);p+=20)imports.push(string(offset(u32(p+12))).toLowerCase());
const allowed=new Set(['kernel32.dll','user32.dll','advapi32.dll','comctl32.dll','ole32.dll','shell32.dll','comdlg32.dll','gdi32.dll','wintrust.dll','crypt32.dll','winhttp.dll','bcrypt.dll']);
for(const name of imports)assert(allowed.has(name)||/^api-ms-win-crt-[a-z0-9-]+\.dll$/.test(name),'Unexpected runtime import: '+name);
assert(imports.length>0);
console.log('No-install source and Windows-system PE imports passed: '+imports.join(', '));
