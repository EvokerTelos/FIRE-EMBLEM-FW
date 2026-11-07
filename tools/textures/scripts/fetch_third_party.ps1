# Fetches tools\textures\third_party\ at the versions THIRD_PARTY.md records (astc-encoder 5.7.0, bc7enc_rdo b943862, ISPC 1.32.0).
# Nothing fetched is committed (tools/textures/.gitignore). Run once before tools\textures\scripts\build.ps1.
$ErrorActionPreference = 'Stop'
$tp = Join-Path (Split-Path -Parent $PSScriptRoot) 'third_party'
New-Item -ItemType Directory -Force $tp | Out-Null

if (-not (Test-Path "$tp\astc-encoder\Source")) {
    git clone --depth 1 --branch 5.7.0 https://github.com/ARM-software/astc-encoder.git "$tp\astc-encoder"
    if ($LASTEXITCODE) { throw 'astc-encoder clone failed' }
}
if (-not (Test-Path "$tp\bc7enc_rdo\bc7e.ispc")) {
    git clone https://github.com/richgel999/bc7enc_rdo.git "$tp\bc7enc_rdo"
    if ($LASTEXITCODE) { throw 'bc7enc_rdo clone failed' }
    git -C "$tp\bc7enc_rdo" checkout -q b9438627eef73a1157e84201b6fa6eb2ffd6d9f0
    if ($LASTEXITCODE) { throw 'bc7enc_rdo checkout failed' }
}
$ispc = "$tp\ispc\ispc-v1.32.0-windows\bin\ispc.exe"
if (-not (Test-Path $ispc)) {
    $dl = Join-Path (Split-Path -Parent $PSScriptRoot) 'build\downloads\ispc-v1.32.0'
    New-Item -ItemType Directory -Force $dl | Out-Null
    $zip = "$dl\ispc-v1.32.0-windows.zip"
    Invoke-WebRequest https://github.com/ispc/ispc/releases/download/v1.32.0/ispc-v1.32.0-windows.zip -OutFile $zip
    $h = (Get-FileHash $zip -Algorithm SHA256).Hash
    if ($h -ne '148D5CD96ABE7501C730D5FBD00F1F0C893B96BFEADA38D1811C9B6251E86A5C') { throw "ispc zip hash mismatch: $h" }
    Expand-Archive $zip -DestinationPath "$tp\ispc" -Force
    if (-not (Test-Path $ispc)) { throw "ispc.exe not found after extracting (looked for $ispc)" }
}
Write-Output "third_party ready: $tp"
