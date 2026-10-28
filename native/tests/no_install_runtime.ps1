param([Parameter(Mandatory=$true)][string]$Exe)
$ErrorActionPreference='Stop'
# Artificial data only. No tool installation, network setting, or real LINE access.
$original=(Resolve-Path -LiteralPath $Exe).Path
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('chatatlas-clean-smoke-'+[Guid]::NewGuid().ToString('N'))
$bin=Join-Path $fixture 'bin'; $archive=Join-Path $fixture 'archive'
[void][IO.Directory]::CreateDirectory($bin)
$copy=Join-Path $bin 'ChatAtlas.exe'
Copy-Item -LiteralPath $original -Destination $copy
function Invoke-Isolated([string]$mode,[bool]$expectSuccess=$true) {
 $start=New-Object Diagnostics.ProcessStartInfo
 $start.FileName=$copy; $start.Arguments=$mode+' "'+$archive+'"'; $start.WorkingDirectory=$bin
 $start.UseShellExecute=$false; $start.CreateNoWindow=$true
 $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
 $start.EnvironmentVariables.Clear()
 foreach($name in @('SystemRoot','WINDIR')) { $start.EnvironmentVariables[$name]=$env:SystemRoot }
 $start.EnvironmentVariables['TEMP']=$fixture; $start.EnvironmentVariables['TMP']=$fixture
 $start.EnvironmentVariables['PATH']=Join-Path $env:SystemRoot 'System32'
 $p=New-Object Diagnostics.Process; $p.StartInfo=$start
 try {
  if (!$p.Start()) { throw 'Native executable did not start' }
  $stdout=$p.StandardOutput.ReadToEndAsync(); $stderr=$p.StandardError.ReadToEndAsync()
  if (!$p.WaitForExit(120000)) { $p.Kill(); throw 'Native isolated run timed out' }
  if (($p.ExitCode -eq 0) -ne $expectSuccess) { throw ('Native isolated run unexpected exit: '+$p.ExitCode) }
  [void]$stdout.Wait(); [void]$stderr.Wait()
  return $stdout.Result
 } finally { $p.Dispose() }
}
function Test-IsolatedGui {
 # Run only on an ephemeral CI runner. GUI startup may create ordinary per-user
 # preferences in that runner's LocalAppData; these are not installed packages.
 Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class AtlasSmokeWindow {
 [DllImport("user32.dll", CharSet=CharSet.Unicode)]
 public static extern int GetClassName(IntPtr hwnd, StringBuilder value, int count);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect value);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
 [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)]
 public static extern IntPtr SetControlText(IntPtr hwnd, uint message, IntPtr wParam, string text);
 [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)]
 public static extern IntPtr ReadControlText(IntPtr hwnd, uint message, IntPtr wParam, StringBuilder text);
 [DllImport("user32.dll")] public static extern bool RedrawWindow(IntPtr hwnd, IntPtr rect, IntPtr region, uint flags);
}
'@
 $start=New-Object Diagnostics.ProcessStartInfo
 $start.FileName=$copy; $start.WorkingDirectory=$bin; $start.UseShellExecute=$false
 $start.EnvironmentVariables.Clear()
 foreach($name in @('SystemRoot','WINDIR')) { $start.EnvironmentVariables[$name]=$env:SystemRoot }
 $start.EnvironmentVariables['TEMP']=$fixture; $start.EnvironmentVariables['TMP']=$fixture
 $start.EnvironmentVariables['PATH']=Join-Path $env:SystemRoot 'System32'
 $p=New-Object Diagnostics.Process; $p.StartInfo=$start; $started=$false
 try {
  if (!$p.Start()) { throw 'Native GUI did not start' }; $started=$true
  # Samples cannot capture every short-lived child; they are an extra regression,
  # not exhaustive process or network telemetry.
  if (@(Get-CimInstance Win32_Process -Filter ('ParentProcessId = '+$p.Id)).Count) { throw 'Unexpected child process during GUI startup' }
  if (!$p.WaitForInputIdle(20000)) { throw 'GUI input queue never became idle' }
  $deadline=[DateTime]::UtcNow.AddSeconds(20)
  do {
   $p.Refresh()
   if ($p.HasExited) { throw 'Native GUI exited before showing its main window' }
   if ($p.MainWindowHandle -ne [IntPtr]::Zero) { break }
   Start-Sleep -Milliseconds 100
  } while ([DateTime]::UtcNow -lt $deadline)
  if ($p.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Runner did not expose a native GUI window; GUI test failed, not skipped' }
  $name=New-Object Text.StringBuilder 256
  [void][AtlasSmokeWindow]::GetClassName($p.MainWindowHandle,$name,$name.Capacity)
  if ($name.ToString() -cne 'ChatAtlasNativeWindow' -or [string]::IsNullOrWhiteSpace($p.MainWindowTitle)) { throw 'Expected ChatAtlas main window was not shown' }
  foreach ($id in @(100,101,105,107,110,115,116)) {
   if ([AtlasSmokeWindow]::GetDlgItem($p.MainWindowHandle,$id) -eq [IntPtr]::Zero) { throw ('Native GUI initialization incomplete: control '+$id) }
  }
  Start-Sleep -Milliseconds 1000; $p.Refresh()
  if ($p.HasExited) { throw 'Native GUI did not remain alive' }
  if (@(Get-CimInstance Win32_Process -Filter ('ParentProcessId = '+$p.Id)).Count) { throw 'Unexpected child process while native GUI was idle' }
  if ($env:ATLAS_QA) {
   # Capture this process's real HWND only. Never copy the desktop or other apps.
   [uint32]$owner=0
   [void][AtlasSmokeWindow]::GetWindowThreadProcessId($p.MainWindowHandle,[ref]$owner)
   if ($owner -ne $p.Id) { throw 'Screenshot window ownership changed' }
   $folder=[AtlasSmokeWindow]::GetDlgItem($p.MainWindowHandle,105)
   [uint32]$folderOwner=0
   [void][AtlasSmokeWindow]::GetWindowThreadProcessId($folder,[ref]$folderOwner)
   if ($folder -eq [IntPtr]::Zero -or $folderOwner -ne $p.Id) { throw 'Screenshot input ownership changed' }
   $neutral='C:\ChatAtlas-Example'
   # WM_SETTEXT is marshalled for the edit in our separate process. SetWindowText
   # does not reliably update another process's edit controls.
   if ([AtlasSmokeWindow]::SetControlText($folder,0x000C,[IntPtr]::Zero,$neutral) -eq [IntPtr]::Zero) { throw 'Could not set neutral screenshot input' }
   $visible=New-Object Text.StringBuilder 256
   [void][AtlasSmokeWindow]::ReadControlText($folder,0x000D,[IntPtr]$visible.Capacity,$visible)
   if ($visible.ToString() -cne $neutral) { throw 'Neutral screenshot input readback mismatch' }
   # This is an input example, not a claim that this archive exists or was saved.
   [void][AtlasSmokeWindow]::RedrawWindow($p.MainWindowHandle,[IntPtr]::Zero,[IntPtr]::Zero,0x0185)
   Start-Sleep -Milliseconds 500
   [void]$visible.Clear()
   [void][AtlasSmokeWindow]::ReadControlText($folder,0x000D,[IntPtr]$visible.Capacity,$visible)
   if ($visible.ToString() -cne $neutral) { throw 'Neutral screenshot input changed before capture' }
   $rect=New-Object 'AtlasSmokeWindow+Rect'
   if (![AtlasSmokeWindow]::GetWindowRect($p.MainWindowHandle,[ref]$rect)) { throw 'Native screenshot bounds unavailable' }
   $width=$rect.Right-$rect.Left; $height=$rect.Bottom-$rect.Top
   if ($width -lt 600 -or $height -lt 400 -or $width -gt 4096 -or $height -gt 4096) { throw 'Unexpected native screenshot dimensions' }
   Add-Type -AssemblyName System.Drawing
   $bitmap=New-Object -TypeName System.Drawing.Bitmap -ArgumentList @([int]$width,[int]$height)
   $graphics=[Drawing.Graphics]::FromImage($bitmap)
   try {
    $graphics.Clear([Drawing.Color]::Magenta)
    $hdc=$graphics.GetHdc()
    try { $rendered=[AtlasSmokeWindow]::PrintWindow($p.MainWindowHandle,$hdc,0) } finally { $graphics.ReleaseHdc($hdc) }
    if (!$rendered) { throw 'PrintWindow failed; no substitute screenshot created' }
    $colors=New-Object 'System.Collections.Generic.HashSet[int]'
    for($y=0;$y -lt $height;$y+=7) { for($x=0;$x -lt $width;$x+=7) { [void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb()) } }
    if ($colors.Count -lt 8) { throw 'Native screenshot is blank or insufficiently rendered' }
    [void][IO.Directory]::CreateDirectory($env:ATLAS_QA)
    $image=Join-Path $env:ATLAS_QA 'atlas-native-windows.png'
    $bitmap.Save($image,[Drawing.Imaging.ImageFormat]::Png)
    if ((Get-Item -LiteralPath $image).Length -lt 4096) { throw 'Native screenshot file is unexpectedly empty' }
    Write-Output ('Captured actual ChatAtlas startup HWND: '+$width+'x'+$height+'; synthetic input path only.')
   } finally { $graphics.Dispose(); $bitmap.Dispose() }
  }
  if (!$p.CloseMainWindow()) { throw 'Native GUI refused graceful close' }
  if (!$p.WaitForExit(10000) -or $p.ExitCode -ne 0) { throw 'Native GUI did not close cleanly' }
  Write-Output 'Native GUI main HWND/title, required controls, idle survival and graceful close passed under isolated developer environment.'
 } finally {
  if ($started -and !$p.HasExited) { $p.Kill(); [void]$p.WaitForExit(5000) }
  $p.Dispose()
 }
}
try {
 [void](Invoke-Isolated '--check' $false)
 if (Test-Path -LiteralPath $archive) { throw 'Check created an absent archive' }
 $result=Invoke-Isolated '--demo'
 if (!(Test-Path -LiteralPath (Join-Path $archive 'index.html'))) { throw 'Synthetic HTML missing' }
 $check=Invoke-Isolated '--check'
 if ($check.Trim() -cne '[]') { throw 'Synthetic archive integrity failed' }
 if (@(Get-ChildItem -LiteralPath $bin -Force).Count -ne 1) { throw 'Unexpected runtime dependency in isolated binary folder' }
 Write-Output 'Single native EXE demo/check passed with only Windows System32 in PATH and no developer environment variables.'
 Test-IsolatedGui
 Write-Output 'This hosted-runner smoke test is not a clean OS, full GUI interaction, LINE, or exhaustive network proof.'
} finally {
 if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
