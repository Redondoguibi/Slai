$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$compiler=Join-Path $root 'bin/slai.exe'
$folder=Join-Path ([IO.Path]::GetTempPath()) ('slai-pe-test-'+[guid]::NewGuid())
[IO.Directory]::CreateDirectory($folder) | Out-Null
function Check([bool]$Condition,[string]$Name) { if(!$Condition) { throw $Name }; $script:count++ }
$script:count=0
try {
    $source=Join-Path $folder 'test.slai'
    $normal=Join-Path $folder 'normal.exe'
    $release=Join-Path $folder 'release.exe'
    [IO.File]::WriteAllText($source,"sys.log((2 + 3) * (7 - 1))`n")
    & $compiler build $source -o $normal | Out-Null
    Check ($LASTEXITCODE -eq 0) 'normal build'
    & $compiler build $source -o $release --release | Out-Null
    Check ($LASTEXITCODE -eq 0) 'release build'
    $a=[IO.File]::ReadAllBytes($normal)
    $b=[IO.File]::ReadAllBytes($release)
    Check ([Convert]::ToBase64String($a) -ne [Convert]::ToBase64String($b)) 'release optimizer changes code'
    $sir=((& $compiler sir $source --release) -join "`n") | ConvertFrom-Json
    Check (@($sir[0].instructions | Where-Object {$_.op -eq 'const' -and $_.value -eq 30}).Count -gt 0) 'release folds arithmetic in typed SIR'
    Check ($a[0] -eq 0x4D -and $a[1] -eq 0x5A) 'DOS signature'
    $pe=[BitConverter]::ToInt32($a,0x3C)
    Check ([BitConverter]::ToInt32($a,$pe) -eq 0x4550) 'PE signature'
    Check ([BitConverter]::ToUInt16($a,$pe+4) -eq 0x8664) 'AMD64 architecture'
    Check ([BitConverter]::ToUInt16($a,$pe+24) -eq 0x20B) 'PE32+ optional header'
    Check ($a.Length % 512 -eq 0) 'file alignment'
    $sections=[BitConverter]::ToUInt16($a,$pe+6)
    Check ($sections -eq 5) 'five native sections'
    $exceptionSize=[BitConverter]::ToInt32($a,$pe+24+112+28)
    Check ($exceptionSize -gt 0 -and $exceptionSize % 12 -eq 0) 'unwind function directory'
    $textSize=[BitConverter]::ToInt32($a,$pe+24+240+16)
    $textStart=[BitConverter]::ToInt32($a,$pe+24+240+20)
    Check ($textStart+$textSize -le $a.Length) 'section within file'
    Push-Location (Join-Path $root 'tests')
    try { & (Join-Path $root 'tools/fasm/FASM.EXE') unwind.asm (Join-Path $folder 'unwind.exe') | Out-Null; Check ($LASTEXITCODE -eq 0) 'Assembly unwind verifier builds' }
    finally { Pop-Location }
    & (Join-Path $folder 'unwind.exe') $normal
    Check ($LASTEXITCODE -eq 0) 'Windows unwinds runtime and generated functions'
    & (Join-Path $folder 'unwind.exe') $release
    Check ($LASTEXITCODE -eq 0) 'Windows unwinds optimized functions'
    & $compiler build (Join-Path $root 'examples/application.slai') -o $normal | Out-Null
    & (Join-Path $folder 'unwind.exe') $normal
    Check ($LASTEXITCODE -eq 0) 'Windows unwinds classes and recursive functions'
    $before=[IO.File]::ReadAllBytes($normal)
    [IO.File]::WriteAllText($source,'sys.log(missing)')
    $p=[Diagnostics.ProcessStartInfo]::new($compiler)
    foreach($arg in @('build',$source,'-o',$normal,'--json')) { $p.ArgumentList.Add($arg) }
    $p.UseShellExecute=$false; $p.RedirectStandardError=$true; $p.RedirectStandardOutput=$true
    $process=[Diagnostics.Process]::Start($p)
    $stderr=$process.StandardError.ReadToEnd(); $stdout=$process.StandardOutput.ReadToEnd(); $process.WaitForExit()
    Check ($process.ExitCode -eq 1 -and $stdout -eq '' -and ($stderr | ConvertFrom-Json).ok -eq $false) 'JSON diagnostics on stderr'
    Check ([Convert]::ToBase64String($before) -eq [Convert]::ToBase64String([IO.File]::ReadAllBytes($normal))) 'failed build preserves previous executable'
} finally {
    $resolved=[IO.Path]::GetFullPath($folder)
    if($resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath())) -and (Split-Path $resolved -Leaf) -like 'slai-pe-test-*') { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
Write-Host "$script:count PE/optimizer checks passed"

