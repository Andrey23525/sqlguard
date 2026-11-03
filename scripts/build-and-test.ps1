# Builds, tests and reports SQLGuard on Windows with the MSVC toolchain.
# Usage: powershell -ExecutionPolicy Bypass -File scripts/build-and-test.ps1
param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    & "$PSScriptRoot\vsenv.cmd" cmake -S . -B $BuildDir -G Ninja `
        "-DCMAKE_BUILD_TYPE=$Config"
    if ($LASTEXITCODE -ne 0) { throw "configure failed" }

    & "$PSScriptRoot\vsenv.cmd" cmake --build $BuildDir --parallel
    if ($LASTEXITCODE -ne 0) { throw "build failed" }

    ctest --test-dir $BuildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "tests failed" }

    & "$BuildDir\bin\sqlguard_tests_shared.exe"
    if ($LASTEXITCODE -ne 0) { throw "live shared test executable failed" }

    & "$BuildDir\bin\sqlguard_tests_static.exe"
    if ($LASTEXITCODE -ne 0) { throw "live static test executable failed" }

    & "$BuildDir\bin\sqlguard_demo.exe"
    if ($LASTEXITCODE -ne 0) { throw "demo executable failed" }
} finally {
    Pop-Location
}
