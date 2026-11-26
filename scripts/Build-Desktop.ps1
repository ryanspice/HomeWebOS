[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$VcpkgRoot,
    [string]$BuildDir,
    [switch]$Run
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Root = Split-Path $PSScriptRoot -Parent
if (-not $BuildDir) { $BuildDir = Join-Path $Root 'build\windows' }
$Toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if (-not (Test-Path -LiteralPath $Toolchain -PathType Leaf)) { throw "vcpkg toolchain missing: $Toolchain" }
foreach ($Tool in @('cmake','ninja','cl')) {
    if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        throw "$Tool not found. Use a Visual Studio 2022 Developer PowerShell with C++ build tools and Ninja."
    }
}
function Invoke-Checked {
    param([string]$Command, [string[]]$Arguments)
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command exited with code $LASTEXITCODE" }
}
# Manifest mode may download/build the dependencies declared in vcpkg.json.
Invoke-Checked -Command 'cmake' -Arguments @('-S',$Root,'-B',$BuildDir,'-G','Ninja',
    '-DCMAKE_BUILD_TYPE=Release','-DSPICE_STRICT=ON',
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",'-DVCPKG_TARGET_TRIPLET=x64-windows')
Invoke-Checked -Command 'cmake' -Arguments @('--build',$BuildDir,'--parallel','4')
Invoke-Checked -Command 'ctest' -Arguments @('--test-dir',$BuildDir,'--output-on-failure')
$Exe = Join-Path $BuildDir 'spice-home-cpp.exe'
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw 'Native executable was not created.' }
# SDL/FreeType use dynamic loading, so their DLLs are not found by linker-based app-local deployment.
$DllDir = Join-Path $BuildDir 'vcpkg_installed\x64-windows\bin'
if (-not (Test-Path -LiteralPath $DllDir -PathType Container)) { throw "Inspect the vcpkg runtime DLL location: $DllDir" }
Get-ChildItem -LiteralPath $DllDir -Filter '*.dll' -File | Copy-Item -Destination $BuildDir
Write-Host "Built: $Exe"
if ($Run) { Invoke-Checked -Command $Exe -Arguments @('--assets',(Join-Path $Root 'assets')) }
