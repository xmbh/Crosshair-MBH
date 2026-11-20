param([string] $Executable = (Join-Path $PSScriptRoot '..\build\MBH-Crosshair.exe'))
$ErrorActionPreference = 'Stop'
if (-not ('ReticleCheck' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ReticleCheck {
  public delegate bool Visit(IntPtr h,IntPtr data);
  [DllImport("user32.dll")] static extern bool EnumWindows(Visit fn,IntPtr d);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint process);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h,StringBuilder s,int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out Rect r);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h,int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,int m,IntPtr w,IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,int m,IntPtr w,IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(Point p);
  [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
  [DllImport("gdi32.dll")] public static extern uint GetPixel(IntPtr dc,int x,int y);
  public struct Rect {public int Left,Top,Right,Bottom;}
  public struct Point {public int X,Y;}
  public static IntPtr Find(int pid,string cls) {
    IntPtr found=IntPtr.Zero;EnumWindows(delegate(IntPtr h,IntPtr ignored) {
      uint p;GetWindowThreadProcessId(h,out p); if(p!=pid)return true;
      var s=new StringBuilder(256);GetClassName(h,s,256);if(s.ToString()!=cls)return true;
      found=h;return false;},IntPtr.Zero);return found;
  }
}
'@
}
$original = (Resolve-Path -LiteralPath $Executable).Path
if (Get-Process -Name 'MBH-Crosshair' -ErrorAction SilentlyContinue) {
    throw 'Chiudi MBH Crosshair prima di avviare il test.'
}
$testFolder = New-Item -ItemType Directory -Path (Join-Path ([IO.Path]::GetTempPath()) ('mbh-test-' + [guid]::NewGuid().ToString('N')))
$envPath = Join-Path $testFolder.FullName 'MBH-Crosshair.exe'
Copy-Item -LiteralPath $original -Destination $envPath
$proc=Start-Process -FilePath $envPath -PassThru -WindowStyle Normal
$priorDpi=[ReticleCheck]::SetThreadDpiAwarenessContext([IntPtr](-4))
function Find-TestWindow([string]$name) { [ReticleCheck]::Find($proc.Id,$name) }
function Wait-For([scriptblock]$condition,[string]$failure) {
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    do { if (& $condition) { return }; Start-Sleep -Milliseconds 70 } while ([DateTime]::UtcNow -lt $deadline)
    throw $failure
}
try {
    Wait-For { $window=Find-TestWindow 'MBH.Lite.Panel'; $window -and [ReticleCheck]::IsWindowVisible($window) -and [ReticleCheck]::GetDlgItem($window,114) -ne 0 } 'Panel missing'
    $panel=Find-TestWindow 'MBH.Lite.Panel'
    $hub=Find-TestWindow 'MBH.Crosshair.Lite.Controller'
    $ov=Find-TestWindow 'MBH.Lite.Overlay'
    if(!$hub -or !$ov -or ![ReticleCheck]::IsWindowVisible($ov)){throw 'Visible overlay or controller missing'}
    $styles=[ReticleCheck]::GetWindowLong($ov,-20)
    if(($styles -band 0x080800a8) -ne 0x080800a8){throw 'Unexpected overlay window style'}
    $rect=New-Object ReticleCheck+Rect
    [void][ReticleCheck]::GetWindowRect($ov,[ref]$rect)
    if(($rect.Right-$rect.Left) -ne 100 -or ($rect.Bottom-$rect.Top) -ne 100){throw 'Overlay bounds incorrect'}
    $point=New-Object ReticleCheck+Point
    $point.X=$rect.Left+54; $point.Y=$rect.Top+50
    if([ReticleCheck]::WindowFromPoint($point) -eq $ov){throw 'Overlay received pointer input'}
    $dc=[ReticleCheck]::GetDC(0)
    $sample=[ReticleCheck]::GetPixel($dc,$point.X,$point.Y)
    [void][ReticleCheck]::ReleaseDC(0,$dc)
    $shape=[ReticleCheck]::GetDlgItem($panel,101)
    [void][ReticleCheck]::SendMessage($shape,0x14e,2,0)
    [void][ReticleCheck]::SendMessage($panel,0x111,(1 -shl 16 -bor 101),$shape)
    [void][ReticleCheck]::PostMessage($panel,0x10,0,0)
    Wait-For { (Find-TestWindow 'MBH.Lite.Panel') -eq 0 } 'X did not close panel'
    if($proc.HasExited -or ![ReticleCheck]::IsWindowVisible($ov)){throw 'X closed overlay'}
    $proc.Refresh(); $cpu=$proc.TotalProcessorTime; Start-Sleep -Milliseconds 1500; $proc.Refresh()
    $ms=($proc.TotalProcessorTime - $cpu).TotalMilliseconds
    $working=$proc.WorkingSet64
    [void][ReticleCheck]::PostMessage($hub,0x8001,1,0x202)
    Wait-For { $window=Find-TestWindow 'MBH.Lite.Panel'; $window -and [ReticleCheck]::IsWindowVisible($window) -and [ReticleCheck]::GetDlgItem($window,114) -ne 0 } 'Tray callback did not reopen panel'
    $panel=Find-TestWindow 'MBH.Lite.Panel'; $shape=[ReticleCheck]::GetDlgItem($panel,101)
    if([ReticleCheck]::SendMessage($shape,0x147,0,0).ToInt32() -ne 2){throw 'Shape lost after reopening'}
    [void][ReticleCheck]::PostMessage($panel,0x111,114,0)
    if(!$proc.WaitForExit(4000) -or $proc.ExitCode -ne 0){throw 'Exit failed'}
    $ini=[IO.Path]::ChangeExtension($envPath,'.ini')
    if(!((Get-Content -LiteralPath $ini) -contains 'shape=2')){throw 'Persistence failed'}
    "Passed: startup, overlay, click-through, X, tray callback, settings and exit. Sample pixel=$sample; working set (bytes)=$working; idle CPU (ms in 1.5 s)=$ms"
} finally {
    [void][ReticleCheck]::SetThreadDpiAwarenessContext($priorDpi)
    if(!$proc.HasExited){$proc.Kill();[void]$proc.WaitForExit(2000)}
    $proc.Dispose()
    foreach ($name in 'MBH-Crosshair.ini', 'MBH-Crosshair.ini.tmp', 'MBH-Crosshair.exe') {
        $testFile = Join-Path $testFolder.FullName $name
        if (Test-Path -LiteralPath $testFile) { Remove-Item -LiteralPath $testFile }
    }
    [IO.Directory]::Delete($testFolder.FullName)
}

