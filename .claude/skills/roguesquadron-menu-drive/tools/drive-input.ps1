# Send a key sequence to the game window via Win32 SendInput scancodes (set 1), which is what SDL reads.
# -Keys "enter:100:1500,space:100:800"  -> token = name:holdMs:afterMs (defaults 90/400)
param([Parameter(Mandatory=$true)][string]$Keys, [string]$Title = "Rogue Squadron 64 Recompiled")

Add-Type @'
using System; using System.Runtime.InteropServices;
public static class Drv {
  [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Explicit, Size=40)] public struct INPUT { [FieldOffset(0)] public uint type; [FieldOffset(8)] public KEYBDINPUT ki; }
  [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] p, int cb);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr FindWindow(string c, string w);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  const uint KEYEVENTF_SCANCODE=0x0008, KEYEVENTF_KEYUP=0x0002, KEYEVENTF_EXTENDEDKEY=0x0001;
  public static void Key(ushort scan, bool ext, int holdMs) {
    uint f = KEYEVENTF_SCANCODE | (ext ? KEYEVENTF_EXTENDEDKEY : 0);
    INPUT[] d = new INPUT[1]; d[0].type=1; d[0].ki.wScan=scan; d[0].ki.dwFlags=f;
    SendInput(1, d, Marshal.SizeOf(typeof(INPUT)));
    System.Threading.Thread.Sleep(holdMs);
    INPUT[] u = new INPUT[1]; u[0].type=1; u[0].ki.wScan=scan; u[0].ki.dwFlags=f|KEYEVENTF_KEYUP;
    SendInput(1, u, Marshal.SizeOf(typeof(INPUT)));
  }
}
'@
$map = @{ enter=@(0x1C,$false); space=@(0x39,$false); lshift=@(0x2A,$false); lctrl=@(0x1D,$false);
          w=@(0x11,$false); a=@(0x1E,$false); s=@(0x1F,$false); d=@(0x20,$false); e=@(0x12,$false); q=@(0x10,$false);
          up=@(0x48,$true); down=@(0x50,$true); left=@(0x4B,$true); right=@(0x4D,$true); esc=@(0x01,$false);
          backspace=@(0x0E,$false); x=@(0x2D,$false); f=@(0x21,$false); z=@(0x2C,$false); alt=@(0x38,$false); prtsc=@(0x37,$true) }

$proc = Get-Process RogueSquadron64Recomp -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Error "game not running"; exit 1 }
[void][Drv]::SetForegroundWindow($proc.MainWindowHandle); Start-Sleep -Milliseconds 400

foreach ($tok in $Keys.Split(',')) {
    $p = $tok.Trim().Split(':'); $name = $p[0].ToLower()
    if (-not $map.ContainsKey($name)) { Write-Warning "unknown key '$name'"; continue }
    $hold = if ($p.Count -gt 1 -and $p[1]) { [int]$p[1] } else { 90 }
    $after = if ($p.Count -gt 2 -and $p[2]) { [int]$p[2] } else { 400 }
    [void][Drv]::SetForegroundWindow($proc.MainWindowHandle)
    [Drv]::Key([uint16]$map[$name][0], [bool]$map[$name][1], $hold)
    Start-Sleep -Milliseconds $after
}
"sent: $Keys"
