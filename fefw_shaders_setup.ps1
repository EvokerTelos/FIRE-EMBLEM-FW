# Stage 4 setup: the on-demand shader translator (spirv_harness) for Fortune's Weave.
#   powershell -ExecutionPolicy Bypass -File fefw_shaders_setup.ps1
# Installs .NET SDK 10 and the Vulkan SDK (winget), fetches the Ryubing sources the translator is built from (refs\ryubing, git-ignored),
# stages them (tools\shaders\scripts\setup_ryujinx.ps1) and builds tools\shaders\harness. Log: work\logs\shaders_setup.txt
$ErrorActionPreference = 'Continue'
Set-Location $PSScriptRoot
New-Item -ItemType Directory -Force work\logs, refs | Out-Null
Start-Transcript work\logs\shaders_setup.txt -Force | Out-Null

Write-Host "== 1/4 .NET SDK 10 + Vulkan SDK" -ForegroundColor Cyan
winget install --id Microsoft.DotNet.SDK.10 -e --accept-package-agreements --accept-source-agreements --silent
winget install --id KhronosGroup.VulkanSDK -e --accept-package-agreements --accept-source-agreements --silent
$env:PATH = [Environment]::GetEnvironmentVariable('PATH','Machine') + ';' + [Environment]::GetEnvironmentVariable('PATH','User')
$vk = Get-ChildItem C:\VulkanSDK -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
if ($vk) { $env:VULKAN_SDK = $vk.FullName; $env:PATH = "$($vk.FullName)\Bin;$env:PATH"; Write-Host "Vulkan SDK: $($vk.FullName)" }
dotnet --list-sdks

Write-Host "== 2/4 Ryubing sources" -ForegroundColor Cyan
if (-not (Test-Path refs\ryubing\src\Ryujinx.Graphics.Shader)) {
    foreach ($u in 'https://git.ryujinx.app/ryubing/ryujinx.git', 'https://github.com/Ryubing/Ryujinx.git', 'https://github.com/ryujinx-mirror/ryujinx.git') {
        Write-Host "trying $u"
        git clone $u refs\ryubing
        if ($LASTEXITCODE -eq 0 -and (Test-Path refs\ryubing\src\Ryujinx.Graphics.Shader)) { break }
        Remove-Item -Recurse -Force refs\ryubing -ErrorAction SilentlyContinue
    }
}
if (Test-Path refs\ryubing\.git) {
    git -C refs\ryubing checkout -q a82350bb77 2>$null
    if ($LASTEXITCODE) { Write-Host "commit a82350bb77 not in this clone: building from its HEAD (patches may not apply)" -ForegroundColor Yellow }
    git -C refs\ryubing log --oneline -1
}

Write-Host "== 3/4 stage translator sources" -ForegroundColor Cyan
powershell -ExecutionPolicy Bypass -File tools\shaders\scripts\setup_ryujinx.ps1

Write-Host "== 4/4 build spirv_harness" -ForegroundColor Cyan
dotnet build tools\shaders\harness\SpirvHarness.csproj -c Release
$exe = Get-ChildItem tools\shaders\build -Recurse -Filter spirv_harness.exe -ErrorAction SilentlyContinue | Select-Object -First 1
if ($exe) { Write-Host "OK: $($exe.FullName)" -ForegroundColor Green } else { Write-Host "spirv_harness.exe not built: see work\logs\shaders_setup.txt" -ForegroundColor Red }
Stop-Transcript | Out-Null
