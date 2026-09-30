# Slai

Compilador experimental **Slai 0.1**, com geração própria de machine code x86-64
e executáveis PE32+ para **Windows x64**.

O compilador está implementado em Python 3.11+ e usa somente a biblioteca padrão.
O programa compilado é um `.exe` nativo: **não depende de Python**, de um
interpretador Slai, de uma VM ou de arquivos deste repositório para executar.
Não há LLVM, Cranelift, transpiler, assembler ou linker externo no pipeline.

## Usar no Windows

Abra o PowerShell nesta pasta (`Slai/workspace`):

```powershell
.\slai.cmd run examples\application.slai
.\slai.cmd build examples\application.slai --release
.\examples\application.exe
```

O launcher usa o comando `python`. Alternativa equivalente:

```powershell
python -m slai run examples/hello.slai
python -m slai build examples/application.slai -o application.exe
python -m slai check examples/application.slai
```

`run` compila para um diretório temporário, executa o `.exe` e devolve seu código
de saída. `build` mantém o executável no caminho informado, ou junto do fonte.
Não é necessário instalar dependências nem um compilador C/C++.

Para disponibilizar `slai` como comando no ambiente Python, opcionalmente:

```powershell
python -m pip install -e .
slai build examples/hello.slai
```

## Exemplo

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

jogadores = [$Ana, $Lucas, $Pedro]
jogadores.push("João")
for i=0; <jogadores.length:
    sys.log(jogadores$i)
```

## Implementado

- Lexer com comentários, strings, indentação e localização de erros.
- Parser/AST para variáveis, expressões, chamadas, funções, condicionais, laços,
  listas, objetos, classes e imports de arquivos `.slai`.
- Inferência, anotações `int`, `bool`, `string`, escopos e resolução de nomes.
- Especialização de funções pelos tipos dos argumentos, recursão, métodos,
  construtores, retorno explícito e retorno da última expressão.
- SIR tipada e constant folding de aritmética com `--release`.
- Backend x86-64 próprio: chamadas, branches, aritmética, stack frames e ABI
  Windows x64, inclusive argumentos além dos quatro registradores.
- PE próprio com imports de DLL e informações de unwind.
- `sys.log`, strings UTF-8, listas expansíveis, objetos e classes por referência.
- Erros controlados de divisão e limites de lista.
- CLI `build`, `run`, `check`, `inspect` e `sir`, com diagnósticos JSON.

## Ferramentas para agentes

```powershell
.\slai.cmd check examples/application.slai --json
.\slai.cmd inspect examples/application.slai
.\slai.cmd sir examples/application.slai --release
```

`inspect` expõe símbolos, tipos, posições, referências, layouts de objetos e
especializações do compilador. `sir` mostra a representação interna em JSON.
`check --json` retorna diagnóstico com arquivo, linha, coluna, código e mensagem.
Esses comandos são a base inicial de tooling semântico; a API `sys.ia` dentro
do código Slai, incluindo renomeação e aplicação de mudanças, **não está implementada**.

## Convenções e limites

A especificação original é um rascunho com decisões em aberto. As escolhas
necessárias para executar esta versão estão separadas em
[docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md), sem transformar propostas
em decisões definitivas da linguagem.

Async/`await`, tratamento/propagação de erros, closures, herança, generics
explícitos, floats, GC e o conjunto completo de otimizações do roadmap ainda
não estão disponíveis. Objetos/listas são mantidos até o fim do processo.
Esta versão é voltada a experimentação e programas pequenos.

Os exemplos originais foram preservados. `conditions.slai` referencia
`morrer`/`continuar` sem defini-las e, portanto, produz erro de nome.
`async.slai` produz um diagnóstico de recurso não implementado.
`functions.slai` e `classes.slai` só declaram símbolos, sem saída.
O exemplo [application.slai](examples/application.slai) é executável e reúne
funções, recursão, classes, listas, objetos e controle de fluxo.

## Testes

```powershell
python -m unittest discover -s tests -v
```

Os testes nativos geram executáveis temporários e verificam saída e código de
saída no Windows x64, tanto em modo normal quanto com `--release`. Em outros
hosts esses testes são explicitamente marcados como ignorados. Os testes de
frontend, imports, diagnósticos e estrutura PE não precisam executar binários.

## Organização

```text
slai/
    frontend.py   lexer, tokens, AST, parser e diagnósticos
    semantic.py   escopos, tipos, símbolos, SIR e constant folding
    native.py     encoder x86-64, runtime nativo e gerador PE
    __main__.py   CLI
tests/            testes de frontend e execução nativa
examples/         exemplos de sintaxe e aplicação executável
docs/             especificações e estado da implementação
```

Consulte [SLAI_SPEC.md](docs/SLAI_SPEC.md), [SYNTAX.md](docs/SYNTAX.md),
[ROADMAP.md](docs/ROADMAP.md), [AI_INTERFACE.md](docs/AI_INTERFACE.md) e
[PERFORMANCE_AND_COMPILATION.md](docs/PERFORMANCE_AND_COMPILATION.md).
