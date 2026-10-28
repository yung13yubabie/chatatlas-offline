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
