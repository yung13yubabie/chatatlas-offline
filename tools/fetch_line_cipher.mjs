// Developer-only source dependency. The application never downloads a cipher engine.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import {fileURLToPath} from 'node:url';
import {crc32} from './zip_archive.mjs';

const repo=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
export const lineCipherVersion='2.6.1';
export const lineCipherSqliteVersion='3.54.0';
export const lineCipherArchiveSha256='3f25af603566a23c821d8e581778b5ece2a541d2435776de57b8d845c176815a';
export const lineCipherFlags=['-DSQLITE_THREADSAFE=1','-DSQLITE_OMIT_LOAD_EXTENSION=1','-DSQLITE_TEMP_STORE=3','-DSQLITE_DQS=0','-DCODEC_TYPE=CODEC_TYPE_AES128','-DHAVE_CIPHER_AES_128_CBC=1','-DHAVE_CIPHER_AES_256_CBC=0','-DHAVE_CIPHER_CHACHA20=1','-DHAVE_CIPHER_SQLCIPHER=0','-DHAVE_CIPHER_RC4=0','-DHAVE_CIPHER_ASCON128=0','-DHAVE_CIPHER_AEGIS=0','-DSQLITE3MC_OMIT_AES_HARDWARE_SUPPORT=1','-DSQLITE3MC_SECURE_MEMORY=1'];
const asset='sqlite3mc-2.6.1-sqlite-3.54.0-amalgamation.zip';
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
export const lineCipherFileHashes=Object.freeze({
  'sqlite3mc_amalgamation.c':'9b129d1ceb2c7d5dbf374b833eb29a640ecfa8e4a5af6d48508b429019c707b0',
  'sqlite3mc_amalgamation.h':'99b3e6997aa5fc604d9ea2352e6ae89dfd4d0af8d5bd7cbfe7293d959779e6eb',
  'sqlite3.h':'cd20a2877bc82b94e36c1093b727dd87cfa3c1d581aac6e240925535cbd90166',
});
const expected=lineCipherFileHashes;
function noLinks(p){for(let here=p;;here=path.dirname(here)){if(fs.existsSync(here)&&fs.lstatSync(here).isSymbolicLink())throw Error('Cipher source path must not contain links');const next=path.dirname(here);if(next===here)break;}}
function verified(directory){
  noLinks(directory);
  for(const [name,digest] of Object.entries(expected)){
    const p=path.join(directory,name);if(!fs.existsSync(p))return false;
    if(!fs.lstatSync(p).isFile()||sha(fs.readFileSync(p))!==digest)throw Error('Pinned cipher source differs: '+name);
  }return true;
}
// Only invoked after the complete upstream archive matches the pinned digest.
function unpack(bytes){
  const files=new Map();let offset=0;
  while(offset+30<=bytes.length&&bytes.readUInt32LE(offset)===0x04034b50){
    const flags=bytes.readUInt16LE(offset+6),method=bytes.readUInt16LE(offset+8),crc=bytes.readUInt32LE(offset+14),packed=bytes.readUInt32LE(offset+18),size=bytes.readUInt32LE(offset+22),nl=bytes.readUInt16LE(offset+26),el=bytes.readUInt16LE(offset+28),start=offset+30+nl+el;
    if(flags!==0||method!==8||size>16*1024*1024||start+packed>bytes.length)throw Error('Unexpected pinned cipher ZIP member');
    const name=bytes.subarray(offset+30,offset+30+nl).toString('ascii');
    if(!['sqlite3mc_amalgamation.c','sqlite3mc_amalgamation.h','sqlite3.h','sqlite3ext.h','shell3mc_amalgamation.c','README.md'].includes(name)||files.has(name))throw Error('Unexpected cipher archive path');
    const value=zlib.inflateRawSync(bytes.subarray(start,start+packed),{maxOutputLength:16*1024*1024});
    if(value.length!==size||crc32(value)!==crc)throw Error('Cipher ZIP checksum mismatch');
    files.set(name,value);offset=start+packed;
  }
  if(files.size!==6||offset+4>bytes.length||bytes.readUInt32LE(offset)!==0x02014b50)throw Error('Incomplete cipher source archive');
  return files;
}
export async function ensureLineCipher(){
  const directory=path.join(repo,'.toolchains','sqlite3mc-'+lineCipherVersion);
  if(!verified(directory)){
    const root=path.dirname(directory);noLinks(root);fs.mkdirSync(root,{recursive:true});
    const archive=path.join(root,asset);let bytes;
    if(fs.existsSync(archive)){noLinks(archive);bytes=fs.readFileSync(archive);}
    else{
      const response=await fetch('https://github.com/utelle/SQLite3MultipleCiphers/releases/download/v'+lineCipherVersion+'/'+asset,{signal:AbortSignal.timeout(120000)});
      if(!response.ok)throw Error('Official cipher source download failed: '+response.status);
      const chunks=[];let length=0;
      for await(const chunk of response.body){length+=chunk.length;if(length>8*1024*1024)throw Error('Cipher archive size limit exceeded');chunks.push(chunk);}
      bytes=Buffer.concat(chunks);
    }
    if(sha(bytes)!==lineCipherArchiveSha256)throw Error('Cipher archive does not match the pinned SHA-256');
    const files=unpack(bytes);
    for(const [name,digest] of Object.entries(expected))if(!files.has(name)||sha(files.get(name))!==digest)throw Error('Pinned cipher member mismatch');
    if(!fs.existsSync(archive))fs.writeFileSync(archive,bytes,{flag:'wx'});
    noLinks(directory);fs.mkdirSync(directory,{recursive:true});
    for(const [name] of Object.entries(expected)){
      const target=path.join(directory,name);noLinks(target);
      if(fs.existsSync(target)){if(sha(fs.readFileSync(target))!==expected[name])throw Error('Refusing to replace existing cipher source');}
      else fs.writeFileSync(target,files.get(name),{flag:'wx'});
    }
    if(!verified(directory))throw Error('Cipher dependency extraction failed');
  }
  return {directory,source:path.join(directory,'sqlite3mc_amalgamation.c'),header:path.join(directory,'sqlite3mc_amalgamation.h'),version:lineCipherVersion,sqliteVersion:lineCipherSqliteVersion,archiveSha256:lineCipherArchiveSha256,files:{...expected}};
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))console.log((await ensureLineCipher()).directory);
