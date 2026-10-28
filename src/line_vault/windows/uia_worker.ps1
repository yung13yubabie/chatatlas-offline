$ErrorActionPreference = 'Stop'
# Preserve the bootstrap's buffered stdin reader if it already uses UTF-8.
if ([Console]::InputEncoding.CodePage -ne 65001) { [Console]::InputEncoding = New-Object System.Text.UTF8Encoding($false) }
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class VaultNative {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h, int command);
}
'@
$script:ProcessId = 0
$script:ProcessStarted = 0
$script:ProcessPath = $null
$script:SaveDialogNames = @('Save As','Save as','另存新檔','另存為','另存为','名前を付けて保存')
$script:Main = $null
$script:Cache = @{}
$script:Stage = $null
$script:AllowedNames = @('下載','儲存','另存新檔','另存','Download','Save','Save as','Save As','保存','ダウンロード','保存する')

function Node-Key($e) { return (($e.GetRuntimeId() | ForEach-Object { [string]$_ }) -join '.') }
function Node-Object($e) {
 $c=$e.Current
 $id=Node-Key $e
 $script:Cache[$id]=$e
 return @{ token=$id; name=$c.Name; automation_id=$c.AutomationId;
  control_type=($c.ControlType.ProgrammaticName -replace '^ControlType\.','');
  class_name=$c.ClassName; enabled=$c.IsEnabled; offscreen=$c.IsOffscreen }
}
function Assert-Window {
 if ($null -eq $script:Main) { throw 'not_connected' }
 $p=Get-Process -Id $script:ProcessId
 if ($p.ProcessName -cne 'LINE' -or $p.StartTime.ToUniversalTime().Ticks -ne $script:ProcessStarted -or $p.Path -ine $script:ProcessPath) { throw 'wrong_process' }
 if ($script:Main.Current.ProcessId -ne $script:ProcessId) { throw 'wrong_window' }
}
function Assert-Foreground {
 Assert-Window
 [uint32]$pid0=0
 [void][VaultNative]::GetWindowThreadProcessId([VaultNative]::GetForegroundWindow(),[ref]$pid0)
 if ($pid0 -ne $script:ProcessId) { throw 'focus_changed' }
}
function Assert-SaveDialog($dialog) {
 Assert-Window
 $c=$dialog.Current
 if ($c.ProcessId -ne $script:ProcessId -or $c.ClassName -cne '#32770' -or $c.Name -cnotin $script:SaveDialogNames -or !$c.IsEnabled -or $c.IsOffscreen -or $c.NativeWindowHandle -eq 0) { throw 'save_dialog_unsupported' }
 if ([VaultNative]::GetForegroundWindow() -ne [IntPtr]$c.NativeWindowHandle) { throw 'focus_changed' }
}
function Assert-SaveDestination($valuePattern,$dest) {
 if ($valuePattern.Current.Value -cne $dest -or (Test-Path -LiteralPath $dest)) { throw 'invalid_destination' }
}
function Get-LineDialogs {
 $wins=[System.Windows.Automation.AutomationElement]::RootElement.FindAll([System.Windows.Automation.TreeScope]::Children,[System.Windows.Automation.Condition]::TrueCondition)
 return @($wins | Where-Object { $_.Current.ProcessId -eq $script:ProcessId -and $_.Current.ClassName -eq '#32770' })
}
function Resolve-Node($token) {
 Assert-Window
 if (!$script:Cache.ContainsKey([string]$token)) { throw 'stale_node' }
 $e=$script:Cache[[string]$token]
 if ($e.Current.ProcessId -ne $script:ProcessId) { throw 'wrong_process' }
 return $e
}
function Match-Node($e,$selector) {
 $c=$e.Current
 if ($selector.automation_id -and $c.AutomationId -cne [string]$selector.automation_id) { return $false }
 if ($selector.name -and $c.Name -cne [string]$selector.name) { return $false }
 if ($selector.control_type -and ($c.ControlType.ProgrammaticName -replace '^ControlType\.','') -ne [string]$selector.control_type) { return $false }
 return $true
}
function Find-Nodes($root,$selector,$max=6000) {
 $queue=New-Object System.Collections.Queue
 $queue.Enqueue($root)
 $found=New-Object System.Collections.ArrayList
 $walker=[System.Windows.Automation.TreeWalker]::ControlViewWalker
 $count=0
 while ($queue.Count -gt 0) {
  $e=$queue.Dequeue();$count++
  if ($count -gt $max) { throw 'tree_limit' }
  if (Match-Node $e $selector) { [void]$found.Add($e) }
  $child=$walker.GetFirstChild($e)
  while ($null -ne $child) { $queue.Enqueue($child);$child=$walker.GetNextSibling($child) }
 }
 return ,$found.ToArray()
}

while ($null -ne ($line=[Console]::ReadLine())) {
 try {
  if ($line.Length -gt 131072) { throw 'request_limit' }
  $r=ConvertFrom-Json $line
  $result=$null
  switch ($r.action) {
   'connect' {
    $processes=@(Get-Process -Name LINE -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 })
    if ($processes.Count -ne 1) { throw 'line_window_ambiguous' }
    $p=$processes[0]
    $signature=Get-AuthenticodeSignature -FilePath $p.Path
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(?:^|,\s*)O="?(?:LINE Corporation|LY Corporation|NAVER Corporation)"?(?:,|$)') { throw 'publisher_unverified' }
    $script:ProcessId=$p.Id
    $script:ProcessStarted=$p.StartTime.ToUniversalTime().Ticks
    $script:ProcessPath=$p.Path
    $script:Main=[System.Windows.Automation.AutomationElement]::FromHandle($p.MainWindowHandle)
    if ($script:Main.Current.ClassName -eq '#32770') { throw 'wrong_window' }
    $result=@{version=$p.FileVersionInfo.FileVersion; root=(Node-Object $script:Main); publisher_verified=$true}
   }
   'activate' {
    Assert-Window
    $p=Get-Process -Id $script:ProcessId
    [void][VaultNative]::ShowWindowAsync($p.MainWindowHandle,9)
    [void][VaultNative]::SetForegroundWindow($p.MainWindowHandle)
    $deadline=[DateTime]::UtcNow.AddSeconds(3)
    do {
     [uint32]$pid0=0
     [void][VaultNative]::GetWindowThreadProcessId([VaultNative]::GetForegroundWindow(),[ref]$pid0)
     if ($pid0 -eq $script:ProcessId) { break }
     Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    Assert-Foreground
    $result=$true
   }
   'inspect' {
    Assert-Window
    # Return counts only: never titles, text, AutomationIds, paths or tokens.
    $nodes=Find-Nodes $script:Main @{}
    $types=@{};$patterns=@{};$ids=0
    foreach ($e in $nodes) {
     $type=$e.Current.ControlType.ProgrammaticName -replace '^ControlType\.',''
     if (!$types.ContainsKey($type)) {$types[$type]=0};$types[$type]++
     if ($e.Current.AutomationId) {$ids++}
     foreach ($p in $e.GetSupportedPatterns()) {
      $name=$p.ProgrammaticName -replace '^.*\.',''
      if (!$patterns.ContainsKey($name)) {$patterns[$name]=0};$patterns[$name]++
     }
    }
    $result=@{node_count=$nodes.Count;control_types=$types;patterns=$patterns;nodes_with_automation_id=$ids}
   }
   'find' {
    Assert-Window
    $root=if($r.root){ Resolve-Node $r.root }else{$script:Main}
    $nodes=Find-Nodes $root $r.selector
    $result=@($nodes | ForEach-Object { Node-Object $_ })
   }
   'text' {
    $e=Resolve-Node $r.token
    $pattern=$null
    if ($e.TryGetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern,[ref]$pattern)) {
     $result=$pattern.DocumentRange.GetText(262144)
    } elseif ($e.TryGetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern,[ref]$pattern)) {
     $result=$pattern.Current.Value
    } else { $result=$e.Current.Name }
   }
   'select' {
    Assert-Foreground
    $e=Resolve-Node $r.token;$pattern=$null
    if (!$e.TryGetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern,[ref]$pattern)) { throw 'selection_unsupported' }
    $pattern.Select();$result=$true
   }
   'scroll_up' {
    Assert-Foreground
    $e=Resolve-Node $r.token;$pattern=$null
    if (!$e.TryGetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern,[ref]$pattern)) { throw 'scroll_unsupported' }
    if (!$pattern.Current.VerticallyScrollable -or $pattern.Current.VerticalScrollPercent -eq 0) { $result=$false }
    else { $pattern.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount,[System.Windows.Automation.ScrollAmount]::LargeDecrement);$result=$true }
   }
   'scroll_down' {
    Assert-Foreground
    $e=Resolve-Node $r.token;$pattern=$null
    if (!$e.TryGetCurrentPattern([System.Windows.Automation.ScrollPattern]::Pattern,[ref]$pattern)) { throw 'scroll_unsupported' }
    if (!$pattern.Current.VerticallyScrollable -or $pattern.Current.VerticalScrollPercent -eq 100) { $result=$false }
    else { $pattern.Scroll([System.Windows.Automation.ScrollAmount]::NoAmount,[System.Windows.Automation.ScrollAmount]::LargeIncrement);$result=$true }
   }
   'invoke_navigation' {
    Assert-Foreground
    $e=Resolve-Node $r.token
    if ($e.Current.Name -notin @('相簿','記事本','討論串','Albums','Notes','Threads','照片⋅影片','照片・影片','Photos & videos','聊天','Chats','返回','關閉','Back','Close','戻る','閉じる')) { throw 'navigation_not_allowed' }
    $pattern=$e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $pattern.Invoke();$result=$true
   }
   'download' {
    Assert-Foreground
    $e=Resolve-Node $r.token
    if ($e.Current.Name -notin $script:AllowedNames) { throw 'download_action_not_allowed' }
    $dest=[System.IO.Path]::GetFullPath([string]$r.destination)
    $parent=[System.IO.Path]::GetDirectoryName($dest)
    if (!(Test-Path -LiteralPath $parent -PathType Container) -or [System.IO.Path]::GetFileName($dest) -ne 'payload.bin' -or (Test-Path -LiteralPath $dest)) { throw 'invalid_destination' }
    if ((Get-Item -LiteralPath $parent).Attributes -band [System.IO.FileAttributes]::ReparsePoint) { throw 'linked_destination' }
    # Refuse an already open dialog instead of mistaking it for this download.
    if (@(Get-LineDialogs).Count -ne 0) { throw 'save_dialog_unsupported' }
    Assert-Foreground
    $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    $deadline=[DateTime]::UtcNow.AddSeconds(10);$dialog=$null
    while ([DateTime]::UtcNow -lt $deadline -and $null -eq $dialog) {
     Assert-Window
     $saveWindows=@(Get-LineDialogs)
     if ($saveWindows.Count -gt 1) { throw 'save_dialog_unsupported' }
     if ($saveWindows.Count -eq 1) { $dialog=$saveWindows[0] }
     if ($null -eq $dialog) { Start-Sleep -Milliseconds 100 }
    }
    if ($null -eq $dialog) { throw 'save_dialog_missing' }
    Assert-SaveDialog $dialog
    $edits=Find-Nodes $dialog @{control_type='Edit';automation_id='1001'} 2000
    if ($edits.Count -ne 1) { throw 'save_dialog_unsupported' }
    if ($edits[0].Current.ProcessId -ne $script:ProcessId -or !$edits[0].Current.IsEnabled) { throw 'save_dialog_unsupported' }
    $valuePattern=$edits[0].GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    if ($valuePattern.Current.IsReadOnly) { throw 'save_dialog_unsupported' }
    $valuePattern.SetValue($dest)
    if ($edits[0].GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value -cne $dest) { throw 'invalid_destination' }
    $buttons=Find-Nodes $dialog @{control_type='Button'} 2000
    $save=@($buttons | Where-Object { $_.Current.AutomationId -eq '1' -and ($_.Current.Name -replace '&','') -match '^(Save|儲存|保存)(\([A-Za-z]\))?$' -and $_.Current.ProcessId -eq $script:ProcessId -and $_.Current.IsEnabled -and !$_.Current.IsOffscreen })
    if ($save.Count -ne 1) { throw 'save_button_unsupported' }
    $invoke=$save[0].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    # Revalidate this exact dialog and destination immediately before invoking.
    # UIA cannot make this sequence atomic against a malicious same-user process.
    Assert-SaveDialog $dialog
    Assert-SaveDestination $valuePattern $dest
    $invoke.Invoke()
    $result=$true
   }
   'close' { exit 0 }
   default { throw 'unknown_action' }
  }
  @{ok=$true;result=$result} | ConvertTo-Json -Depth 20 -Compress | ForEach-Object { [Console]::WriteLine($_) }
 } catch {
  # Never return exception text, which can contain private paths or UI contents.
  $allowed=@('not_connected','wrong_process','wrong_window','focus_changed','stale_node','tree_limit','request_limit','line_window_ambiguous','publisher_unverified','selection_unsupported','scroll_unsupported','navigation_not_allowed','download_action_not_allowed','invalid_destination','linked_destination','save_dialog_missing','save_dialog_unsupported','save_button_unsupported','unknown_action')
  $code=if($_.Exception.Message -in $allowed){$_.Exception.Message}else{'uia_operation_failed'}
  @{ok=$false;code=$code} | ConvertTo-Json -Compress | ForEach-Object { [Console]::WriteLine($_) }
 }
}
