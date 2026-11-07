# Fortune's Weave dev loop: pull, rebuild the exe, run it for a while. Logs: work\logs\build.txt, nxrt.log.
#   powershell -ExecutionPolicy Bypass -File fefw_test.ps1 [-Seconds 40] [-Lift] [-RomFS <folder>]
# -Lift also re-lifts/recompiles the game modules (only needed after lifter changes).
param([int]$Seconds = 40, [switch]$Lift, [string]$RomFS = "C:\Users\Patricio\Desktop\SWITCH 2\FEFW ROM\FEFW ROM\131072\romfs")
Set-Location $PSScriptRoot
New-Item -ItemType Directory -Force work\logs | Out-Null
git pull
if ($Lift) {
    $env:NX_JOBS = "12"
    cmd /c "recomp\scripts\build_game.cmd fefw -k 0 > work\logs\lift.txt 2>&1"
    Get-Content work\logs\lift.txt -Tail 3
}
pwsh -File game-build\scripts\build.ps1 -Manifest recomp\games\fefw\game.toml *> work\logs\build.txt
if ($LASTEXITCODE) { Write-Host "BUILD FAILED (work\logs\build.txt)" -ForegroundColor Red; Get-Content work\logs\build.txt -Tail 15; exit 1 }
Write-Host "build ok, running $Seconds s..." -ForegroundColor Green
$env:NXRT_FS_ROMFS = $RomFS
# stage 4: on-demand shader translation when tools\shaders has been built (fefw_shaders_setup.ps1); translations persist in work\shader_cache
$harness = Get-ChildItem tools\shaders\build -Recurse -Filter spirv_harness.exe -ErrorAction SilentlyContinue | Select-Object -First 1
if ($harness) {
    New-Item -ItemType Directory -Force work\shader_cache\spirv | Out-Null
    $env:NXRT_NVN_SHADER_TRANSLATOR = $harness.FullName
    $env:NXRT_NVN_SHADER_CACHE = (Resolve-Path work\shader_cache\spirv).Path
    Write-Host "shader translator: $($harness.FullName)"
}
$p = Start-Process game-build\build\release\fefw.exe -PassThru -RedirectStandardError work\logs\run_err.txt
$t = 0; while (-not $p.HasExited -and $t -lt $Seconds) { Start-Sleep 1; $t++ }
if (-not $p.HasExited) { Stop-Process $p -Force; Write-Host "still running after $Seconds s (stopped)" -ForegroundColor Green }
else { Write-Host "exited after $t s" -ForegroundColor Yellow }
