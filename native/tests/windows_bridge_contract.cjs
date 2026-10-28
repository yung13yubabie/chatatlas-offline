// Portable source-contract checks; Windows behavioral tests are separate.
const fs=require('node:fs'),assert=require('node:assert/strict'),path=require('node:path');
const root=path.resolve(__dirname,'../..');
const cpp=fs.readFileSync(path.join(root,'native/src/windows_source.cpp'),'utf8');
const ps=fs.readFileSync(path.join(root,'src/line_vault/windows/uia_worker.ps1'),'utf8');
assert.equal(ps.charCodeAt(0),0xfeff,'Windows PowerShell 5.1 needs UTF-8 BOM');
assert(!cpp.includes('quote(wide(worker_script))'),'worker cannot be placed on command line');
assert(cpp.includes('auto payload=base64(worker_script)+"\\n"'),'worker must travel via pipe');
assert(cpp.includes('FromBase64String($worker)'));
assert(cpp.includes('p.contains("process_name")'),'reject obsolete process override');
assert(ps.includes('Get-Process -Name LINE'));
assert(ps.includes("$signature.Status -ne 'Valid'"));
assert(ps.includes('$p.StartTime.ToUniversalTime().Ticks -ne $script:ProcessStarted'));
assert(ps.includes('$p.Path -ine $script:ProcessPath'));
const final=ps.indexOf('    $invoke.Invoke()');
assert(final>ps.indexOf('    Assert-SaveDialog $dialog',ps.indexOf('$invoke=')));
assert(final>ps.indexOf('    Assert-SaveDestination $valuePattern $dest',ps.indexOf('$invoke=')));
assert(ps.includes('if (@(Get-LineDialogs).Count -ne 0)'));
console.log('Native bridge source contracts passed (not Windows runtime validation).');

assert(cpp.includes('[IO.StreamReader]::new([Console]::OpenStandardInput()'));
assert(!cpp.includes('[Console]::InputEncoding='));
assert(!ps.includes('[Console]::InputEncoding ='));
assert(ps.includes('[Console]::SetIn($ChatAtlasInput)'));
assert(ps.includes('[Console]::SetOut($ChatAtlasOutput)'));
