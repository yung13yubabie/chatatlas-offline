// Portable source-contract regressions, not a substitute for real Windows UI QA.
const fs=require('node:fs'),assert=require('node:assert/strict'),path=require('node:path');
const cpp=fs.readFileSync(path.join(__dirname,'../src/app.cpp'),'utf8');
assert(cpp.includes('wcscmp(type,L"STATIC")?WS_TABSTOP:0'),'interactive controls join keyboard traversal');
assert(cpp.includes('if(id==ID_CONNECT){clearConnection();app.report=J();'),'reconnect discards stale source before new attempt');
assert(cpp.includes('atomicText(app.config/L"connection.local.json",content);clearConnection();'),'valid replacement profile invalidates old source');
const collect=cpp.slice(cpp.indexOf('void collectClicked()'),cpp.indexOf('void command('));
assert(collect.indexOf('if(kinds.empty())')<collect.indexOf('auto root=folderPath()'),'invalid content rejected before folder mutation');
assert(collect.indexOf('since>=until')<collect.indexOf('auto root=folderPath()'),'invalid dates rejected before folder mutation');
assert(collect.includes('result["failed"]')&&collect.includes('result["unavailable"]'),'partial saves are disclosed');
assert(cpp.includes('app.busy=false;taskControls(false);throw;'),'thread launch failure restores controls');
assert(cpp.includes('app.control->stopped=true;app.control->paused=false;EnableWindow(GetDlgItem(w,ID_PAUSE),FALSE)'),'close cancels pause and prevents re-pausing');
assert(cpp.includes('if(!app.closing&&(r->message.size()>250||r->error))'),'close is not blocked by completion modal');
assert(cpp.includes('if(!PostMessageW(app.window,MSG_CHATS,0,reinterpret_cast<LPARAM>(p)))delete p;'),'failed chat message does not leak payload');
const main=cpp.slice(cpp.indexOf('int WINAPI wWinMain'));
assert(main.indexOf('if(!demoMode&&!fs::is_regular_file(target/L"archive.sqlite3"))')<main.indexOf('Store store(target)'),'check does not create missing archive');
assert(main.includes('return issues.empty()?0:1;'),'check failures have nonzero exit status');
assert(main.includes('WriteFile(GetStdHandle(STD_ERROR_HANDLE)'),'headless errors report stderr');
console.log('Native application flow contracts passed; interactive Windows/LINE validation remains separate.');
