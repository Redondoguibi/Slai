# Slai

Compilador experimental **Slai 0.2.0-asm**, escrito em **Assembly x86-64**, para
Windows x64. Lexer, parser, análise semântica, SIR, otimização, encoder, gerador
PE, CLI e runtime estão em `asm/`.

O compilador gera machine code e executáveis PE32+ diretamente. Não usa Python,
LLVM, transpiler, VM ou backend externo. FASM é usado apenas para montar a
própria toolchain durante o desenvolvimento. Os executáveis usam bibliotecas
do Windows para arquivos, memória, console e threads.

## Executar

Para compilar e executar um arquivo:

```console
slai examples/application.slai
```

No PowerShell, na raiz do projeto, use `.\slai.cmd` se a pasta ainda não
estiver no `PATH`:

```powershell
.\slai.cmd --version
.\slai.cmd examples/application.slai
.\slai.cmd build examples\application.slai --release -o application.exe
.\application.exe
```

`slai.cmd` chama `bin\slai.exe`. Passar um arquivo `.slai` diretamente equivale
a `run`: compila um PE temporário, executa-o e
retorna seu código de saída. `build` grava ao lado do fonte por padrão e
substitui um executável anterior somente depois de concluir a compilação e a
escrita do novo arquivo.

## Montar a toolchain

Em um clone novo, obtenha o FASM oficial e monte o compilador:

```powershell
powershell -File scripts\bootstrap.ps1
```

O script verifica o SHA-256 da distribuição fixada. Para reconstruir:

```powershell
powershell -File scripts\build.ps1
```

Também é possível informar um FASM já instalado:

```powershell
powershell -File scripts\build.ps1 -Fasm C:\fasm\FASM.EXE
```

Não é necessário instalar Python, um compilador C/C++ ou LLVM.

## Linguagem disponível

- Inteiros de 64 bits, booleanos, strings UTF-8, inferência e anotações de tipos.
- Funções especializadas por tipos, recursão, classes, métodos e construtores.
- Condicionais, laços, `break`, `continue`, listas, objetos e imports.
- `error`, propagação `?`, tratamento `?!` e `sys.assert`.
- `sys.spawn`, `await` e `sys.sleep`, com threads reais e erros por thread.
- `sys.ia.inspect`, `refs`, `tree`, `type`, `dependencies` e `errors`.
- `sys.ia.rename`, `edit`, `preview` e `apply`, com mudanças baseadas em símbolos.
- Constant folding com `--release`; PE com imports e informações de unwind.

```slai
func somar($a, $b) => a + b

cl Player:
    .args($name):
        this.name = name
        this.health = 100
    .damage(amount) => this.health -= amount

player = Player("Alex")
player.damage(25)
sys.log(player.name)
sys.log(player.health)
sys.log(somar(20, 22))
```

## Tooling

```powershell
.\slai.cmd check examples\application.slai --json
.\slai.cmd inspect examples\application.slai
.\slai.cmd sir examples\application.slai --release
.\slai.cmd ia examples\application.slai inspect Player
.\slai.cmd ia examples\application.slai refs Player.health
.\slai.cmd ia examples\application.slai rename Player.health hp --json
```

A última chamada retorna a proposta, incluindo os arquivos antes/depois, sem
alterar fontes. `--apply` aplica uma proposta da CLI. Dentro de um programa,
`sys.ia.apply(change)` aplica o snapshot somente durante a execução; compilar
esse programa não modifica seu código-fonte. Fontes alterados desde a compilação
fazem a aplicação falhar. Referências ambíguas e colisões de nomes são recusadas.

Diagnósticos ficam em **stderr**; `--json` inclui arquivo, linha, coluna, código e
mensagem. `inspect` expõe um inventário de símbolos com referências e tipos;
`sir` mostra funções e instruções. Os formatos de tooling são experimentais.

## Testar

As suítes usam **PowerShell 7** e executam os binários no Windows x64:

```powershell
pwsh -File scripts\test.ps1
pwsh -File scripts\test-ia.ps1
pwsh -File scripts\test-pe.ps1
```

A validação atual contém 142 verificações: linguagem/runtime em modo normal e
release, refatorações, snapshots, rollback entre arquivos, estrutura PE,
otimização e unwind pelo próprio Windows. O verificador de unwind também é
escrito em Assembly.

## Estado e organização

Esta migração troca a linguagem de implementação da toolchain; não significa
que todo o roadmap da Slai esteja concluído. Convenções provisórias, limites e
recursos pendentes estão em [docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md).

```text
asm/                        compilador e runtime Assembly
bin/slai.exe                compilador montado localmente
scripts/                    bootstrap, build e testes PowerShell
tests/unwind.asm            verificação da ABI pelo Windows
examples/                   exemplos Slai
docs/                       especificação e estado real
legacy/python-prototype/    protótipo anterior preservado para referência
```

O protótipo histórico não participa de nenhuma etapa da toolchain ativa.
Os exemplos originais foram preservados: `conditions.slai` precisa das funções
`morrer`/`continuar`, e `async.slai` referencia `fetch`/`url`, que não estão
implementados. [application.slai](examples/application.slai) é o exemplo integrado
executável.
