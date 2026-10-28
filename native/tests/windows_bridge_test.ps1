$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../../src/line_vault/windows/uia_worker.ps1'
$tokens=$null; $errors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path $source),[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
# Load only pure guards; never execute the worker or contact a real LINE process.
foreach ($name in @('Assert-Window','Assert-SaveDialog','Assert-SaveDestination','Assert-OwnedSaveConfirmation','Wait-SaveCompletion')) {
 $fn=$ast.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
 if (!$fn) { throw "Missing guard $name" }
 Invoke-Expression $fn.Extent.Text
}
Add-Type 'public static class VaultNative { public static System.IntPtr Foreground = new System.IntPtr(123); public static System.IntPtr Owner = new System.IntPtr(123); public static System.IntPtr GetWindow(System.IntPtr h, uint command) { return Owner; } public static System.IntPtr GetForegroundWindow() { return Foreground; } }'
$script:ProcessId=42; $script:ProcessPath='C:\Synthetic\LINE.exe'
$script:ProcessStarted=123
$script:Main=[pscustomobject]@{Current=[pscustomobject]@{ProcessId=42}}
$script:FakeProcess=[pscustomobject]@{ProcessName='LINE'; StartTime=[datetime]::new(123,[DateTimeKind]::Utc); Path=$script:ProcessPath}
function Get-Process { param($Id) return $script:FakeProcess }
$script:SaveDialogNames=@('Save As')
function New-Dialog { return [pscustomobject]@{Current=[pscustomobject]@{ProcessId=42; ClassName='#32770'; Name='Save As'; IsEnabled=$true; IsOffscreen=$false; NativeWindowHandle=123}} }
function Expect-Error($action,$expected) {
 try { & $action; throw 'test_did_not_fail' } catch { if ($_.Exception.Message -cne $expected) { throw } }
}
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
function Node-Key($e) { return [string]$e.Current.NativeWindowHandle }
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
$script:ClosedConfirmation=0;$script:ClosedSave=0
$closeConfirmation=[pscustomobject]@{}
$closeConfirmation | Add-Member ScriptMethod Close { $script:ClosedConfirmation++;[VaultNative]::Foreground=[IntPtr]123 }
$closeSave=[pscustomobject]@{}
$closeSave | Add-Member ScriptMethod Close { $script:ClosedSave++ }
$confirmation | Add-Member ScriptMethod GetCurrentPattern { param($pattern) return $closeConfirmation }
$dialog | Add-Member ScriptMethod GetCurrentPattern { param($pattern) return $closeSave }
$script:FakeDialogs=@($dialog,$confirmation)
function Get-LineDialogs { return $script:FakeDialogs }
[VaultNative]::Foreground=[IntPtr]124
Expect-Error { Wait-SaveCompletion $dialog $value $dest '123' 123 } 'save_conflict'
if ($script:ClosedConfirmation -ne 1 -or $script:ClosedSave -ne 1) { throw 'owned_confirmation_cancel_failed' }
$script:ClosedConfirmation=0;$script:ClosedSave=0
$confirmation.Current.Name='Unrelated user dialog'
[VaultNative]::Foreground=[IntPtr]124
Expect-Error { Wait-SaveCompletion $dialog $value $dest '123' 123 } 'save_dialog_unsupported'
if ($script:ClosedConfirmation -or $script:ClosedSave) { throw 'unrelated_dialog_was_closed' }
$confirmation.Current.Name='Confirm Save As'
[VaultNative]::Owner=[IntPtr]999
Expect-Error { Wait-SaveCompletion $dialog $value $dest '123' 123 } 'focus_changed'
if ($script:ClosedConfirmation -or $script:ClosedSave) { throw 'foreign_owner_was_closed' }
[VaultNative]::Owner=[IntPtr]123
$script:FakeDialogs=@()
Wait-SaveCompletion $dialog $value $dest '123' 123
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
