# Configure + build astc2bc7 (library, CLI, astcenc decoder DLLs) with CMake + Ninja + clang-cl.
#   pwsh -File tools\textures\scripts\build.ps1 [-Clean]
# Needs third_party\ (see third_party\VERSIONS.md): astc-encoder, bc7enc_rdo, ispc. Output: tools\textures\build\release\
param([switch]$Clean)
$ErrorActionPreference = 'Stop'
$root  = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = Join-Path $root 'build\release'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsdir = if (Test-Path $vswhere) { & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
if (-not $vsdir) { $vsdir = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools' }
$vcvars = Join-Path $vsdir 'VC\Auxiliary\Build\vcvars64.bat'
$env:PATH = $env:PATH -replace '"', ''   # a stray quote in PATH breaks vcvars64.bat
foreach ($line in (cmd /c "`"$vcvars`" >nul 2>&1 && set")) {
    $i = $line.IndexOf('=')
    if ($i -gt 0) { Set-Item -Path ("Env:" + $line.Substring(0, $i)) -Value $line.Substring($i + 1) }
}
$env:PATH = "C:\Program Files\LLVM\bin;C:\Program Files\CMake\bin;$env:LOCALAPPDATA\Microsoft\WinGet\Links;$env:PATH"
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }
cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
if ($LASTEXITCODE) { throw 'cmake configure failed' }
cmake --build $build
if ($LASTEXITCODE) { throw 'build failed' }
Write-Output "built: $build\astc2bc7.exe"
