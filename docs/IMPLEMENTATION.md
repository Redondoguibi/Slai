# Estado da implementação — Assembly x86-64

A toolchain ativa é **Slai 0.2.0-asm**, escrita em Assembly x86-64 para Windows
x64. A exigência de Assembly é uma decisão do autor. As demais convenções
provisórias abaixo não encerram decisões ainda abertas em `SLAI_SPEC.md`.

## Pipeline e fontes

```text
.slai UTF-8 -> lexer -> AST -> escopos/tipos/especializações -> SIR
            -> constant folding opcional -> bytes x64 -> PE32+ próprio
```

| Parte | Implementação |
| --- | --- |
| Tokens, indentação, escapes e posições | `asm/lexer.inc` |
| AST e gramática | `asm/parser.inc` |
| Resolução de nomes, inclusive corpos não utilizados | `asm/names.inc` |
| Tipos, escopos e SIR | `asm/semantic.inc`, `expressions.inc`, `statements.inc`, `functions.inc` |
| Otimização de constantes | `asm/optimizer.inc` |
| Encoder direto de instruções x64 | `asm/encoder.inc` |
| Cabeçalhos, seções, imports e unwind PE | `asm/pe.inc`, `imports.asm` |
| Console, listas, erros e threads | `asm/runtime.asm` |
| Índice, consultas e propostas de mudança | `asm/index.inc`, `ia.inc`, `changes.inc` |
| Aplicação de snapshots e rollback | `asm/runtime_apply.inc` |
| CLI, arquivos e JSON | `asm/cli.inc`, `support.inc`, `json.inc` |

`asm/slai.asm` reúne o compilador. FASM monta o runtime e a tabela de imports
como blobs binários e os incorpora ao compilador. Durante `slai build`, o
compilador utiliza esses dados e emite os bytes das funções e do PE diretamente;
não cria Assembly intermediário para enviar ao FASM, nem executa outro backend.
PowerShell é usado somente para preparação, montagem e testes.

## Convenções de linguagem

- `int` é assinado de 64 bits. Soma, subtração e multiplicação usam wrap;
  divisão trunca em direção a zero, e o resto acompanha o dividendo.
- Variáveis e campos são mutáveis, mas seus tipos não mudam após a primeira
  atribuição. Listas são homogêneas; listas vazias não têm inferência nesta versão.
- Anotações disponíveis: `int`, `bool`, `string`. Parâmetros podem usar a
  extensão provisória `func f(x: int) => x`.
- `$` em parâmetros é opcional. `$Nome` em expressões é uma string curta.
  `lista$indice` começa em zero; `lista$(i+1)`, índices encadeados e campos após
  índices são aceitos.
- `.nome = valor` equivale provisoriamente a `nome = valor`.
- A última expressão de uma função retorna seu valor. Atribuições finais não
  retornam um valor. Caminhos sem retorno são recusados quando há retorno tipado.
- Funções e métodos são especializados pelos tipos dos argumentos. Recursão
  reutiliza um tipo de retorno conhecido; ciclos sem tipo inicialmente assumem
  `int` e validam os retornos encontrados.
- `if` escolhe a primeira condição verdadeira; `el` deve ser o último ramo.
  Condições aceitam `bool` ou `int`; operadores lógicos têm curto-circuito.
- `for i=0; <10:` incrementa em 1. O passo explícito, como
  `for i=3; >0; i-=1:`, é uma extensão provisória. `continue` executa o passo.
- Novos nomes de ramos e laços ficam no bloco. Funções enxergam globais que
  já existem quando a especialização é solicitada. Argumentos e operandos são
  avaliados da esquerda para a direita e capturados antes das próximas expressões.
- Listas, objetos e instâncias são compartilhados por referência. Campos são
  fixos; campos de classe são introduzidos no construtor `.args`, fora de laços
  e condicionais. Não há herança.
- Strings são imutáveis e UTF-8. Comparação é lexicográfica por bytes. Escapes
  usuais, `\xNN`, `\uNNNN` e `\UNNNNNNNN` são decodificados; NUL e codepoints
  inválidos são recusados. Colunas dos tokens são medidas em bytes UTF-8.
- Imports são relativos ao arquivo importador, carregam uma vez e compartilham
  o namespace. Ciclos e colisões são erros. BOM UTF-8 e CRLF são preservados nas
  refatorações. Não há resolução de pacotes.

## Runtime, erros e concorrência

`sys.log` imprime um `int`, `bool` ou `string`. Listas oferecem `push` e `length`,
com crescimento geométrico e validação de índices. Divisão por zero, overflow
`INT64_MIN / -1` e índices inválidos são falhas fatais com código 1.

`error Nome` e `error "mensagem"` propagam um erro textual. `?! nome:` executa
um handler e consome o erro. `?` propaga para o chamador. `sys.assert` produz um
erro tratável. Um nome criado numa operação protegida só pode ser utilizado
após o handler se houver atribuição nos dois caminhos.

`sys.spawn(funcao, argumentos...)` cria uma thread Win32. `await` espera sua
conclusão e recupera resultado/erro; esperas repetidas são aceitas. O estado de
erro é armazenado em TLS. Listas e objetos compartilhados não têm sincronização
automática; escritas concorrentes precisam ser evitadas nesta versão.
`sys.sleep` usa milissegundos. Não há event loop, executor de promises nem HTTP.

Alocações de objetos/listas permanecem até o fim do processo. Não há GC,
ownership ou contagem de referências. Esta é uma implementação experimental
para programas pequenos, não um modelo definitivo de gerenciamento de memória.

## `sys.ia` e CLI

Consultas: `inspect`, `refs`, `tree`, `type`, `dependencies`, `errors`.
O índice contém declarações, referências de nomes e membros, tipos observados
nas especializações e localizações de fonte. A consulta de árvore retorna a
AST disponível. Corpos genéricos não chamados têm nomes verificados, mas nem
sempre têm tipos determinados; `check` não prova todas as futuras especializações.

Mudanças:

```slai
change = sys.ia.rename(Player.health, hp)
sys.log(sys.ia.preview(change))
sys.ia.apply(change)
```

```slai
change = sys.ia.edit(Player.damage):
    .param armor, int
sys.ia.apply(change)
```

A edição adiciona um parâmetro e atualiza chamadas diretas, usando `0`, `false`
ou `""` como argumento padrão. Renomeações usam vínculos semânticos, preservam
comentários/strings e distinguem membros de classes diferentes. Referências
polimórficas ou genéricas sem dono determinado fazem a refatoração ser recusada.
O preview contém os textos completos antes/depois, em vez de um diff por hunks.

A compilação captura os bytes originais sem escrever nos fontes. A aplicação
valida todos os snapshots, prepara arquivos temporários ao lado dos destinos e
substitui os arquivos. Falha de substituição aciona rollback dos arquivos já
alterados. Se também houver falha de I/O durante a recuperação, o runtime a
informa explicitamente. Isso não é uma transação resistente a queda de energia
ou a escritores concorrentes que alterem arquivos durante o commit.

CLI correspondente:

```powershell
.\slai.cmd ia app.slai inspect Player
.\slai.cmd ia app.slai rename Player.health hp --json
.\slai.cmd ia app.slai rename Player.health hp --apply
.\slai.cmd ia app.slai edit Player.damage armor --type int --apply
```

`slai arquivo.slai [--release]` compila e executa diretamente, como `run`.
`build`, `run`, `check`, `inspect` e `sir` continuam disponíveis. Erros são
escritos em stderr; `--json` retorna `ok` e `diagnostics`, com localização,
mensagem e código. Os schemas de inspeção/SIR evoluíram em relação ao protótipo
histórico e não são uma API estável.

## ABI, PE e otimização

A geração usa a ABI Windows x64: RCX/RDX/R8/R9 para os primeiros argumentos,
pilha para os demais, shadow space e alinhamento de 16 bytes. Retorno em RAX.
Valores da SIR ficam em slots de pilha; ainda não há alocador de registradores.

O PE contém `.text`, `.idata`, `.data`, `.rdata` e `.pdata`, com `UNWIND_INFO`
para as funções geradas e o runtime. Há base fixa, NX e relocations desativadas;
as referências absolutas usam regiões virtuais pré-reservadas. O arquivo em
disco contém somente os bytes necessários, alinhados a 512 bytes. Não há ASLR
ou símbolos de depuração. Os limites de frame dispensam stack probing.

O runtime importa rotinas de `KERNEL32.dll` e `msvcrt.dll` para serviços do SO e
biblioteca. Elas não são compiladores nem backends. O compilador também usa
`SHELL32.dll` para argumentos Unicode. Nenhum desses componentes chama Python.

`--release` propaga/funde constantes em valores escritos uma única vez, incluindo
aritmética, comparações e operações unárias. Não assume que globais ou locais
mutáveis sejam constantes. Divisões que falhariam mantêm suas guardas de runtime.
Otimizações adicionais do protótipo interrompido, como inlining e eliminação de
código morto, não foram portadas; não fazem parte do modo release atual.

Limites explícitos: 1 MiB por fonte; até 16 argumentos por chamada; 400 slots por
função; 256 funções/especializações por compilação; 1 MiB por buffer de código
individual, constantes ou saída de tooling; região total de código inferior a
4 MiB. Os temporários de edição usam APIs Win32 que também podem impor limites
de caminho. Exceder os limites gera um diagnóstico, sem fallback para outra
linguagem.

## Pendências do projeto

A migração não conclui toda a linguagem documentada. Continuam pendentes:
closures/funções como valores, floats, generics explícitos, interfaces, herança,
construção recursiva da mesma classe, modelo definitivo de memória, biblioteca
HTTP/`fetch`, `sys.ia.create/remove/move`, comandos `fmt`/`test`, debugger,
integração de editor, ASLR e otimizações avançadas do roadmap.

O protótipo anterior está preservado em `legacy/python-prototype/` e não integra
o build ou a execução. Seus testes permanecem como referência histórica.

## Verificação

Após `scripts/build.ps1`, execute em PowerShell 7:

```powershell
pwsh -File scripts/test.ps1
pwsh -File scripts/test-ia.ps1
pwsh -File scripts/test-pe.ps1
```

São 142 verificações na suíte atual. Ela compara saída/status dos programas
nativos normais e release, testa imports, Unicode, escopos, avaliação, classes,
listas, recursão, erros, threads, refatorações, snapshots e rollback entre
arquivos. O teste PE verifica a estrutura e a otimização e monta um verificador
Assembly que chama `RtlVirtualUnwind` do Windows para cada função emitida.

Referências técnicas usadas para ABI e formato:
[PE](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format),
[calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention),
[unwind](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64),
[prólogo/epílogo](https://learn.microsoft.com/en-us/cpp/build/prolog-and-epilog).
