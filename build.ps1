$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$compiler = (Get-Command g++ -ErrorAction SilentlyContinue).Source
if (-not $compiler) {
    $compiler = Join-Path $root 'tools/w64devkit/bin/g++.exe'
}
if (-not (Test-Path $compiler)) {
    throw 'g++ was not found. Install MinGW-w64 or place w64devkit under tools/w64devkit.'
}
$env:PATH = (Split-Path $compiler) + ';' + $env:PATH

$dist = Join-Path $root 'dist'
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $dist, $build | Out-Null

$sources = Get-ChildItem (Join-Path $root 'src') -Filter '*.cpp' |
    ForEach-Object { $_.FullName }
$arguments = @(
    '-std=c++20',
    '-Os',
    '-DNDEBUG',
    '-DUNICODE',
    '-D_UNICODE',
    '-DWIN32_LEAN_AND_MEAN',
    '-DNOMINMAX',
    '-D_WIN32_WINNT=0x0A00',
    '-DWINVER=0x0A00',
    '-ffunction-sections',
    '-fdata-sections',
    '-municode',
    '-mwindows',
    '-static',
    '-static-libgcc',
    '-static-libstdc++'
) + $sources

$resource = Join-Path $root 'src/Jianting.rc'
if (Test-Path $resource) {
    $windres = Join-Path (Split-Path $compiler) 'windres.exe'
    $resourceObject = Join-Path $build 'Jianting-res.o'
    & $windres '-c' '65001' '-I' (Join-Path $root 'src') $resource $resourceObject
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    $arguments += $resourceObject
}

$arguments += @(
    '-Wl,--gc-sections',
    '-Wl,-s',
    '-lole32',
    '-luuid',
    '-lksuser',
    '-lavrt',
    '-ldwmapi',
    '-luxtheme',
    '-lcomctl32',
    '-lgdi32',
    '-luser32',
    '-o',
    (Join-Path $dist 'Jianting.exe')
)

& $compiler @arguments
exit $LASTEXITCODE
