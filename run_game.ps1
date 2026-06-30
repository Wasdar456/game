$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $MyInvocation.MyCommand.Path
$qtRoot = "D:\Qt"
$env:PATH = "$qtRoot\Tools\mingw1310_64\bin;$qtRoot\6.11.0\mingw_64\bin;$qtRoot\Tools\Ninja;$env:PATH"

$exe = "$repo\build-codex\DffenseAndAttack.exe"
if (!(Test-Path $exe)) {
    & "$repo\build_game.ps1"
}

Start-Process -FilePath $exe -WorkingDirectory "$repo\build-codex"
