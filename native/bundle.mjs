import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const base=path.dirname(fileURLToPath(import.meta.url)),web=path.join(base,'..','src','line_vault','web');
const resources={web_template:fs.readFileSync(path.join(web,'index.html'),'utf8'),web_css:fs.readFileSync(path.join(web,'reader.css'),'utf8'),web_script:['graph-model.js','graph-view.js','download.js','reader.js'].map(n=>fs.readFileSync(path.join(web,n),'utf8')).join('\n'),worker_script:fs.readFileSync(path.join(base,'..','src','line_vault','windows','uia_worker.ps1'),'utf8')};
let output='#include "core.hpp"\nnamespace atlas {\n';for(const[name,value]of Object.entries(resources)){if(value.includes(')ATLAS"'))throw Error('Reserved delimiter');output+=`const char*${name}=R"ATLAS(${value})ATLAS";\n`;}output+='}\n';fs.writeFileSync(path.join(base,'src','bundle.cpp'),output);
