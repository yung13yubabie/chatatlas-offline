// Developer-only C compiler entrypoint; no dependency is installed at app runtime.
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {ensureLineCipher,lineCipherFlags} from './fetch_line_cipher.mjs';
const args=process.argv.slice(2);
if(args.length!==4||args[0]!=='--cc'||args[2]!=='--output')throw Error('Usage: node tools/build_line_cipher.mjs --cc compiler --output object');
const dependency=await ensureLineCipher();
const result=spawnSync(args[1],['-std=c11','-D_GNU_SOURCE','-O1','-fvisibility=hidden',...lineCipherFlags,'-c',dependency.source,'-o',path.resolve(args[3])],{stdio:'inherit'});
if(result.error)throw result.error;
if(result.status!==0)throw Error('Native LINE cipher compilation failed');
