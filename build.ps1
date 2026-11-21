param(
    [string] $Compiler = 'i686-w64-mingw32-clang++',
    [string] $ResourceCompiler
)

$ErrorActionPreference = 'Stop'
$Compiler = (Get-Command -Name $Compiler -CommandType Application).Source
if (!$ResourceCompiler) {
    $ResourceCompiler = Join-Path (Split-Path $Compiler) 'i686-w64-mingw32-windres.exe'
}
$ResourceCompiler = (Get-Command -Name $ResourceCompiler -CommandType Application).Source
$release = Join-Path $PSScriptRoot 'Release'
$out = Join-Path $release 'Multiple Bonus Vehicles.lmlh'
$resource = Join-Path ([IO.Path]::GetTempPath()) ('mbv-' + [guid]::NewGuid().ToString('N') + '.o')
New-Item -Path $release -ItemType Directory -Force | Out-Null

Push-Location -LiteralPath $PSScriptRoot
try {
    & $ResourceCompiler -i 'hack.rc' -o $resource -O coff
    if ($LASTEXITCODE -ne 0) { throw "Resource build failed with exit code $LASTEXITCODE." }
    & $Compiler -std=c++17 -m32 -O2 -Wall -Wextra -Werror -shared -fno-exceptions -fno-rtti -static bonus.cpp hooks.S $resource '-Wl,--dynamicbase,--nxcompat,--strip-all' -o $out
    if ($LASTEXITCODE -ne 0) { throw "Hack build failed with exit code $LASTEXITCODE." }
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $resource) { Remove-Item -LiteralPath $resource }
}
Get-Item -LiteralPath $out
