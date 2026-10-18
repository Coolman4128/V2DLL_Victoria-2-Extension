param(
    [string]$BuildRoot = (Join-Path $env:LOCALAPPDATA ('V2DLLFork/build-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))),
    [string]$MSBuild = 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe'
)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source = Join-Path $repository 'Source/V2/V2'
$root = [IO.Path]::GetFullPath($BuildRoot)
if ($root.StartsWith($repository + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'BuildRoot must be outside the repository.'
}
if (Test-Path -LiteralPath $root) { throw 'BuildRoot must be a new directory for a clean build.' }
$staged = Join-Path $root 'source'
$artifacts = Join-Path $root 'artifacts'
New-Item -ItemType Directory -Path $staged,$artifacts | Out-Null
$manifest = foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File) {
    if ($file.Extension -notin '.cpp','.h','.c','.vcxproj') { continue }
    $relative = $file.FullName.Substring($source.Length + 1)
    $destination = Join-Path $staged $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination
    [PSCustomObject]@{ Path=$relative; SHA256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash }
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'source-manifest.json') -Encoding UTF8
# Global overrides also cover EventMusic and V2Mod. Default project-local
# intermediate paths remain separate; no old binary or object is copied.
& $MSBuild (Join-Path $staged 'V2.vcxproj') /t:Rebuild /m:1 /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0.26100.0 "/p:OutDir=$artifacts/" /v:minimal /fl "/flp:logfile=$artifacts/build.log;verbosity=normal"
if ($LASTEXITCODE -ne 0) { throw "MSBuild failed with exit code $LASTEXITCODE" }
Get-ChildItem -LiteralPath $artifacts -File | Where-Object Extension -in '.dll','.pdb' | Get-FileHash -Algorithm SHA256 | Format-Table
Write-Output "Built current-source Win32 artifacts in $artifacts; no game deployment performed."
