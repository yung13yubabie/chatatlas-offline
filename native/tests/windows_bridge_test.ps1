$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../../src/line_vault/windows/uia_worker.ps1'
$tokens=$null; $errors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path $source),[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
# Load only pure guards; never execute the worker or contact a real LINE process.
foreach ($name in @('Assert-Window','Assert-SaveDialog','Assert-SaveDestination')) {
 $fn=$ast.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
 if (!$fn) { throw "Missing guard $name" }
 Invoke-Expression $fn.Extent.Text
}
Add-Type 'public static class VaultNative { public static System.IntPtr Foreground = new System.IntPtr(123); public static System.IntPtr GetForegroundWindow() { return Foreground; } }'
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
Write-Output 'Windows bridge guard tests passed; no real LINE UI was accessed.'
# Exercise the actual C++ bootstrap with a harmless worker, including a queued request.
$cpp=[IO.File]::ReadAllText((Join-Path $PSScriptRoot '../src/windows_source.cpp'))
$match=[regex]::Match($cpp,'const std::wstring bootstrap=L"([^"\r\n]+)";')
if (!$match.Success) { throw 'bootstrap_not_found' }
$bootstrap=$match.Groups[1].Value
$testWorker=@'
if ([Console]::InputEncoding.CodePage -ne 65001) { [Console]::InputEncoding = New-Object System.Text.UTF8Encoding($false) }
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$first=[Console]::ReadLine()
[Console]::WriteLine('received:'+ $first)
'@
$start=New-Object System.Diagnostics.ProcessStartInfo
$start.FileName=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$start.Arguments='-NoLogo -NoProfile -NonInteractive -Command "'+$bootstrap+'"'
$start.UseShellExecute=$false; $start.CreateNoWindow=$true
$start.RedirectStandardInput=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
$child=New-Object System.Diagnostics.Process; $child.StartInfo=$start
$started=$false
try {
 $started=$child.Start()
 if (!$started) { throw 'bootstrap_start_failed' }
 $stderr=$child.StandardError.ReadToEndAsync()
 $payload=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($testWorker))
 # Match native WriteFile exactly: inherited StreamWriter encoding/BOM is not
 # part of the C++ protocol. Send both records together to exercise buffering.
 $wire=[Text.Encoding]::UTF8.GetBytes($payload+"`n"+'queued-request'+"`n")
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
 if (!$child.WaitForExit(5000) -or $child.ExitCode -ne 0) {
  if (!$child.HasExited) { $child.Kill(); [void]$child.WaitForExit(5000) }
  $errorText=if($stderr.Wait(5000)){$stderr.Result}else{'stderr timeout'}
  Write-Output (@{test='bootstrap_exit';stdout=$received;stderr=$errorText;exit_code=$child.ExitCode} | ConvertTo-Json -Compress)
  throw 'bootstrap_exit_failed'
 }
} finally { if ($started -and !$child.HasExited) { $child.Kill() }; $child.Dispose() }
Write-Output 'Native stdin bootstrap roundtrip passed without invoking LINE.'
