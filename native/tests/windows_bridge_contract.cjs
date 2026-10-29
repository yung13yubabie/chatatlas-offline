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
// Empty/tray-only and multiple-main-window cases must remain distinct across
// production selection, the worker's safe error allowlist and native display.
const mainSelection=ps.slice(ps.indexOf('function Get-LineMainProcess {'),ps.indexOf('while ($null -ne ($line=[Console]::ReadLine()))'));
assert(mainSelection.includes('Where-Object { $_.MainWindowHandle -ne 0 }'));
assert(mainSelection.includes("if ($processes.Count -eq 0) { throw 'line_window_missing' }"));
assert(mainSelection.includes("if ($processes.Count -gt 1) { throw 'line_window_ambiguous' }"));
assert(!mainSelection.includes('$processes.Count -ne 1'));
assert(mainSelection.indexOf("throw 'line_window_missing'")<mainSelection.indexOf('return $processes[0]'));
assert(mainSelection.indexOf("throw 'line_window_ambiguous'")<mainSelection.indexOf('return $processes[0]'));
const connect=ps.slice(ps.indexOf("   'connect' {"),ps.indexOf("   'activate' {"));
assert(connect.includes('$p=Get-LineMainProcess'));
assert(connect.indexOf('$p=Get-LineMainProcess')<connect.indexOf('Get-AuthenticodeSignature'));
const safeCodes=ps.match(/\$allowed=@\(([^\r\n]+)\)/)[1];
for(const code of ['line_window_missing','line_window_ambiguous']) {
 assert(safeCodes.includes("'"+code+"'"),'window diagnostic must survive the safe allowlist');
 assert(cpp.includes('if(code=="'+code+'")throw CapabilityError('),'native display must map the same code');
}
assert(ps.includes("$code=if($_.Exception.Message -in $allowed){$_.Exception.Message}else{'uia_operation_failed'}"));
const embedded=fs.readFileSync(path.join(root,'native/src/bundle.cpp'),'utf8').split('const char*worker_script=R"ATLAS(')[1].split(')ATLAS";')[0];
assert.equal(embedded,ps.replace(/^\uFEFF/,''),'embedded worker must match the edited script exactly');
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

// Regression evidence: these limits already existed in the immutable alpha.2.
assert(cpp.includes('error=CreateFileW(L"NUL"'));
assert(cpp.includes('si.hStdError=error'));
assert(cpp.includes('buffered.size()>16*1024*1024'));
const timing=fs.readFileSync(path.join(root,'native/src/bridge_timing.hpp'),'utf8');
assert(timing.includes('std::chrono::seconds(35)'));
assert(cpp.includes('BridgeRequestGate gate('));
assert(cpp.includes('PeekNamedPipe(output'));
assert(cpp.includes('if(control)control->check()'));
assert(!cpp.includes('errRead'));
assert(ps.includes('Wait-SaveCompletion $dialog $valuePattern $dest $dialogKey $dialogHandle'));
assert(ps.includes('Assert-OwnedSaveConfirmation $confirmation $dialog $dialogKey $dialogHandle'));
assert(ps.includes('[VaultNative]::GetWindow([IntPtr]$c.NativeWindowHandle,4)'));
assert(ps.includes('$close.Close()'));
assert(!ps.includes("Invoke-Item"));
console.log('Bridge limits and narrowly owned confirmation source contracts passed.');

// A modal Close is asynchronous with respect to focus restoration. The bounded
// cleanup may observe, but never activate or overwrite an unverified window.
const cleanup=ps.slice(ps.indexOf('function Close-TrackedSaveAfterConfirmation('),ps.indexOf('function Wait-SaveCompletion('));
assert(cleanup.includes('[Diagnostics.Stopwatch]::StartNew()'));
assert(cleanup.includes('$elapsed.ElapsedMilliseconds -lt 500'));
assert(cleanup.includes('Start-Sleep -Milliseconds 20'));
assert(cleanup.includes('Assert-Window'));
assert(cleanup.includes('(Node-Key $dialog) -cne $dialogKey'));
assert(cleanup.includes('[VaultNative]::GetWindow([IntPtr]$dialogHandle,4) -ne $dialogOwner'));
assert(cleanup.includes('$valuePattern.Current.Value -cne $dest'));
assert(cleanup.includes('(Node-Key $extra[0]) -cne $confirmationKey'));
assert(cleanup.includes('$extra.Count -eq 0 -and $foreground -eq [IntPtr]$dialogHandle'));
assert(cleanup.includes("throw 'focus_changed'"));
assert(cleanup.includes("throw 'save_conflict_cleanup_required'"));
assert(!cleanup.includes('SetForegroundWindow'));
assert(!cleanup.includes('.Invoke('));
assert(!cleanup.includes('catch { }'));
assert(ps.includes("'save_conflict','save_conflict_cleanup_required','save_incomplete'"));
console.log('Bounded, identity-checked save cancellation source contracts passed.');
