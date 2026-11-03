# Verifies the installed CMake package with both exported targets.
# Usage: powershell -ExecutionPolicy Bypass -File scripts/verify-install.ps1
param(
    [string]$BuildDir = "build",
    [string]$Prefix = "build/install",
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$consumerBuild = Join-Path $BuildDir "package-consumer"
Push-Location $root
try {
    & "$PSScriptRoot\vsenv.cmd" cmake -S . -B $BuildDir -G Ninja `
        "-DCMAKE_BUILD_TYPE=$Config"
    if ($LASTEXITCODE -ne 0) { throw "configure failed" }

    & "$PSScriptRoot\vsenv.cmd" cmake --build $BuildDir --parallel
    if ($LASTEXITCODE -ne 0) { throw "build failed" }

    & "$PSScriptRoot\vsenv.cmd" cmake --install $BuildDir --prefix $Prefix
    if ($LASTEXITCODE -ne 0) { throw "install failed" }

    $prefixPath = (Resolve-Path $Prefix).Path
    & "$PSScriptRoot\vsenv.cmd" cmake `
        -S tests/package_consumer -B $consumerBuild -G Ninja `
        "-DCMAKE_BUILD_TYPE=$Config" `
        "-DCMAKE_PREFIX_PATH=$prefixPath"
    if ($LASTEXITCODE -ne 0) { throw "consumer configure failed" }

    & "$PSScriptRoot\vsenv.cmd" cmake --build $consumerBuild --parallel
    if ($LASTEXITCODE -ne 0) { throw "consumer build failed" }

    $oldPath = $env:PATH
    $env:PATH = "$prefixPath\bin;$env:PATH"
    try {
        & "$consumerBuild\consumer_shared.exe"
        if ($LASTEXITCODE -ne 0) { throw "shared consumer failed" }
        & "$consumerBuild\consumer_static.exe"
        if ($LASTEXITCODE -ne 0) { throw "static consumer failed" }
    } finally {
        $env:PATH = $oldPath
    }
} finally {
    Pop-Location
}
