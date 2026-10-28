import os from 'node:os';
import path from 'node:path';
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {ROOT} from './check_public_tree.mjs';
if(process.platform!=='win32')throw Error('These tests must execute on Windows');
const zig=process.env.ATLAS_ZIG||path.join(ROOT,'.toolchains/zig-x86_64-windows-0.14.1/zig.exe');
if(execFileSync(zig,['version'],{encoding:'utf8'}).trim()!=='0.14.1')throw Error('Pinned compiler required');
fs.mkdirSync(path.join(ROOT,'native/build'),{recursive:true});
const base=['c++','-std=c++20','-O1','-fstack-protector-strong','-DUNICODE','-D_UNICODE','-Isrc','-Iinclude','-Ivendor'];
function run(args){execFileSync(zig,[...base,...args],{cwd:path.join(ROOT,'native'),stdio:'inherit'});}
const sources=['platform','database','store','archive','zip','demo','bundle'].map(x=>'src/'+x+'.cpp');
run([...sources,'tests/windows_permissions_test.cpp','-ladvapi32','-o','build/windows-permissions-test.exe']);
execFileSync(path.join(ROOT,'native/build/windows-permissions-test.exe'),[],{stdio:'inherit'});
run([...sources,'tests/core_test.cpp','-ladvapi32','-o','build/core-test.exe']);
execFileSync(path.join(ROOT,'native/build/core-test.exe'),[],{stdio:'inherit'});

run([...sources.filter(s=>s!=='src/zip.cpp'),'tests/zip_bounds_test.cpp','-ladvapi32','-o','build/zip-bounds-test.exe']);
execFileSync(path.join(ROOT,'native/build/zip-bounds-test.exe'),[],{stdio:'inherit'});
run(['src/database.cpp','src/platform.cpp','tests/database_contract_test.cpp','-ladvapi32','-o','build/database-contract-test.exe']);
const databaseFixture=fs.mkdtempSync(path.join(os.tmpdir(),'chatatlas-db-contract-'));
try{execFileSync(path.join(ROOT,'native/build/database-contract-test.exe'),[databaseFixture],{stdio:'inherit'});}finally{fs.rmSync(databaseFixture,{recursive:true,force:true});}

run(['src/platform.cpp','tests/path_identity_test.cpp','-ladvapi32','-o','build/path-identity-test.exe']);
const pathFixture=fs.mkdtempSync(path.join(os.tmpdir(),'chatatlas-path-identity-'));
try{execFileSync(path.join(ROOT,'native/build/path-identity-test.exe'),[pathFixture],{stdio:'inherit'});}finally{fs.rmSync(pathFixture,{recursive:true,force:true});}

// Real controls, same-process synthetic DPI messages; no OS display setting change.
run([...sources,'src/windows_source.cpp','tests/windows_ui_test.cpp','-lcomctl32','-lole32','-lshell32','-luuid','-lcomdlg32','-lgdi32','-ladvapi32','-o','build/windows-ui-test.exe']);
const uiFixture=fs.mkdtempSync(path.join(os.tmpdir(),'chatatlas-ui-dpi-'));
try{const exe=path.join(uiFixture,'windows-ui-test.exe');fs.copyFileSync(path.join(ROOT,'native/build/windows-ui-test.exe'),exe);execFileSync(exe,[],{cwd:uiFixture,stdio:'inherit'});}finally{fs.rmSync(uiFixture,{recursive:true,force:true});}
