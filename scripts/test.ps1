param([string]$Compiler = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$Compiler) { $Compiler = Join-Path $root 'bin/slai.exe' }
$folder = Join-Path ([IO.Path]::GetTempPath()) ('slai-asm-test-' + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($folder) | Out-Null
$script:count = 0
$script:failures = 0
function Invoke-Slai([string[]]$Arguments) {
    $info = [Diagnostics.ProcessStartInfo]::new($Compiler)
    foreach ($arg in $Arguments) { $info.ArgumentList.Add($arg) }
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [Text.Encoding]::UTF8
    $info.StandardErrorEncoding = [Text.Encoding]::UTF8
    $p = [Diagnostics.Process]::Start($info)
    $stdout = $p.StandardOutput.ReadToEndAsync()
    $stderr = $p.StandardError.ReadToEndAsync()
    if (!$p.WaitForExit(15000)) { $p.Kill($true); throw 'Compiler/program timed out' }
    @{ Code = $p.ExitCode; Text = ($stdout.Result + $stderr.Result).Replace("`r`n", "`n"); Error = $stderr.Result }
}
function Case([string]$Name, [string]$Source, [string]$Expected, [int]$Status=0) {
    $path = Join-Path $folder 'main.slai'
    [IO.File]::WriteAllText($path, $Source, [Text.UTF8Encoding]::new($false))
    foreach ($release in @($false,$true)) {
        $args = @('run',$path)
        if ($release) { $args += '--release' }
        $r = Invoke-Slai $args
        $script:count++
        if ($r.Code -ne $Status -or $r.Text -cne $Expected) {
            $script:failures++
            Write-Host "FAIL $Name release=$release exit=$($r.Code): $($r.Text) $($r.Error)" -ForegroundColor Red
        }
    }
}
function Reject([string]$Source) {
    $path = Join-Path $folder 'bad.slai'
    [IO.File]::WriteAllText($path, $Source)
    $r = Invoke-Slai @('check',$path,'--json')
    $script:count++
    if ($r.Code -ne 1 -or $r.Text -notmatch '"ok":false') {
        $script:failures++
        Write-Host "FAIL rejection: $Source => $($r.Code) $($r.Text)" -ForegroundColor Red
    }
}
try {
    $directPath = Join-Path $folder 'aplicação com espaços.SLAI'
    [IO.File]::WriteAllText($directPath, 'sys.log("direto")', [Text.UTF8Encoding]::new($false))
    foreach ($release in @($false,$true)) {
        $args = @($directPath)
        if ($release) { $args += '--release' }
        $r = Invoke-Slai $args
        $script:count++
        if ($r.Code -ne 0 -or $r.Text -cne "direto`n") {
            $script:failures++
            Write-Host "FAIL direct source release=${release}: $($r.Code) $($r.Text)" -ForegroundColor Red
        }
    }
    foreach ($test in @(
        @{ Source='sys.log(1 / 0)'; Args=@($directPath); Code=1; Pattern='divisao por zero' },
        @{ Source='x = missing'; Args=@($directPath,'--json'); Code=1; Pattern='"ok":false' },
        @{ Source='sys.log("direto")'; Args=@('unknown'); Code=2; Pattern='slai file.slai' },
        @{ Source='sys.log("direto")'; Args=@($directPath,'--unknown'); Code=2; Pattern='slai file.slai' }
    )) {
        [IO.File]::WriteAllText($directPath, $test.Source, [Text.UTF8Encoding]::new($false))
        $r = Invoke-Slai $test.Args
        $script:count++
        if ($r.Code -ne $test.Code -or $r.Text -notmatch $test.Pattern) {
            $script:failures++
            Write-Host "FAIL direct source status: $($r.Code) $($r.Text)" -ForegroundColor Red
        }
    }
    $examples = @{
        hello="Hello, Slai!`n"; variables="Alex`n14`n"; loops=(0..9 -join "`n")+"`n"
        lists="Ana`n"; objects="Alex`n100`n"
        application="Alex`n75`n42`n3628800`nAna`nLucas`nPedro`nJoão`nMaria`nPartida concluída!`n"
    }
    foreach ($name in $examples.Keys) {
        Case $name ([IO.File]::ReadAllText((Join-Path $root "examples/$name.slai"))) $examples[$name]
    }
    Case arithmetic @'
sys.log(2 + 3 * 4)
sys.log(-7 / 3)
sys.log(-7 % 3)
sys.log(7 / -3)
sys.log(9223372036854775807 + 1)
sys.log(-9223372036854775808)
sys.log(123456789012345 * 7)
'@ "14`n-2`n-1`n-2`n-9223372036854775808`n-9223372036854775808`n864197523086415`n"
    Case arguments @'
func fact(n):
    if:
        n <= 1 => return 1
        el => return n * fact(n - 1)
func total(a,b,c,d,e,f,g,h) => a+b+c+d+e+f+g+h
sys.log(fact(10))
sys.log(total(1,2,3,4,5,6,7,fact(3)))
'@ "3628800`n34`n"
    Case globals @'
x = 1
func change():
    x = 5
    return 2
func pair(a,b) => a*10+b
sys.log(pair(x, change()))
x = 1
sys.log(x + change())
sys.log(x)
'@ "12`n3`n5`n"
    Case compound @'
x = 1
a = [1]
func change():
    x = 5
    a$0 = 5
    return 2
x += change()
sys.log(x)
a$0 = 1
a$0 += change()
sys.log(a$0)
'@ "3`n3`n"
    Case specialization @'
func identity(a) => a
sys.log(identity(42))
sys.log(identity("Olá"))
func even(n):
    if:
        n == 0 => return true
        el => return odd(n-1)
func odd(n):
    if:
        n == 0 => return false
        el => return even(n-1)
sys.log(even(10))
sys.log(odd(10))
'@ "42`nOlá`ntrue`nfalse`n"
    Case nestedcalls @'
func f(a,b,c,d,e,five) => a + b*10 + c*100 + d*1000 + e*10000 + five*100000
func id(x) => x
sys.log(f(id(1),id(2),id(3),id(4),id(5),id(6)))
'@ "654321`n"
    Case shortcircuit @'
sys.log("á" == "á")
sys.log("a" < "b")
sys.log("b" < "a")
sys.log(not false)
sys.log(1 != 2)
sys.log(false and (1 / 0))
sys.log(true or (1 / 0))
sys.log(0 or 12)
'@ "true`ntrue`nfalse`ntrue`ntrue`nfalse`ntrue`ntrue`n"
    Case reallocation @'
a = [0]
b = a
for i=1; <100:
    a.push(i)
a$50 += 5
sys.log(b.length)
sys.log(b$50)
sys.log(a$99)
'@ "100`n55`n99`n"
    Case nestedlists @'
a = [[1,2],[3,4]]
sys.log(a$1$0)
a$0$1 = 9
sys.log(a$0$1)
'@ "3`n9`n"
    Case classes @'
cl Player:
    .args($name):
        this.name = name
        this.health = 100
    .damage(amount) => this.health -= amount
    .alive() => this.health > 0
func create(name) => Player(name)
a = create("João")
b = create("Ana")
a.damage(20)
sys.log(a.name)
sys.log(a.health)
sys.log(b.health)
sys.log(a.alive())
players = [a,b]
sys.log(players$1.name)
'@ "João`n80`n100`ntrue`nAna`n"
    Case inlineobjects @'
jogador => nome: "Alex"; vida: 100; nivel: 5
jogador.vida -= 10
sys.log(jogador.nome)
sys.log(jogador.vida)
'@ "Alex`n90`n"
    Case loops @'
total = 0
for i=0; <5:
    if:
        i == 2 => continue
    for j=0; <5:
        if:
            j == 3 => break
        total += 1
sys.log(total)
for k=3; >0; k -= 1:
    sys.log(k)
'@ "12`n3`n2`n1`n"
    Case catch @'
func load(n):
    if:
        n < 0 => error NotFound
    return n * 2
func wrap(n) => load(n)?
x = 10
x = wrap(-1) ?! err:
    sys.log(err)
    x = 7
sys.log(x)
x = wrap(4) ?! err:
    x = 0
sys.log(x)
'@ "NotFound`n7`n8`n"
    Case catchbinding @'
func fail():
    error Failure
x = fail() ?! err:
    x = 42
sys.log(x)
'@ "42`n"
    Case unhandled @'
func fail():
    error "Falha explícita"
fail()
sys.log("não executa")
'@ "Falha explícita`n" 1
    Case rethrow @'
func fail():
    error First
func wrap():
    fail() ?! err:
        error Second
wrap() ?! err:
    sys.log(err)
sys.log("ok")
'@ "Second`nok`n"
    Case threads @'
func sum(a,b,c,d,e,five):
    sys.sleep(10)
    return a+b+c+d+e+five
a = sys.spawn(sum,1,2,3,4,5,6)
b = sys.spawn(sum,2,3,4,5,6,7)
sys.log(await a)
sys.log(await b)
sys.log(await a)
'@ "21`n27`n21`n"
    Case threaderrors @'
func fail():
    error WorkerFailed
func ok() => 42
a = sys.spawn(fail)
b = sys.spawn(ok)
x = 0
x = await a ?! err:
    sys.log(err)
sys.log(await b)
'@ "WorkerFailed`n42`n"
    Case assertion @'
sys.assert(true)
sys.assert(false) ?! err:
    sys.log("caught")
'@ "caught`n"
    Case unicode 'sys.log("\u00e1\U0001f600\x41")' "á😀A`n"
    Case divisionzero 'sys.log(1 / 0)' "Slai runtime: divisao por zero`n" 1
    Case divisionoverflow 'sys.log(-9223372036854775808 / -1)' "Slai runtime: overflow na divisao int64`n" 1
    Case listbounds "a = [1]`nsys.log(a`$1)" "Slai runtime: indice fora da lista`n" 1
    Case negativeindex "a = [1]`na`$(-1) = 3" "Slai runtime: indice de lista negativo`n" 1
    [IO.File]::WriteAllText((Join-Path $folder 'module.slai'), "sys.log(`"Módulo`")`nfunc double(x) => x * 2`n")
    Case imports @'
import $module.slai
import "module.slai"
sys.log(double(21))
'@ "Módulo`n42`n"
    foreach ($bad in @('x = (1','x = [1)','x = "unterminated','/? unfinished','x = 1.5','x = missing',
        'x, int = "a"',"x = 1`nx = `"a`"",'return 1','break','sys.log()','x = []','x = [1,"a"]',
        'sys.log("\0")','func f(a,a) => a','x = 9223372036854775808','func f() => missing',
        'func f(a: float) => a','func f(f) => f','x = 1?',"x = 1`nx.nope = 2",'sys.ia.unknown()',
        "if:`n    false => x = 1`nsys.log(x)",
        "func f(x):`n    if:`n        x > 0 => return x`nsys.log(f(1))")) { Reject $bad }
} finally {
    # Only the exact GUID-named directory created by this test is removed.
    $resolved = [IO.Path]::GetFullPath($folder)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($tempRoot) -and (Split-Path $resolved -Leaf) -like 'slai-asm-test-*') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
Write-Host "$script:count checks; $script:failures failures"
if ($script:failures) { exit 1 }

