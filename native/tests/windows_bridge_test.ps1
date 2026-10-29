$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../../src/line_vault/windows/uia_worker.ps1'
$tokens=$null; $errors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path $source),[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
# Load only pure guards; never execute the worker or contact a real LINE process.
foreach ($name in @('Get-LineMainProcess','Assert-Window','Assert-SaveDialog','Assert-SaveDestination','Assert-OwnedSaveConfirmation','Close-TrackedSaveAfterConfirmation','Wait-SaveCompletion')) {
 $fn=$ast.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
 if (!$fn) { throw "Missing guard $name" }
 Invoke-Expression $fn.Extent.Text
}
Add-Type 'public static class VaultNative { public static System.IntPtr Foreground = new System.IntPtr(123); public static System.IntPtr Owner = new System.IntPtr(123); public static System.IntPtr ParentOwner = System.IntPtr.Zero; public static System.IntPtr GetWindow(System.IntPtr h, uint command) { return h.ToInt64() == 123 ? ParentOwner : Owner; } public static System.IntPtr GetForegroundWindow() { return Foreground; } }'
$script:ProcessId=42; $script:ProcessPath='C:\Synthetic\LINE.exe'
$script:ProcessStarted=123
$script:Main=[pscustomobject]@{Current=[pscustomobject]@{ProcessId=42}}
$script:FakeProcess=[pscustomobject]@{ProcessName='LINE'; StartTime=[datetime]::new(123,[DateTimeKind]::Utc); Path=$script:ProcessPath}
function Get-Process {
 param($Id,$Name,$ErrorAction)
 if ($Name) {
  if ($Name -cne 'LINE' -or $ErrorAction -cne 'SilentlyContinue') { throw 'unexpected_process_query' }
  return $script:FakeProcesses
 }
 return $script:FakeProcess
}
$script:SaveDialogNames=@('Save As')
function New-Dialog { return [pscustomobject]@{Current=[pscustomobject]@{ProcessId=42; ClassName='#32770'; Name='Save As'; IsEnabled=$true; IsOffscreen=$false; NativeWindowHandle=123}} }
function Expect-Error($action,$expected) {
 try { & $action; throw 'test_did_not_fail' } catch { if ($_.Exception.Message -cne $expected) { throw } }
}
# Keep process-selection fixtures in their own scope. PowerShell variable names
# are case-insensitive: a script-level $main would overwrite $script:Main.
function Test-LineMainProcessSelection {
 $priorCandidates=Get-Variable -Name FakeProcesses -Scope Script -ErrorAction SilentlyContinue
 $priorCandidateValue=$null
 if ($null -ne $priorCandidates) { $priorCandidateValue=$priorCandidates.Value }
 try {
  # Exercise the exact production process selection with synthetic objects only.
  # An absent process and a tray-only process are missing windows, not ambiguity.
  $script:FakeProcesses=@()
  Expect-Error { Get-LineMainProcess } 'line_window_missing'
  $candidateTray=[pscustomobject]@{MainWindowHandle=[IntPtr]::Zero; Id=101}
  $candidateMain=[pscustomobject]@{MainWindowHandle=[IntPtr]123; Id=102}
  $candidateSecond=[pscustomobject]@{MainWindowHandle=[IntPtr]124; Id=103}
  $script:FakeProcesses=@($candidateTray)
  Expect-Error { Get-LineMainProcess } 'line_window_missing'
  $script:FakeProcesses=@($candidateMain)
  if ((Get-LineMainProcess).Id -ne 102) { throw 'single_main_window_not_selected' }
  $script:FakeProcesses=@($candidateTray,$candidateMain)
  if ((Get-LineMainProcess).Id -ne 102) { throw 'tray_process_counted_as_main_window' }
  $script:FakeProcesses=@($candidateMain,$candidateSecond)
  Expect-Error { Get-LineMainProcess } 'line_window_ambiguous'
  $script:FakeProcesses=@($candidateTray,$candidateMain,$candidateSecond)
  Expect-Error { Get-LineMainProcess } 'line_window_ambiguous'
  # Re-evaluate every connect request; do not retain a disappeared candidate.
  $script:FakeProcesses=@()
  Expect-Error { Get-LineMainProcess } 'line_window_missing'
 } finally {
  if ($null -ne $priorCandidates) { $script:FakeProcesses=$priorCandidateValue }
  else { Remove-Variable -Name FakeProcesses -Scope Script }
 }
}
$identityBeforeSelection=[pscustomobject]@{Main=$script:Main; ProcessId=$script:ProcessId; ProcessStarted=$script:ProcessStarted; ProcessPath=$script:ProcessPath; FakeProcess=$script:FakeProcess}
Test-LineMainProcessSelection
if (![Object]::ReferenceEquals($script:Main,$identityBeforeSelection.Main) -or $script:ProcessId -ne $identityBeforeSelection.ProcessId -or $script:ProcessStarted -ne $identityBeforeSelection.ProcessStarted -or $script:ProcessPath -cne $identityBeforeSelection.ProcessPath -or ![Object]::ReferenceEquals($script:FakeProcess,$identityBeforeSelection.FakeProcess)) { throw 'selection_fixture_changed_connection_identity' }
Assert-Window
Write-Output 'Seven synthetic LINE main-window selection cases passed; connection fixture identity preserved.'
Assert-SaveDialog (New-Dialog)
foreach ($change in @(@('Name','Open'),@('ClassName','NotADialog'),@('ProcessId',99),@('IsEnabled',$false),@('IsOffscreen',$true),@('NativeWindowHandle',0))) {
 $dialog=New-Dialog; $dialog.Current.($change[0])=$change[1]
 Expect-Error { Assert-SaveDialog $dialog } 'save_dialog_unsupported'
}
[VaultNative]::Foreground=[IntPtr]124
Expect-Error { Assert-SaveDialog (New-Dialog) } 'focus_changed'
[VaultNative]::Foreground=[IntPtr]123
$script:FakeProcess.Path='C:\Other\LINE.exe'
Expect-Error { Assert-SaveDialog (New-Dialog) } 'wrong_process'
$script:FakeProcess.Path=$script:ProcessPath
$script:FakeProcess.StartTime=[datetime]::new(124,[DateTimeKind]::Utc)
Expect-Error { Assert-SaveDialog (New-Dialog) } 'wrong_process'
$script:FakeProcess.StartTime=[datetime]::new(123,[DateTimeKind]::Utc)
$dest=Join-Path ([IO.Path]::GetTempPath()) ('chatatlas-guard-'+[Guid]::NewGuid().ToString('N')+'.bin')
$value=[pscustomobject]@{Current=[pscustomobject]@{Value=$dest}}
Assert-SaveDestination $value $dest
$value.Current.Value=$dest+'.changed'
Expect-Error { Assert-SaveDestination $value $dest } 'invalid_destination'
$value.Current.Value=$dest
try { [IO.File]::WriteAllText($dest,'synthetic'); Expect-Error { Assert-SaveDestination $value $dest } 'invalid_destination' } finally { if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest } }

# No UI automation is performed: synthetic identity/ownership guards only.
function Node-Key($e) { if ($e.PSObject.Properties['FakeKey']) { return $e.FakeKey }; return [string]$e.Current.NativeWindowHandle }
$dialog=New-Dialog
$confirmation=New-Dialog
$confirmation.Current.Name='Confirm Save As';$confirmation.Current.NativeWindowHandle=124
[VaultNative]::Foreground=[IntPtr]124
Assert-OwnedSaveConfirmation $confirmation $dialog '123' 123
Expect-Error { Assert-OwnedSaveConfirmation $confirmation $dialog 'wrong-runtime-id' 123 } 'save_dialog_unsupported'
Expect-Error { Assert-OwnedSaveConfirmation $confirmation $dialog '123' 999 } 'save_dialog_unsupported'
foreach ($change in @(@('Name','Open'),@('ClassName','NotADialog'),@('ProcessId',99),@('IsEnabled',$false),@('IsOffscreen',$true),@('NativeWindowHandle',0),@('NativeWindowHandle',123))) {
 $bad=New-Dialog;$bad.Current.Name='Confirm Save As';$bad.Current.NativeWindowHandle=124;$bad.Current.($change[0])=$change[1]
 Expect-Error { Assert-OwnedSaveConfirmation $bad $dialog '123' 123 } 'save_dialog_unsupported'
}
[VaultNative]::Owner=[IntPtr]999
Expect-Error { Assert-OwnedSaveConfirmation $confirmation $dialog '123' 123 } 'focus_changed'
[VaultNative]::Owner=[IntPtr]123;[VaultNative]::Foreground=[IntPtr]999
Expect-Error { Assert-OwnedSaveConfirmation $confirmation $dialog '123' 123 } 'focus_changed'
[VaultNative]::Foreground=[IntPtr]123

# Simulated lifecycle: only test doubles receive Close(), never any real window.
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
function Get-LineDialogs { return $script:FakeDialogs }
function Start-Sleep {
 param($Milliseconds)
 $script:PollTicks++
 & $script:OnSleep
 [Threading.Thread]::Sleep([Math]::Min($Milliseconds,20))
}
function Reset-Lifecycle {
 $script:ClosedConfirmation=0;$script:ClosedSave=0;$script:PollTicks=0
 $script:OnSleep={}
 $script:OnConfirmationClose={ $script:FakeDialogs=@($script:Dialog);[VaultNative]::Foreground=[IntPtr]123 }
 $script:OnSaveClose={}
 $script:Dialog=New-Dialog
 $script:Confirmation=New-Dialog
 $script:Confirmation.Current.Name='Confirm Save As';$script:Confirmation.Current.NativeWindowHandle=124
 $script:CloseConfirmation=[pscustomobject]@{}
 $script:CloseConfirmation | Add-Member ScriptMethod Close { $script:ClosedConfirmation++;& $script:OnConfirmationClose }
 $script:CloseSave=[pscustomobject]@{}
 $script:CloseSave | Add-Member ScriptMethod Close { $script:ClosedSave++;& $script:OnSaveClose }
 $script:Confirmation | Add-Member ScriptMethod GetCurrentPattern { param($pattern) return $script:CloseConfirmation }
 $script:Dialog | Add-Member ScriptMethod GetCurrentPattern { param($pattern) return $script:CloseSave }
 $script:FakeDialogs=@($script:Dialog,$script:Confirmation)
 $script:FakeProcess.Path=$script:ProcessPath
 $value.Current.Value=$dest
 [VaultNative]::Owner=[IntPtr]123;[VaultNative]::ParentOwner=[IntPtr]::Zero;[VaultNative]::Foreground=[IntPtr]124
}
function Expect-LifecycleError($expected,$confirmationCloses,$saveCloses) {
 Expect-Error { Wait-SaveCompletion $script:Dialog $value $dest '123' 123 } $expected
 if ($script:ClosedConfirmation -ne $confirmationCloses -or $script:ClosedSave -ne $saveCloses) { throw ('unexpected_close_count:'+ $expected) }
}
Reset-Lifecycle
Expect-LifecycleError 'save_conflict' 1 1
# Delayed close and transient zero focus; the original save window stays open
# until its tracked confirmation disappears AND focus actually returns.
foreach ($ticks in @(3,10)) {
 Reset-Lifecycle
 $script:DelayTicks=$ticks
 $script:OnConfirmationClose={}
 $script:OnSleep={
  if ($script:PollTicks -eq 1) { $script:FakeDialogs=@($script:Dialog);[VaultNative]::Foreground=[IntPtr]::Zero }
  if ($script:PollTicks -ge $script:DelayTicks) { [VaultNative]::Foreground=[IntPtr]123 }
  if ($script:ClosedSave) { throw 'closed_before_focus_returned' }
 }
 Expect-LifecycleError 'save_conflict' 1 1
 if ($script:PollTicks -lt $ticks) { throw 'did_not_wait_for_focus' }
}
Reset-Lifecycle
$script:OnConfirmationClose={ $script:FakeDialogs=@($script:Dialog);[VaultNative]::Foreground=[IntPtr]::Zero }
$watch=[Diagnostics.Stopwatch]::StartNew()
Expect-LifecycleError 'save_conflict_cleanup_required' 1 0
if ($watch.ElapsedMilliseconds -gt 3000) { throw 'cleanup_not_bounded' }
# A still-visible confirmation is also bounded, never blindly closed twice.
Reset-Lifecycle
$script:OnConfirmationClose={}
Expect-LifecycleError 'save_conflict_cleanup_required' 1 0
Reset-Lifecycle
$script:OnConfirmationClose={ $script:FakeDialogs=@($script:Dialog);[VaultNative]::Foreground=[IntPtr]999 }
Expect-LifecycleError 'focus_changed' 1 0
Reset-Lifecycle
$script:OnConfirmationClose={ $script:Dialog | Add-Member NoteProperty FakeKey 'replacement';$script:FakeDialogs=@($script:Dialog);[VaultNative]::Foreground=[IntPtr]123 }
Expect-LifecycleError 'save_dialog_unsupported' 1 0
Reset-Lifecycle
$script:OnConfirmationClose={ $script:FakeDialogs=@($script:Dialog);[VaultNative]::ParentOwner=[IntPtr]999;[VaultNative]::Foreground=[IntPtr]123 }
Expect-LifecycleError 'save_dialog_unsupported' 1 0
Reset-Lifecycle
$script:OnConfirmationClose={ $script:FakeDialogs=@($script:Dialog);$value.Current.Value=$dest+'.changed';[VaultNative]::Foreground=[IntPtr]123 }
Expect-LifecycleError 'invalid_destination' 1 0
Reset-Lifecycle
$script:OnConfirmationClose={ $script:FakeProcess.Path='C:\Other\LINE.exe' }
Expect-LifecycleError 'wrong_process' 1 0
Reset-Lifecycle
$script:OnConfirmationClose={ $script:Confirmation | Add-Member NoteProperty FakeKey 'replacement' }
Expect-LifecycleError 'save_dialog_unsupported' 1 0
Reset-Lifecycle
$script:OnConfirmationClose={ $other=New-Dialog;$other.Current.NativeWindowHandle=125;$script:FakeDialogs=@($script:Dialog,$other) }
Expect-LifecycleError 'save_dialog_unsupported' 1 0
# Cancellation/failure while polling must propagate, not be swallowed and close
# the parent later. Native Control cancellation terminates the worker process.
Reset-Lifecycle
$script:OnConfirmationClose={}
$script:OnSleep={ throw 'test_cancelled' }
Expect-LifecycleError 'test_cancelled' 1 0
Reset-Lifecycle
$script:OnConfirmationClose={ throw 'synthetic_close_failure' }
Expect-LifecycleError 'save_conflict_cleanup_required' 1 0
Reset-Lifecycle
$script:OnSaveClose={ throw 'synthetic_close_failure' }
Expect-LifecycleError 'save_conflict_cleanup_required' 1 1
Reset-Lifecycle
$script:Confirmation.Current.Name='Unrelated user dialog'
Expect-LifecycleError 'save_dialog_unsupported' 0 0
Reset-Lifecycle
[VaultNative]::Owner=[IntPtr]999
Expect-LifecycleError 'focus_changed' 0 0
Reset-Lifecycle
$script:FakeDialogs=@()
Wait-SaveCompletion $script:Dialog $value $dest '123' 123
if ($script:ClosedConfirmation -or $script:ClosedSave) { throw 'completed_save_was_closed' }
Write-Output 'Windows bridge guard tests passed; no real LINE UI was accessed.'
# Exercise the actual C++ bootstrap with a harmless worker, including a queued request.
$cpp=[IO.File]::ReadAllText((Join-Path $PSScriptRoot '../src/windows_source.cpp'))
$match=[regex]::Match($cpp,'const std::wstring bootstrap=L"([^"\r\n]+)";')
if (!$match.Success) { throw 'bootstrap_not_found' }
$bootstrap=$match.Groups[1].Value
# Exercise the production stream initialization, not a separately copied version.
$workerSource=[IO.File]::ReadAllText((Resolve-Path $source))
$preamble=$workerSource.Substring(0,$workerSource.IndexOf('Add-Type -AssemblyName UIAutomationClient'))
$testWorker=$preamble+@'
$first=[Console]::ReadLine()
$second=[Console]::ReadLine()
[Console]::WriteLine('received:'+ $first)
[Console]::WriteLine('unicode:'+ $second)
'@
$start=New-Object System.Diagnostics.ProcessStartInfo
$start.FileName=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$start.Arguments='-NoLogo -NoProfile -NonInteractive -Command "'+$bootstrap+'"'
$start.UseShellExecute=$false; $start.CreateNoWindow=$true
$start.RedirectStandardInput=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
$start.StandardOutputEncoding=[Text.UTF8Encoding]::new($false,$true)
$child=New-Object System.Diagnostics.Process; $child.StartInfo=$start
$started=$false
try {
 $started=$child.Start()
 if (!$started) { throw 'bootstrap_start_failed' }
 $stderr=$child.StandardError.ReadToEndAsync()
 $payload=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($testWorker))
 # Match native WriteFile exactly: inherited StreamWriter encoding/BOM is not
 # part of the C++ protocol. Send both records together to exercise buffering.
 # Build Chinese test data from code points, keeping this PS 5.1 test ASCII-safe.
 $unicodeRequest=([string][char]0x6e2c)+([char]0x8a66)+([char]0x4e2d)+([char]0x6587)
 $wire=[Text.Encoding]::UTF8.GetBytes($payload+"`n"+'queued-request'+"`n"+$unicodeRequest+"`n")
 $child.StandardInput.BaseStream.Write($wire,0,$wire.Length)
 $child.StandardInput.BaseStream.Flush()
 $line=$child.StandardOutput.ReadLineAsync()
 $completed=$line.Wait(10000)
 $received=if($completed){$line.Result}else{$null}
 if (!$completed -or $received -cne 'received:queued-request') {
  # Only this synthetic worker is run here. No real UI, paths, or chat data.
  if (!$child.HasExited) { $child.Kill(); [void]$child.WaitForExit(5000) }
  $errorText=if($stderr.Wait(5000)){$stderr.Result}else{'stderr timeout'}
  $exitCode=if($child.HasExited){$child.ExitCode}else{$null}
  Write-Output (@{test='bootstrap';completed=$completed;stdout=$received;stderr=$errorText;exit_code=$exitCode;writer_codepage=$child.StandardInput.Encoding.CodePage;writer_preamble=[Convert]::ToBase64String($child.StandardInput.Encoding.GetPreamble())} | ConvertTo-Json -Compress)
  throw 'bootstrap_roundtrip_failed'
 }
 $unicodeLine=$child.StandardOutput.ReadLineAsync()
 $unicodeCompleted=$unicodeLine.Wait(10000)
 $unicodeReceived=if($unicodeCompleted){$unicodeLine.Result}else{$null}
 if (!$unicodeCompleted -or $unicodeReceived -cne ('unicode:'+$unicodeRequest)) {
  Write-Output (@{test='bootstrap_unicode';completed=$unicodeCompleted;stdout=$unicodeReceived;expected=('unicode:'+$unicodeRequest)} | ConvertTo-Json -Compress)
  throw 'bootstrap_unicode_roundtrip_failed'
 }
 if (!$child.WaitForExit(5000) -or $child.ExitCode -ne 0) {
  if (!$child.HasExited) { $child.Kill(); [void]$child.WaitForExit(5000) }
  $errorText=if($stderr.Wait(5000)){$stderr.Result}else{'stderr timeout'}
  Write-Output (@{test='bootstrap_exit';stdout=$received;stderr=$errorText;exit_code=$child.ExitCode} | ConvertTo-Json -Compress)
  throw 'bootstrap_exit_failed'
 }
} finally { if ($started -and !$child.HasExited) { $child.Kill() }; $child.Dispose() }
Write-Output 'Native stdin bootstrap roundtrip passed without invoking LINE.'
