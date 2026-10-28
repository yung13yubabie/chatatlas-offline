// Deterministic ZIP32 for small release artifacts; no extraction or external dependencies.
import zlib from 'node:zlib';
const table=Uint32Array.from({length:256},(_,n)=>{for(let k=0;k<8;k++)n=(n&1)?0xedb88320^(n>>>1):n>>>1;return n>>>0;});
export function crc32(bytes){let crc=0xffffffff;for(const b of bytes)crc=table[(crc^b)&255]^(crc>>>8);return (crc^0xffffffff)>>>0;}
export function validName(name){return typeof name==='string'&&name.length>0&&!name.startsWith('/')&&!name.includes('\\')&&!name.includes('\0')&&!name.split('/').some(x=>x==='..'||x==='.'||!x);}
export function writeZip(files){
 const local=[],central=[];let offset=0;
 if(files.size>65535)throw Error('ZIP32 member limit');
 for(const [name,value]of [...files].sort(([a],[b])=>a<b?-1:a>b?1:0)){
  if(!validName(name))throw Error('Unsafe ZIP name');const filename=Buffer.from(name),bytes=Buffer.from(value),packed=zlib.deflateRawSync(bytes,{level:9}),crc=crc32(bytes);
  if(filename.length>65535||bytes.length>=0xffffffff||offset>=0xffffffff)throw Error('ZIP32 size limit');
  const h=Buffer.alloc(30);h.writeUInt32LE(0x04034b50);h.writeUInt16LE(20,4);h.writeUInt16LE(0x800,6);h.writeUInt16LE(8,8);h.writeUInt16LE(23585,12);h.writeUInt32LE(crc,14);h.writeUInt32LE(packed.length,18);h.writeUInt32LE(bytes.length,22);h.writeUInt16LE(filename.length,26);
  const c=Buffer.alloc(46);c.writeUInt32LE(0x02014b50);c.writeUInt16LE(0x314,4);h.copy(c,6,4,30);c.writeUInt32LE((0o100644*65536)>>>0,38);c.writeUInt32LE(offset,42);
  local.push(h,filename,packed);central.push(c,filename);offset+=h.length+filename.length+packed.length;
 }
 const directory=Buffer.concat(central),end=Buffer.alloc(22);end.writeUInt32LE(0x06054b50);end.writeUInt16LE(files.size,8);end.writeUInt16LE(files.size,10);end.writeUInt32LE(directory.length,12);end.writeUInt32LE(offset,16);
 return Buffer.concat([...local,directory,end]);
}
export function readZip(bytes){
 const result=new Map();let p=0;
 while(p+4<=bytes.length&&bytes.readUInt32LE(p)===0x04034b50){
  if(p+30>bytes.length)throw Error('Truncated ZIP');const flags=bytes.readUInt16LE(p+6),method=bytes.readUInt16LE(p+8),crc=bytes.readUInt32LE(p+14),packed=bytes.readUInt32LE(p+18),size=bytes.readUInt32LE(p+22),nl=bytes.readUInt16LE(p+26),el=bytes.readUInt16LE(p+28),start=p+30+nl+el;
  if(flags!==0x800||method!==8||size>16*1024*1024||start+packed>bytes.length)throw Error('Unsupported ZIP member');
  const name=new TextDecoder('utf-8',{fatal:true}).decode(bytes.subarray(p+30,p+30+nl));if(!validName(name)||result.has(name))throw Error('Unsafe/duplicate ZIP name');
  const value=zlib.inflateRawSync(bytes.subarray(start,start+packed),{maxOutputLength:16*1024*1024});if(value.length!==size||crc32(value)!==crc)throw Error('ZIP data checksum mismatch');result.set(name,value);p=start+packed;
 }
 if(!result.size||p+4>bytes.length||bytes.readUInt32LE(p)!==0x02014b50)throw Error('Missing ZIP directory');return result;
}
