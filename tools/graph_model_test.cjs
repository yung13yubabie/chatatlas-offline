const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const context={window:{},Intl};vm.createContext(context);vm.runInContext(fs.readFileSync(path.join(__dirname,'../src/line_vault/web/graph-model.js'),'utf8'),context);const build=context.window.VaultGraphModel.build;
const item=(id,chat,text,more={})=>({id,chat_id:chat,source_id:id,kind:'message',text,title:'',sender_name:'同名成員',sent_at:null,state:'active',attachments:[],...more});
const rooms=[{id:'a',title:'A'},{id:'b',title:'B'}];let rows=[item('1','a','#共同主題 準備計畫'),item('2','b','#共同主題 行程筆記')],g=build(rows,rooms);
assert.equal(g.nodes.filter(n=>n.type==='person').length,2,'unknown same-name speakers remain separate');
assert.equal(g.nodes.filter(n=>n.type==='topic'&&n.title==='共同主題').length,1,'explicit keyword is shared');
for(const n of g.nodes)if(n.type!=='chat')assert(g.edges.some(e=>e.source===n.id||e.target===n.id),'no orphan node');
for(const e of g.edges){assert(g.nodes.some(n=>n.id===e.source));assert(g.nodes.some(n=>n.id===e.target));}
rows=rows.map(i=>({...i,sender_ref:'same-confirmed-person'}));g=build(rows,rooms);assert.equal(g.nodes.filter(n=>n.type==='person').length,1,'verified person can span rooms');
const person=g.nodes.find(n=>n.type==='person');assert.equal(g.edges.filter(e=>e.target===person.id&&e.type==='speaker').length,2);
const recalled=item('old','a','secret keyword secret keyword',{state:'recalled'});g=build([recalled],rooms.slice(0,1));assert(!g.nodes.some(n=>n.type==='topic'),'recalled words are not indexed');
const manyRooms=Array.from({length:35},(_,i)=>({id:String(i),title:'Room '+i})),manyRows=manyRooms.flatMap(c=>Array.from({length:100},(_,i)=>item(c.id+'-'+i,c.id,'#同一標籤 '+i,{sender_name:'Member '+i,attachments:[{id:c.id+'-file-'+i,name:'Example '+i}]})));
g=build(manyRows,manyRooms);assert(g.nodes.length<=120,'bounded visible map');assert(g.limited);for(const n of g.nodes)if(n.type!=='chat')assert(g.edges.some(e=>e.source===n.id||e.target===n.id),'capped map has no orphan node');
console.log('graph model: evidence links, same-name isolation, confirmed cross-room people, recalled-word exclusion, bounded graph and no dangling edges passed.');
