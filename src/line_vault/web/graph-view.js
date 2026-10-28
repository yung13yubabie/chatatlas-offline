"use strict";
window.VaultGraphView = (svg,choose) => {
 const ns='http://www.w3.org/2000/svg',motion=window.matchMedia('(prefers-reduced-motion: reduce)');
 let model={nodes:[],edges:[]},elements=new Map(),byId=new Map(),positions=new Map(),selected=null,focused=null,zoom=1,pan={x:0,y:0},drag=null,frame=0,panDrag=null,suppressClick=false;
 const make=(tag,attrs={})=>{const x=document.createElementNS(ns,tag);for(const[k,v]of Object.entries(attrs))x.setAttribute(k,String(v));return x;};
 const layer=make('g'),edgeLayer=make('g',{'class':'graph-edges'}),nodeLayer=make('g');layer.append(edgeLayer,nodeLayer);svg.append(layer);
 const colors={chat:'#38685a',topic:'#a97a3d',person:'#737fa9',source:'#638a75',file:'#a66d7c'},radii={chat:25,topic:16,person:13,source:9,file:11};
 const transform=()=>{layer.setAttribute('transform',`translate(${500+pan.x} ${320+pan.y}) scale(${zoom}) translate(-500 -320)`);const level=document.getElementById('zoom-level');if(level)level.textContent=Math.round(zoom*100)+'%';};
 function paint(){for(const e of model.edges){const a=byId.get(e.source),b=byId.get(e.target),line=elements.get(e.id);if(!a||!b)continue;line.setAttribute('x1',a.x);line.setAttribute('y1',a.y);line.setAttribute('x2',b.x);line.setAttribute('y2',b.y);}for(const n of model.nodes){elements.get(n.id).setAttribute('transform',`translate(${n.x} ${n.y})`);positions.set(n.id,{x:n.x,y:n.y});}layoutLabels();}

 // Resolve labels in graph coordinates so zooming and panning preserve decisions.
 // Room names and the active/focused node win; hidden text remains available via
 // the node's accessible name, native SVG title and the source inspector.
 const measure=document.createElement('canvas').getContext('2d');
 function labelMetrics(n){const text=elements.get(n.id)?.querySelector('text');if(!text)return;const mobile=window.innerWidth<820,size=n.type==='chat'?(mobile?24:17):(mobile?22:15);measure.font='650 '+size+'px system-ui, "Microsoft JhengHei", sans-serif';n.labelWidth=measure.measureText(text.textContent).width+8;n.labelHeight=size*1.35;n.labelSize=size;}
 function layoutLabels(){
  const occupied=[],priority={chat:1,topic:2,person:3,file:4,source:5},ordered=[...model.nodes].sort((a,b)=>{const rank=n=>n.id===focused?-2:n.id===selected?-1:priority[n.type]??6;return rank(a)-rank(b)||a.id.localeCompare(b.id);});
  const overlaps=(a,b)=>a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;
  for(const n of ordered){const text=elements.get(n.id)?.querySelector('text');if(!text)continue;if(!n.labelWidth)labelMetrics(n);const w=n.labelWidth,h=n.labelHeight,r=radii[n.type],gap=9,size=n.labelSize;
   const candidates=[{x:0,y:r+gap+size,anchor:'middle'},{x:0,y:-r-gap,anchor:'middle'},{x:r+gap,y:size*.35,anchor:'start'},{x:-r-gap,y:size*.35,anchor:'end'}];
   let chosen=null;for(const c of candidates){const x=n.x+c.x-(c.anchor==='middle'?w/2:c.anchor==='end'?w:0),y=n.y+c.y-size*1.05,box={x:x-3,y:y-3,w:w+6,h:h+6};
    if(box.x<12||box.x+box.w>988||box.y<10||box.y+box.h>620||occupied.some(b=>overlaps(box,b)))continue;
    if(model.nodes.some(other=>other!==n&&overlaps(box,{x:other.x-radii[other.type]-3,y:other.y-radii[other.type]-3,w:radii[other.type]*2+6,h:radii[other.type]*2+6})))continue;
    chosen={...c,box};break;
   }
   // A selected/focused label is always readable: priority means it is placed
   // before other labels; allowing marker overlap is preferable to hiding it.
   if(!chosen&&(n.id===selected||n.id===focused||n.type==='chat'))for(const c of candidates){const box={x:n.x+c.x-(c.anchor==='middle'?w/2:c.anchor==='end'?w:0)-3,y:n.y+c.y-size*1.05-3,w:w+6,h:h+6};if(!occupied.some(b=>overlaps(box,b))){chosen={...c,box};break;}}
   if(chosen){text.setAttribute('x',chosen.x);text.setAttribute('y',chosen.y);text.setAttribute('text-anchor',chosen.anchor);text.setAttribute('visibility','visible');text.setAttribute('data-label-visible','true');occupied.push(chosen.box);}else{text.setAttribute('visibility','hidden');text.setAttribute('data-label-visible','false');}
  }
 }
 function highlight(id){selected=id;const near=new Set([id]),chosen=byId.get(id);if(chosen?.type==='chat'){const items=new Set(chosen.items);for(const n of model.nodes)if(n.items.some(x=>items.has(x)))near.add(n.id);}for(const e of model.edges)if(e.source===id||e.target===id){near.add(e.source);near.add(e.target);}for(const n of model.nodes){const item=elements.get(n.id);item.classList.toggle('selected',n.id===id);item.classList.toggle('muted',!!id&&!near.has(n.id));item.setAttribute('aria-pressed',String(n.id===id));}for(const e of model.edges){const line=elements.get(e.id);line.classList.toggle('active',e.source===id||e.target===id);line.classList.toggle('muted',!!id&&e.source!==id&&e.target!==id);}const reset=document.getElementById('clear-map-selection');if(reset)reset.hidden=!id;layoutLabels();}
 function step(){const nodes=model.nodes;for(let k=0;k<3;k++){
  for(const n of nodes){n.fx=(500-n.x)*.002;n.fy=(320-n.y)*.002;}
  for(let i=0;i<nodes.length;i++)for(let j=i+1;j<nodes.length;j++){const a=nodes[i],b=nodes[j];let dx=b.x-a.x,dy=b.y-a.y,d=Math.hypot(dx,dy)||.1;const f=Math.min(7,4200/(d*d));a.fx-=dx/d*f;a.fy-=dy/d*f;b.fx+=dx/d*f;b.fy+=dy/d*f;}
  for(const e of model.edges){const a=byId.get(e.source),b=byId.get(e.target);if(!a||!b)continue;const dx=b.x-a.x,dy=b.y-a.y,d=Math.hypot(dx,dy)||.1,w=e.type==='belongs'?.010:.005,f=(d-(e.type==='belongs'?125:145))*w;a.fx+=dx/d*f;a.fy+=dy/d*f;b.fx-=dx/d*f;b.fy-=dy/d*f;}
  for(const n of nodes){if(drag?.node===n)continue;n.x=Math.max(72,Math.min(925,n.x+n.fx));n.y=Math.max(60,Math.min(570,n.y+n.fy));}
 }}
 function settle(steps){cancelAnimationFrame(frame);if(motion.matches){for(let i=0;i<steps;i++)step();paint();return;}let turn=0;const tick=()=>{step();paint();if(++turn<steps)frame=requestAnimationFrame(tick);};tick();}
 function point(event){const p=svg.createSVGPoint();p.x=event.clientX;p.y=event.clientY;return p.matrixTransform(layer.getScreenCTM().inverse());}
 function focusNode(id){for(const n of model.nodes)elements.get(n.id).setAttribute('tabindex',String(n.id===id?0:-1));elements.get(id)?.focus();}
 function navigate(n,event){const directions={ArrowRight:[1,0],ArrowLeft:[-1,0],ArrowDown:[0,1],ArrowUp:[0,-1]},direction=directions[event.key];if(event.key==='Home'||event.key==='End'){event.preventDefault();focusNode(model.nodes[event.key==='Home'?0:model.nodes.length-1]?.id);return;}if(!direction)return;event.preventDefault();let target=null,best=Infinity;for(const other of model.nodes){if(other===n)continue;const dx=other.x-n.x,dy=other.y-n.y,forward=dx*direction[0]+dy*direction[1];if(forward<=0)continue;const lateral=Math.abs(dx*direction[1]-dy*direction[0]),score=Math.hypot(dx,dy)+lateral*2;if(score<best){best=score;target=other;}}if(target)focusNode(target.id);}
 function set(value){cancelAnimationFrame(frame);const mobile=window.innerWidth<820;svg.setAttribute('viewBox',mobile?'150 20 700 600':'0 0 1000 640');const previousFocus=document.activeElement?.getAttribute('data-node-id');model=value;byId=new Map(model.nodes.map(n=>[n.id,n]));elements=new Map();edgeLayer.replaceChildren();nodeLayer.replaceChildren();let index=0,added=0;
  const count=Math.max(1,model.nodes.length);for(const n of model.nodes){const previous=positions.get(n.id),angle=index++*2.3999632297,rad=n.type==='chat'?90:130+Math.sqrt(index/count)*135;n.x=previous?.x??500+Math.cos(angle)*rad;n.y=previous?.y??315+Math.sin(angle)*rad;if(!previous)added++;}
  for(const e of model.edges){const line=make('line',{'class':'graph-edge '+e.type});const title=make('title');title.textContent=e.label;line.append(title);edgeLayer.append(line);elements.set(e.id,line);}
  for(const n of model.nodes){const g=make('g',{'class':'graph-node '+n.type,role:'button',tabindex:n===model.nodes[0]?0:-1,'aria-label':n.title,'data-node-id':n.id,'data-node-type':n.type}),hit=make('circle',{r:Math.max(22,radii[n.type]+9),fill:'transparent','class':'node-hit'}),halo=make('circle',{r:radii[n.type]+7,'class':'node-halo'}),dot=make('circle',{r:radii[n.type],fill:colors[n.type]}),text=make('text',{y:radii[n.type]+22,'text-anchor':'middle'}),maxLabel=mobile?8:12;text.textContent=n.title.length>maxLabel?n.title.slice(0,maxLabel)+'…':n.title;const title=make('title');title.textContent=n.title;g.append(hit,halo,dot,text,title);
   g.addEventListener('focus',()=>{focused=n.id;layoutLabels();});g.addEventListener('blur',()=>{if(focused===n.id)focused=null;layoutLabels();});
   g.addEventListener('click',e=>{e.stopPropagation();if(suppressClick){suppressClick=false;return;}focusNode(n.id);highlight(n.id);choose(n);});
   g.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();highlight(n.id);choose(n);}else if(e.key==='Escape'){e.preventDefault();highlight(null);choose(null);}else navigate(n,e);});
   g.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.stopPropagation();g.setPointerCapture(e.pointerId);const p=point(e);drag={node:n,offsetX:n.x-p.x,offsetY:n.y-p.y,startX:e.clientX,startY:e.clientY};suppressClick=false;});
   g.addEventListener('pointermove',e=>{if(drag?.node!==n)return;if(Math.hypot(e.clientX-drag.startX,e.clientY-drag.startY)>5)suppressClick=true;const p=point(e);n.x=p.x+drag.offsetX;n.y=p.y+drag.offsetY;paint();});g.addEventListener('pointerup',()=>{drag=null;});g.addEventListener('pointercancel',()=>{drag=null;suppressClick=false;});nodeLayer.append(g);elements.set(n.id,g);labelMetrics(n);
  }transform();if(added)settle(positions.size?18:55);else paint();highlight(byId.has(selected)?selected:null);if(previousFocus&&byId.has(previousFocus))focusNode(previousFocus);
  // Bound retained positions when browsing large collections.
  if(positions.size>1500)positions=new Map(model.nodes.map(n=>[n.id,{x:n.x,y:n.y}]));
 }
 svg.addEventListener('pointerdown',e=>{if(e.target.closest('.graph-node')||e.button!==0)return;panDrag={x:e.clientX,y:e.clientY,px:pan.x,py:pan.y};svg.setPointerCapture(e.pointerId);});svg.addEventListener('pointermove',e=>{if(!panDrag)return;const rect=svg.getBoundingClientRect();pan.x=panDrag.px+(e.clientX-panDrag.x)*svg.viewBox.baseVal.width/rect.width;pan.y=panDrag.py+(e.clientY-panDrag.y)*svg.viewBox.baseVal.height/rect.height;transform();});svg.addEventListener('pointerup',e=>{if(panDrag&&Math.hypot(e.clientX-panDrag.x,e.clientY-panDrag.y)<5){highlight(null);choose(null);}panDrag=null;});svg.addEventListener('pointercancel',()=>{panDrag=null;drag=null;});
 function resized(){const mobile=window.innerWidth<820;svg.setAttribute('viewBox',mobile?'150 20 700 600':'0 0 1000 640');for(const n of model.nodes){const text=elements.get(n.id)?.querySelector('text'),limit=mobile?8:12;if(text)text.textContent=n.title.length>limit?n.title.slice(0,limit)+'…':n.title;labelMetrics(n);}layoutLabels();}window.addEventListener('resize',resized);
 function reduceChanged(){if(motion.matches){cancelAnimationFrame(frame);for(let i=0;i<30;i++)step();paint();}}motion.addEventListener('change',reduceChanged);
 return {set,select:highlight,zoom:amount=>{zoom=Math.max(.45,Math.min(2.5,zoom*amount));transform();},center:()=>{zoom=1;pan={x:0,y:0};transform();},destroy:()=>{cancelAnimationFrame(frame);window.removeEventListener('resize',resized);motion.removeEventListener('change',reduceChanged);}};
};
