$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $MyInvocation.MyCommand.Path
$qtRoot = "D:\Qt"
$env:PATH = "$qtRoot\Tools\mingw1310_64\bin;$qtRoot\6.11.0\mingw_64\bin;$qtRoot\Tools\Ninja;$env:PATH"

& "$qtRoot\Tools\CMake_64\bin\cmake.exe" `
    -S $repo `
    -B "$repo\build-codex" `
    -G Ninja `
    -DCMAKE_PREFIX_PATH="$qtRoot\6.11.0\mingw_64" `
    -DCMAKE_CXX_COMPILER="$qtRoot\Tools\mingw1310_64\bin\g++.exe" `
    -DCMAKE_C_COMPILER="$qtRoot\Tools\mingw1310_64\bin\gcc.exe" `
    -DCMAKE_MAKE_PROGRAM="$qtRoot\Tools\Ninja\ninja.exe"

& "$qtRoot\Tools\CMake_64\bin\cmake.exe" --build "$repo\build-codex"
