param([Parameter(Mandatory=$true)][string]$Exe,[switch]$Portable)
$ErrorActionPreference='Stop'
$portableMode=$Portable.IsPresent -or $env:ATLAS_PORTABLE -eq '1'
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
function Test-IsolatedGui([string]$ExpectedFolder='', [switch]$SkipCapture) {
 # Legacy alpha.2 capture remains supported without -Portable.
 if (-not ('AtlasSmokeWindow' -as [type])) {
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
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr hwnd);
 [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW")]
 private static extern IntPtr SendMessageTimeout(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp, uint flags, uint timeout, out UIntPtr result);
 public static long ControlMessage(IntPtr hwnd, uint msg, long wp, long lp) {
  UIntPtr result;
  if (SendMessageTimeout(hwnd,msg,new IntPtr(wp),new IntPtr(lp),2,3000,out result)==IntPtr.Zero) throw new InvalidOperationException("Owned control did not respond");
  return unchecked((long)result.ToUInt64());
 }
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
 [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)]
 public static extern IntPtr SetControlText(IntPtr hwnd, uint message, IntPtr wParam, string text);
 [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)]
 public static extern IntPtr ReadControlText(IntPtr hwnd, uint message, IntPtr wParam, StringBuilder text);
 [DllImport("user32.dll")] public static extern bool RedrawWindow(IntPtr hwnd, IntPtr rect, IntPtr region, uint flags);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
 private delegate bool EnumWindowCallback(IntPtr hwnd, IntPtr data);
 [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowCallback callback, IntPtr data);
 public static IntPtr[] WindowsForProcess(uint processId) {
  var result = new System.Collections.Generic.List<IntPtr>();
  EnumWindows((hwnd, data) => { uint owner; GetWindowThreadProcessId(hwnd, out owner); if(owner == processId) result.Add(hwnd); return true; }, IntPtr.Zero);
  return result.ToArray();
 }
}
'@
 }
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
  if ($portableMode) {
   $data=Join-Path $bin 'ChatAtlasData'
   if (!(Test-Path -LiteralPath $data -PathType Container)) { throw 'Portable data folder missing beside EXE' }
   if ((Get-Item -LiteralPath $data).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Portable data folder is a link' }
   if (!$ExpectedFolder) { $ExpectedFolder=Join-Path $data 'Collections' }
   $folderText=New-Object Text.StringBuilder 32768
   [void][AtlasSmokeWindow]::ReadControlText([AtlasSmokeWindow]::GetDlgItem($p.MainWindowHandle,105),0x000D,[IntPtr]$folderText.Capacity,$folderText)
   if ([IO.Path]::GetFullPath($folderText.ToString()) -ine [IO.Path]::GetFullPath($ExpectedFolder)) { throw 'Portable default or moved preference points outside expected EXE data path' }
  }
  if ($env:ATLAS_QA -and !$SkipCapture) {
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
   function Save-OwnedWindowCapture([string]$FileName) {
    [uint32]$captureOwner=0
    [void][AtlasSmokeWindow]::GetWindowThreadProcessId($p.MainWindowHandle,[ref]$captureOwner)
    if ($captureOwner -ne $p.Id) { throw 'Capture window ownership changed' }
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
    $image=Join-Path $env:ATLAS_QA $FileName
    $bitmap.Save($image,[Drawing.Imaging.ImageFormat]::Png)
    if ((Get-Item -LiteralPath $image).Length -lt 4096) { throw 'Native screenshot file is unexpectedly empty' }
    Write-Output ('Captured actual ChatAtlas HWND: '+$FileName+'; '+$width+'x'+$height+'; artificial data only.')
   } finally { $graphics.Dispose(); $bitmap.Dispose() }
   }
   Save-OwnedWindowCapture 'atlas-native-windows.png'
   if ($portableMode) {
   # Use the real --demo archive through its actual relative path. No runner
   # username/path, fabricated list rows, or real LINE data enters the image.
   if ([IO.Path]::GetFullPath((Join-Path $bin '..\archive')) -ine [IO.Path]::GetFullPath($archive)) { throw 'Synthetic archive relative path mismatch' }
   if ([AtlasSmokeWindow]::SetControlText($folder,0x000C,[IntPtr]::Zero,'..\archive') -eq [IntPtr]::Zero) { throw 'Could not select artificial archive' }
   $tabs=[AtlasSmokeWindow]::GetDlgItem($p.MainWindowHandle,100)
   $history=[AtlasSmokeWindow]::GetDlgItem($p.MainWindowHandle,124)
   $refresh=[AtlasSmokeWindow]::GetDlgItem($p.MainWindowHandle,125)
   foreach($control in @($tabs,$history,$refresh)) {
    [uint32]$controlOwner=0
    [void][AtlasSmokeWindow]::GetWindowThreadProcessId($control,[ref]$controlOwner)
    if ($control -eq [IntPtr]::Zero -or $controlOwner -ne $p.Id) { throw 'History control ownership mismatch' }
   }
   # TCM_SETCURFOCUS on a normal tab control changes its selection and sends
   # the real selection notification; do not forge a cross-process NMHDR.
   [void][AtlasSmokeWindow]::ControlMessage($tabs,0x1330,3,0)
   if ([AtlasSmokeWindow]::ControlMessage($tabs,0x130B,0,0) -ne 3 -or ![AtlasSmokeWindow]::IsWindowVisible($history)) { throw 'History tab did not activate' }
   if (![AtlasSmokeWindow]::IsWindowVisible($refresh) -or ![AtlasSmokeWindow]::IsWindowEnabled($refresh)) { throw 'History refresh is unavailable' }
   [void][AtlasSmokeWindow]::ControlMessage($refresh,0x00F5,0,0) # BM_CLICK
   $deadline=[DateTime]::UtcNow.AddSeconds(30); $rowCount=0
   do {
    $p.Refresh()
    if ($p.HasExited) { throw 'Native GUI exited while loading history' }
    $rowCount=[AtlasSmokeWindow]::ControlMessage($history,0x1004,0,0) # LVM_GETITEMCOUNT, no pointers
    if ($rowCount -gt 0 -and [AtlasSmokeWindow]::IsWindowEnabled($refresh)) { break }
    Start-Sleep -Milliseconds 100
   } while ([DateTime]::UtcNow -lt $deadline)
   if ($rowCount -lt 1 -or ![AtlasSmokeWindow]::IsWindowEnabled($refresh)) { throw 'Artificial capture history did not populate before deadline' }
   [void]$visible.Clear()
   [void][AtlasSmokeWindow]::ReadControlText($folder,0x000D,[IntPtr]$visible.Capacity,$visible)
   if ($visible.ToString() -cne '..\archive') { throw 'History screenshot archive input changed unexpectedly' }
   [void][AtlasSmokeWindow]::RedrawWindow($p.MainWindowHandle,[IntPtr]::Zero,[IntPtr]::Zero,0x0185)
   Start-Sleep -Milliseconds 500
   Save-OwnedWindowCapture 'atlas-native-history-windows.png'
   Write-Output ('Real packaged EXE history tab loaded '+$rowCount+' artificial capture records.')
   }
  }
  if (!$p.CloseMainWindow()) { throw 'Native GUI refused graceful close' }
  if (!$p.WaitForExit(10000) -or $p.ExitCode -ne 0) { throw 'Native GUI did not close cleanly' }
  Write-Output 'Native GUI main HWND/title, required controls, idle survival and graceful close passed under isolated developer environment.'
 } finally {
  if ($started -and !$p.HasExited) { $p.Kill(); [void]$p.WaitForExit(5000) }
  $p.Dispose()
 }
}
function Get-LegacyAppDataSnapshot {
 $old=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'ChatAtlas'
 if (!(Test-Path -LiteralPath $old)) { return 'absent' }
 $records=New-Object 'System.Collections.Generic.List[string]'
 function Visit([string]$dir) {
  foreach($item in Get-ChildItem -LiteralPath $dir -Force) {
   if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Cannot safely snapshot linked legacy data in CI fixture' }
   $relative=$item.FullName.Substring($old.Length)
   if($item.PSIsContainer) { $records.Add('D|'+$relative); Visit $item.FullName }
   else { $records.Add('F|'+$relative+'|'+(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash+'|'+$item.LastWriteTimeUtc.Ticks) }
  }
 }
 Visit $old
 return 'present|'+(Get-Item -LiteralPath $old).LastWriteTimeUtc.Ticks+'|'+(($records | Sort-Object) -join "`n")
}
function Test-PortableReadOnly {
 # Only a newly created test fixture receives a temporary deny-write ACE.
 # Never change the source EXE directory, AppData, or any existing user folder.
 $readOnly=Join-Path $fixture 'read-only-portable'
 [void][IO.Directory]::CreateDirectory($readOnly)
 $executable=Join-Path $readOnly 'ChatAtlas.exe'; Copy-Item -LiteralPath $original -Destination $executable
 $before=Get-Acl -LiteralPath $readOnly
 $locked=Get-Acl -LiteralPath $readOnly
 $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User
 $rule=[Security.AccessControl.FileSystemAccessRule]::new($sid,[Security.AccessControl.FileSystemRights]::Write,([Security.AccessControl.InheritanceFlags]::ContainerInherit -bor [Security.AccessControl.InheritanceFlags]::ObjectInherit),[Security.AccessControl.PropagationFlags]::None,[Security.AccessControl.AccessControlType]::Deny)
 $locked.AddAccessRule($rule)
 $process=$null; $started=$false
 try {
  Set-Acl -LiteralPath $readOnly -AclObject $locked
  $probe=Join-Path $readOnly 'must-not-write.tmp'; $denied=$false
  try { [IO.File]::WriteAllText($probe,'synthetic') } catch {
   if ($_.Exception -is [UnauthorizedAccessException] -or $_.Exception.InnerException -is [UnauthorizedAccessException]) { $denied=$true } else { throw }
  }
  if (!$denied) { throw 'Read-only fixture did not actually deny writes' }
  $start=New-Object Diagnostics.ProcessStartInfo
  $start.FileName=$executable; $start.WorkingDirectory=$readOnly; $start.UseShellExecute=$false
  $start.EnvironmentVariables.Clear()
  foreach($name in @('SystemRoot','WINDIR')) { $start.EnvironmentVariables[$name]=$env:SystemRoot }
  $start.EnvironmentVariables['TEMP']=$fixture; $start.EnvironmentVariables['TMP']=$fixture
  $start.EnvironmentVariables['PATH']=Join-Path $env:SystemRoot 'System32'
  $process=New-Object Diagnostics.Process; $process.StartInfo=$start
  if (!$process.Start()) { throw 'Read-only portable fixture did not start' }; $started=$true
  $deadline=[DateTime]::UtcNow.AddSeconds(30)
  while (!$process.HasExited -and [DateTime]::UtcNow -lt $deadline) {
   foreach($window in [AtlasSmokeWindow]::WindowsForProcess([uint32]$process.Id)) {
    $class=New-Object Text.StringBuilder 256
    [void][AtlasSmokeWindow]::GetClassName($window,$class,$class.Capacity)
    if($class.ToString() -ceq '#32770') { [void][AtlasSmokeWindow]::PostMessage($window,0x0010,[IntPtr]::Zero,[IntPtr]::Zero) }
   }
   Start-Sleep -Milliseconds 100; $process.Refresh()
  }
  if (!$process.HasExited) { throw 'Read-only portable startup did not fail closed' }
  if ($process.ExitCode -eq 0) { throw 'Read-only portable startup incorrectly succeeded' }
  if(Test-Path -LiteralPath (Join-Path $readOnly 'ChatAtlasData')) { throw 'Read-only portable startup created a data directory' }
  Write-Output 'Read-only owned portable folder failed without creating data or selecting a fallback.'
 } finally {
  if ($started -and !$process.HasExited) { $process.Kill(); [void]$process.WaitForExit(5000) }
  if ($process) { $process.Dispose() }
  Set-Acl -LiteralPath $readOnly -AclObject $before
 }
}
try {
 if ($portableMode) { $legacyBefore=Get-LegacyAppDataSnapshot }
 [void](Invoke-Isolated '--check' $false)
 if (Test-Path -LiteralPath $archive) { throw 'Check created an absent archive' }
 $result=Invoke-Isolated '--demo'
 if (!(Test-Path -LiteralPath (Join-Path $archive 'index.html'))) { throw 'Synthetic HTML missing' }
 $check=Invoke-Isolated '--check'
 if ($check.Trim() -cne '[]') { throw 'Synthetic archive integrity failed' }
 if (@(Get-ChildItem -LiteralPath $bin -Force).Count -ne 1) { throw 'Unexpected runtime dependency in isolated binary folder' }
 Write-Output 'Single native EXE demo/check passed with only Windows System32 in PATH and no developer environment variables.'
 Test-IsolatedGui
 if ($portableMode) {
  $data=Join-Path $bin 'ChatAtlasData'
  $unexpected=@(Get-ChildItem -LiteralPath $bin -Force | Where-Object { $_.Name -notin @('ChatAtlas.exe','ChatAtlasData') })
  if ($unexpected.Count) { throw 'Unexpected files beside portable EXE' }
  $preferences=Join-Path $data 'preferences.local.json'
  [IO.File]::WriteAllText($preferences,'{"last_archive":"ChatAtlasData/Collections/Moved-synthetic","archive_base":"portable"}',(New-Object Text.UTF8Encoding($false)))
  $moved=Join-Path $fixture 'moved-portable'
  Move-Item -LiteralPath $bin -Destination $moved
  $bin=$moved; $copy=Join-Path $bin 'ChatAtlas.exe'
  Test-IsolatedGui -ExpectedFolder (Join-Path $bin 'ChatAtlasData/Collections/Moved-synthetic') -SkipCapture
  Test-PortableReadOnly
  if ((Get-LegacyAppDataSnapshot) -cne $legacyBefore) { throw 'Portable execution changed legacy LocalAppData ChatAtlas data' }
  Write-Output 'Portable beside-EXE data, move/reopen preferences and unchanged legacy AppData passed.'
 }
 Write-Output 'This hosted-runner smoke test is not a clean OS, full GUI interaction, LINE, or exhaustive network proof.'
} finally {
 if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
