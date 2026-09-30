$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$toolsDirectory = Join-Path $projectRoot 'tools'
New-Item -ItemType Directory -Path $toolsDirectory -Force | Out-Null
$archive = Join-Path $toolsDirectory 'fasmw17335.zip'
Invoke-WebRequest -Uri 'https://flatassembler.net/fasmw17335.zip' -OutFile $archive
$expected = '8EF871B369638F63D2DF475A64E9F574DA06B601DB5A3FCB8C12654B7BCF5E81'
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) { throw 'SHA256 do FASM não confere.' }
Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $toolsDirectory 'fasm') -Force
& (Join-Path $PSScriptRoot 'build.ps1')
