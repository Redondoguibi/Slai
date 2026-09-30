param([string]$Compiler='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if (!$Compiler) { $Compiler=Join-Path $root 'bin/slai.exe' }
$folder=Join-Path ([IO.Path]::GetTempPath()) ('slai-ia-test-'+[guid]::NewGuid())
[IO.Directory]::CreateDirectory($folder) | Out-Null
$source=Join-Path $folder 'main.slai'
$exe=Join-Path $folder 'program.exe'
$script:count=0
$script:failures=0
$classes=@'
cl Player:
    .args():
        this.health = 100
    .damage(amount) => this.health -= amount
cl Enemy:
    .args():
        this.health = 50
player = Player()
enemy = Enemy()
player.damage(10)
sys.log(player.health)
sys.log(enemy.health)
? Player.health is a comment
sys.log("Player.health is text")
'@
function Write-Source([string]$Text) { [IO.File]::WriteAllText($source,$Text,[Text.UTF8Encoding]::new($false)) }
function Execute([string]$Program,[string[]]$Arguments) {
    $pinfo=[Diagnostics.ProcessStartInfo]::new($Program)
    foreach($arg in $Arguments) { $pinfo.ArgumentList.Add($arg) }
    $pinfo.UseShellExecute=$false
    $pinfo.RedirectStandardOutput=$true
    $pinfo.RedirectStandardError=$true
    $pinfo.StandardOutputEncoding=[Text.Encoding]::UTF8
    $p=[Diagnostics.Process]::Start($pinfo)
    $out=$p.StandardOutput.ReadToEndAsync()
    $err=$p.StandardError.ReadToEndAsync()
    if(!$p.WaitForExit(15000)) { $p.Kill($true); throw 'Timed out' }
    @{Code=$p.ExitCode;Text=($out.Result+$err.Result).Replace("`r`n","`n");Error=$err.Result}
}
function Slai([string[]]$Arguments) { Execute $Compiler $Arguments }
function Assert([bool]$Condition,[string]$Name) {
    $script:count++
    if(!$Condition) { $script:failures++; Write-Host "FAIL: $Name" -ForegroundColor Red }
}
try {
    Write-Source $classes
    $r=Slai @('ia',$source,'inspect','Player'); $j=$r.Text | ConvertFrom-Json
    Assert ($r.Code -eq 0 -and $j.kind -eq 'class' -and $j.references.Count -gt 0) 'class inspection'
    $r=Slai @('ia',$source,'refs','Player.health'); $j=$r.Text | ConvertFrom-Json
    Assert ($r.Code -eq 0 -and $j.Count -eq 2) 'field references'
    foreach($case in @(@('Player.health','hp','this.hp = 100','this.health = 50'),@('Player','Avatar','cl Avatar:','this.health = 100'),@('Player.damage','hit','.hit(amount)','player.hit(10)'))) {
        Write-Source $classes
        $r=Slai @('ia',$source,'rename',$case[0],$case[1],'--json')
        $j=$r.Text | ConvertFrom-Json
        Assert ($r.Code -eq 0 -and $j.files[0].after.Contains($case[2]) -and $j.files[0].after.Contains($case[3])) "rename $($case[0])"
        Assert ([IO.File]::ReadAllText($source) -ceq $classes) 'preview is read only'
        $r=Slai @('ia',$source,'rename',$case[0],$case[1],'--apply')
        Assert ($r.Code -eq 0) "apply $($case[0]): $($r.Text)"
        $r=Slai @('check',$source)
        Assert ($r.Code -eq 0) 'changed source compiles'
    }
    Write-Source $classes
    $r=Slai @('ia',$source,'edit','Player.damage','armor','--type','int','--apply')
    $after=[IO.File]::ReadAllText($source)
    Assert ($r.Code -eq 0 -and $after.Contains('.damage(amount, armor: int)') -and $after.Contains('player.damage(10, 0)')) "edit parameter: $($r.Text)"
    $r=Slai @('run',$source)
    Assert ($r.Code -eq 0 -and $r.Text.StartsWith("90`n50`n")) 'edited method executes'
    Write-Source "x = 10`nfunc identity(x) => x`nsys.log(identity(x))`n"
    $r=Slai @('ia',$source,'rename','identity.x','value','--apply')
    Assert ($r.Code -eq 0 -and [IO.File]::ReadAllText($source) -ceq "x = 10`nfunc identity(value) => value`nsys.log(identity(x))`n") 'shadowed parameter'
    foreach($name in @('damage','return')) {
        Write-Source $classes
        $r=Slai @('ia',$source,'rename','Player.health',$name,'--json')
        Assert ($r.Code -eq 1) "reject collision $name"
    }
    foreach($extra in @("func health(x) => x.health`nsys.log(health(player))`nsys.log(health(enemy))`n","func unused(x) => x.health`n")) {
        Write-Source ($classes+"`n"+$extra)
        $r=Slai @('ia',$source,'rename','Player.health','hp','--json')
        Assert ($r.Code -eq 1 -and $r.Text.Contains('Ambiguous')) 'reject ambiguous receiver'
    }
    Write-Source ($classes+"`nsys.log(sys.ia.inspect(Player))`nsys.log(sys.ia.refs(Player.health))`nsys.log(sys.ia.errors())`n")
    $r=Slai @('run',$source)
    Assert ($r.Code -eq 0) "native queries: $($r.Text)"
    $lines=$r.Text.Trim().Split("`n")
    Assert (($lines[-3] | ConvertFrom-Json).kind -eq 'class' -and ($lines[-2] | ConvertFrom-Json).Count -ge 2 -and $lines[-1] -eq '[]') 'native query JSON includes symbolic references'
    $original=$classes+"`nchange = sys.ia.rename(Player.health, hp)`nsys.ia.apply(change)`n"
    Write-Source $original
    $r=Slai @('build',$source,'-o',$exe)
    Assert ($r.Code -eq 0 -and [IO.File]::ReadAllText($source) -ceq $original) 'compile apply without mutation'
    $r=Execute $exe @()
    $after=[IO.File]::ReadAllText($source)
    Assert ($r.Code -eq 0 -and $after.Contains('this.hp = 100') -and $after.Contains('this.health = 50')) "native apply: $($r.Text)"
    $r=Execute $exe @()
    Assert ($r.Code -eq 1 -and [IO.File]::ReadAllText($source) -ceq $after) 'stale snapshot protects edited source'
    $r=Slai @('check',$source)
    Assert ($r.Code -eq 0) 'rename also updates symbolic query operands'
    Write-Source ($classes+"`nchange = sys.ia.edit(Player.damage):`n    .param armor, int`nsys.ia.apply(change)`n")
    $r=Slai @('run',$source)
    Assert ($r.Code -eq 0 -and [IO.File]::ReadAllText($source).Contains('player.damage(10, 0)')) "native parameter edit: $($r.Text)"
    $other=Join-Path $folder 'other.slai'
    [IO.File]::WriteAllBytes($other,[byte[]](0xEF,0xBB,0xBF)+[Text.Encoding]::UTF8.GetBytes("func value() => 42`r`n"))
    Write-Source "import `$other.slai`r`nsys.log(value())`r`n"
    $r=Slai @('ia',$source,'rename','value','answer','--apply')
    Assert ($r.Code -eq 0) 'multifile rename'
    $bytes=[IO.File]::ReadAllBytes($other)
    Assert ($bytes[0] -eq 0xEF -and [Text.Encoding]::UTF8.GetString($bytes).Contains("func answer() => 42`r`n")) 'BOM and CRLF preserved'
    $r=Slai @('run',$source)
    Assert ($r.Code -eq 0 -and $r.Text -ceq "42`n") 'renamed import executes'
    $beforeMain=[IO.File]::ReadAllText($source)
    $beforeOther=[IO.File]::ReadAllBytes($other)
    $lock=[IO.File]::Open($other,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        $r=Slai @('ia',$source,'rename','answer','result','--apply')
        Assert ($r.Code -eq 1 -and [IO.File]::ReadAllText($source) -ceq $beforeMain -and [Convert]::ToBase64String([IO.File]::ReadAllBytes($other)) -eq [Convert]::ToBase64String($beforeOther)) 'rollback after second-file replacement failure'
    } finally { $lock.Dispose() }
} finally {
    $resolved=[IO.Path]::GetFullPath($folder)
    if($resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath())) -and (Split-Path $resolved -Leaf) -like 'slai-ia-test-*') { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
Write-Host "$script:count IA checks; $script:failures failures"
if($script:failures) { exit 1 }

