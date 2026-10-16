# modhash.ps1 - fingerprint of the game folder for multiplayer desync hunting.
#
# Lists size + MD5 of every data file the simulation may read (txt/csv/lua/gui/gfx/mod/ini/conf)
# in the game folder and of the executables/DLLs in its root, plus CPU / OS / GPU info.
# Run it in the game folder (next to v2game.exe) on EVERY machine, host included:
#     powershell -NoProfile -ExecutionPolicy Bypass -File .\modhash.ps1
# (or double-click modhash.bat). Result: modhash_<COMPUTERNAME>.txt in the game folder -
# send it back. Compare with:  python compare_modhash.py host.txt client1.txt client2.txt ...
param(
    [string]$Root = (Get-Location).Path,
    [string]$Out = ''
)

$ErrorActionPreference = 'SilentlyContinue'
$Root = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\')
if (-not $Out) { $Out = Join-Path $Root ("modhash_" + $env:COMPUTERNAME + ".txt") }

$lines = New-Object System.Collections.Generic.List[string]
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$os = Get-CimInstance Win32_OperatingSystem
$gpu = (Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name + ' ' + $_.DriverVersion }) -join ' | '
$lines.Add("# host: $env:COMPUTERNAME")
$lines.Add("# root: $Root")
$lines.Add("# cpu: " + $cpu.Name + " cores=" + $cpu.NumberOfCores + " logical=" + $cpu.NumberOfLogicalProcessors)
$lines.Add("# os: " + $os.Caption + " " + $os.Version)
$lines.Add("# gpu: $gpu")
$lines.Add("# date: " + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))

$dataExt = '.txt', '.csv', '.lua', '.gui', '.gfx', '.mod', '.ini', '.conf', '.asset', '.sfx', '.yml'
$binExt = '.exe', '.dll'

Get-ChildItem -LiteralPath $Root -Recurse -File -Force | ForEach-Object {
    $ext = $_.Extension.ToLower()
    $rel = $_.FullName.Substring($Root.Length + 1)
    $isRoot = ($_.DirectoryName -eq $Root)
    $take = $false
    if ($dataExt -contains $ext) { $take = $true }
    elseif ($isRoot -and ($binExt -contains $ext)) { $take = $true }
    # logs and caches change on every run
    if ($rel -like 'Logs\*' -or $rel -like 'cache\*' -or $rel -like 'Screenshots\*' -or $rel -like 'save games\*' -or $rel -like 'modhash_*') { $take = $false }
    if ($take) {
        $h = (Get-FileHash -LiteralPath $_.FullName -Algorithm MD5).Hash
        $lines.Add(("{0}`t{1}`t{2}" -f $rel, $_.Length, $h))
    }
}

[System.IO.File]::WriteAllLines($Out, $lines, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Done: $($lines.Count) lines -> $Out"
