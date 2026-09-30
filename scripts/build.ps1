param([string]$Fasm)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $Fasm) { $Fasm = Join-Path $projectRoot 'tools/fasm/FASM.EXE' }
if (-not (Test-Path -LiteralPath $Fasm)) { throw 'FASM não encontrado. Execute scripts/bootstrap.ps1 ou informe -Fasm.' }
$assembler = (Resolve-Path -LiteralPath $Fasm).Path
New-Item -ItemType Directory -Path (Join-Path $projectRoot 'bin') -Force | Out-Null
Push-Location (Join-Path $projectRoot 'asm')
try {
    & $assembler 'runtime.asm' 'runtime.bin'
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao montar runtime.asm' }
    & $assembler 'imports.asm' 'imports.bin'
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao montar imports.asm' }
    & $assembler '-m' '65536' 'slai.asm' '../bin/slai.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao montar slai.asm' }
} finally { Pop-Location }
