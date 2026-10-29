import os from 'node:os';
import path from 'node:path';
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {ROOT} from './check_public_tree.mjs';
import {ensureLineCipher,lineCipherFlags} from './fetch_line_cipher.mjs';
if(process.platform!=='win32')throw Error('These tests must execute on Windows');
const zig=process.env.ATLAS_ZIG||path.join(ROOT,'.toolchains/zig-x86_64-windows-0.14.1/zig.exe');
if(execFileSync(zig,['version'],{encoding:'utf8'}).trim()!=='0.14.1')throw Error('Pinned compiler required');
fs.mkdirSync(path.join(ROOT,'native/build'),{recursive:true});
const dependency=await ensureLineCipher();
const engineObject=path.join(ROOT,'native/build/line-cipher-engine.o');
execFileSync(zig,['cc','-std=c11','-O1',...lineCipherFlags,'-c',dependency.source,'-o',engineObject],{stdio:'inherit'});
const base=['c++','-std=c++20','-O1','-fstack-protector-strong','-DUNICODE','-D_UNICODE','-Isrc','-Iinclude','-Ivendor','-I'+dependency.directory,'-lwintrust','-lcrypt32','-lwinhttp','-lbcrypt'];
function run(args){execFileSync(zig,[...base,...args],{cwd:path.join(ROOT,'native'),stdio:'inherit'});}
run(['tests/private_path_contract_test.cpp','-o','build/private-path-contract-test.exe']);
execFileSync(path.join(ROOT,'native/build/private-path-contract-test.exe'),[],{stdio:'inherit'});
run(['tests/blob_path_contract_test.cpp','-o','build/blob-path-contract-test.exe']);
execFileSync(path.join(ROOT,'native/build/blob-path-contract-test.exe'),[],{stdio:'inherit'});
run(['tests/bridge_timing_test.cpp','-o','build/bridge-timing-test.exe']);
execFileSync(path.join(ROOT,'native/build/bridge-timing-test.exe'),[],{stdio:'inherit'});
run(['tests/request_control_test.cpp','-o','build/request-control-test.exe']);
execFileSync(path.join(ROOT,'native/build/request-control-test.exe'),[],{stdio:'inherit'});
const sources=['platform','database','store','archive','zip','demo','bundle','import','line_cipher','line_process','line_database','line_media','line_obs','line_locations'].map(x=>'src/'+x+'.cpp');
// Compile identical common inputs once per runner; preserve flags across all tests.
const objectRoot='build/windows-common';fs.mkdirSync(path.join(ROOT,'native',objectRoot),{recursive:true});
const objects=new Map();for(const source of sources){const object=objectRoot+'/'+path.basename(source,'.cpp')+'.o';run(['-c',source,'-o',object]);objects.set(source,object);}
const common=[...objects.values(),engineObject];
const object=name=>objects.get('src/'+name+'.cpp');
run([object('platform'),object('line_locations'),'tests/line_locations_test.cpp','-ladvapi32','-o','build/line-locations-test.exe']);
const locationsFixture=fs.mkdtempSync(path.join(os.tmpdir(),'chatatlas-line-locations-'));
try{execFileSync(path.join(ROOT,'native/build/line-locations-test.exe'),[locationsFixture],{stdio:'inherit'});}finally{fs.rmSync(locationsFixture,{recursive:true,force:true});}
run([...common,'tests/time_contract_test.cpp','-ladvapi32','-o','build/time-contract-test.exe']);
execFileSync(path.join(ROOT,'native/build/time-contract-test.exe'),[],{stdio:'inherit'});
run([object('platform'),'tests/windows_private_path_test.cpp','-ladvapi32','-o','build/windows-private-path-test.exe']);
const privatePathFixture=fs.mkdtempSync(path.join(os.tmpdir(),'chatatlas-private-path-'));
try{execFileSync(path.join(ROOT,'native/build/windows-private-path-test.exe'),[privatePathFixture],{stdio:'inherit'});}finally{fs.rmSync(privatePathFixture,{recursive:true,force:true});}
run([...common,'tests/windows_permissions_test.cpp','-ladvapi32','-o','build/windows-permissions-test.exe']);
execFileSync(path.join(ROOT,'native/build/windows-permissions-test.exe'),[],{stdio:'inherit'});
run([...common,'tests/core_test.cpp','-ladvapi32','-o','build/core-test.exe']);
execFileSync(path.join(ROOT,'native/build/core-test.exe'),[],{stdio:'inherit'});

run([...common.filter(s=>s!==object('zip')),'tests/zip_bounds_test.cpp','-ladvapi32','-o','build/zip-bounds-test.exe']);
execFileSync(path.join(ROOT,'native/build/zip-bounds-test.exe'),[],{stdio:'inherit'});
run([object('database'),object('platform'),'tests/database_contract_test.cpp','-ladvapi32','-o','build/database-contract-test.exe']);
const databaseFixture=fs.mkdtempSync(path.join(os.tmpdir(),'chatatlas-db-contract-'));
try{execFileSync(path.join(ROOT,'native/build/database-contract-test.exe'),[databaseFixture],{stdio:'inherit'});}finally{fs.rmSync(databaseFixture,{recursive:true,force:true});}

run([...common,'tests/item_value_contract_test.cpp','-ladvapi32','-o','build/item-value-contract-test.exe']);
execFileSync(path.join(ROOT,'native/build/item-value-contract-test.exe'),[],{stdio:'inherit'});

run([object('platform'),'tests/path_identity_test.cpp','-ladvapi32','-o','build/path-identity-test.exe']);
const pathFixture=fs.mkdtempSync(path.join(os.tmpdir(),'chatatlas-path-identity-'));
try{execFileSync(path.join(ROOT,'native/build/path-identity-test.exe'),[pathFixture],{stdio:'inherit'});}finally{fs.rmSync(pathFixture,{recursive:true,force:true});}

// Real controls, same-process synthetic DPI messages; no OS display setting change.
run([...common,'src/windows_source.cpp','tests/windows_ui_test.cpp','-lcomctl32','-lole32','-lshell32','-luuid','-lcomdlg32','-lgdi32','-ladvapi32','-o','build/windows-ui-test.exe']);
const uiFixture=fs.mkdtempSync(path.join(os.tmpdir(),'chatatlas-ui-dpi-'));
try{const exe=path.join(uiFixture,'windows-ui-test.exe');fs.copyFileSync(path.join(ROOT,'native/build/windows-ui-test.exe'),exe);execFileSync(exe,[],{cwd:uiFixture,stdio:'inherit'});}finally{fs.rmSync(uiFixture,{recursive:true,force:true});}

// New history and extracted-backup import use only artificial owned fixtures.
for(const [name,prefix] of [['blob_path_store','chatatlas-blob-path-store-'],['store_cancel','chatatlas-store-cancel-'],['import','chatatlas-import-'],['capture_history','chatatlas-capture-history-'],['line_cipher','chatatlas-line-cipher-'],['line_media','chatatlas-line-media-'],['line_database','chatatlas-line-database-']]){
 run([...common,'tests/'+name+'_test.cpp','-ladvapi32','-o','build/'+name+'-test.exe']);
 const fixture=fs.mkdtempSync(path.join(os.tmpdir(),prefix));
 try{execFileSync(path.join(ROOT,'native/build/'+name+'-test.exe'),[fixture],{stdio:'inherit'});}finally{fs.rmSync(fixture,{recursive:true,force:true});}
}

run([...common,'tests/line_process_test.cpp','-ladvapi32','-o','build/line-process-test.exe']);
execFileSync(path.join(ROOT,'native/build/line-process-test.exe'),[],{stdio:'inherit'});

run([...common,'tests/line_obs_test.cpp','-ladvapi32','-o','build/line-obs-test.exe']);
execFileSync(path.join(ROOT,'native/build/line-obs-test.exe'),[],{stdio:'inherit'});
