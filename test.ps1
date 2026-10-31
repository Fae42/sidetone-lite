$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction SilentlyContinue).Source
if (-not $compiler) {
    $compiler = Join-Path $root 'tools/w64devkit/bin/g++.exe'
}
if (-not (Test-Path $compiler)) {
    throw 'g++ was not found.'
}
$env:PATH = (Split-Path $compiler) + ';' + $env:PATH
$outputDirectory = Join-Path $root 'build/tests'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$output = Join-Path $outputDirectory 'JiantingTests.exe'

$arguments = @(
    '-std=c++20',
    '-Os',
    '-DUNICODE',
    '-D_UNICODE',
    '-DWIN32_LEAN_AND_MEAN',
    '-DNOMINMAX',
    '-D_WIN32_WINNT=0x0A00',
    '-DWINVER=0x0A00',
    '-ffunction-sections',
    '-fdata-sections',
    '-static',
    '-static-libgcc',
    '-static-libstdc++',
    '-I',
    (Join-Path $root 'src'),
    (Join-Path $root 'tests/tests.cpp'),
    (Join-Path $root 'src/dsp.cpp'),
    (Join-Path $root 'src/settings.cpp'),
    '-Wl,--gc-sections',
    '-Wl,-s',
    '-lksuser',
    '-o',
    $output
)

& $compiler @arguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
& $output
exit $LASTEXITCODE
