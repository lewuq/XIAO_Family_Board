$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$compiler = Get-Command g++ -ErrorAction Stop
$buildDirectory = Join-Path $projectRoot '.pio\native-tests'
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
$testExecutable = Join-Path $buildDirectory 'protocol_native.exe'

# Compile the same protocol header used by the ESP32 firmware, for the host CPU.
& $compiler.Source -std=c++17 -Wall -Wextra -Werror -pedantic `
    '-I' (Join-Path $projectRoot 'include') `
    (Join-Path $PSScriptRoot 'protocol_native.cpp') -o $testExecutable
if ($LASTEXITCODE -ne 0) { throw 'Native protocol test compilation failed.' }
& $testExecutable
if ($LASTEXITCODE -ne 0) { throw 'Native protocol tests failed.' }
