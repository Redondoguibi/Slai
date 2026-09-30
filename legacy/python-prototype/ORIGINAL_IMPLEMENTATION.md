# Slai 0.1 — estado da implementação

Este documento descreve o compilador executável do repositório. Não substitui
as decisões pendentes de `SLAI_SPEC.md`. Toda escolha abaixo sobre um item
pendente é **provisória**, para permitir testar programas reais.

## Pipeline real

```text
arquivo .slai (UTF-8)
  -> tokens com posições e indentação
  -> AST
  -> resolução de nomes / inferência / especialização
  -> SIR tipada
  -> constant folding opcional
  -> bytes x86-64
  -> PE32+ Windows x64
```

O código Python hospeda o compilador. Nenhum programa Slai é avaliado pelo
Python e não há tradução para Python, C ou outra linguagem. A SIR é uma
estrutura de dados de compilação, sem interpretador ou executor.

## Convenções provisórias

| Tema | Comportamento da versão 0.1 |
| --- | --- |
| Arquivos | `.slai`, texto UTF-8; BOM inicial aceito |
| Blocos | Indentação por espaços; tabs na indentação são erro |
| Tipos | `int` assinado de 64 bits, `bool`, `string`; listas e registros inferidos |
| Mutabilidade | Variáveis e campos mutáveis; o tipo não muda após a primeira atribuição |
| Inteiros | `+`, `-`, `*` com wrap em 64 bits; divisão trunca em direção a zero; resto acompanha o dividendo |
| Falhas aritméticas | Divisão por zero e `INT64_MIN / -1` terminam com código 1 |
| `$` nos parâmetros | Marcador opcional, sem mudar passagem ou tipo |
| `$Nome` em expressões | String curta equivalente a `"Nome"`; não é referência a variável |
| `lista$indice` | Indexação iniciada em zero; índice negativo ou fora da lista é erro de runtime |
| Índice composto | `lista$(i + 1)`; `lista$0.nome` e `listas$0$1` também funcionam |
| `.nome = valor` | Mesmo comportamento de `nome = valor`; não declara propriedade implicitamente |
| Retorno | `return valor`; última expressão de uma função também retorna; atribuição final não retorna |
| Parâmetros anotados | Extensão provisória `func soma(a: int, b: int) => a + b` |
| Especialização | Funções/métodos são compilados por combinação de tipos dos argumentos |
| Recursão | Tipos de retorno já conhecidos são reutilizados; ciclos ainda sem tipo assumem `int` e validam os retornos |
| Condicionais | Primeira condição verdadeira vence; `el` opcional e último; condições `bool` ou `int` |
| Ações em bloco | Além de `cond => ação`, aceita `cond:` seguido de um bloco indentado |
| Laços | `for i=0; <10:` reavalia a condição e incrementa `i` em 1 |
| Passo explícito | Extensão `for i=10; >0; i -= 1:` |
| Controle de laço | `break` e `continue`; `continue` executa o passo do laço |
| Escopos | Nomes novos de if/for são locais ao bloco; nomes existentes podem ser atualizados |
| Globais | Declarações no topo ficam acessíveis às funções; devem existir quando a função é especializada |
| Avaliação | Esquerda para direita; argumentos/operandos são capturados antes das próximas expressões |
| Booleanos | `true`, `false`, `not`/`!`, `and`/`&&`, `or`/`||`; operadores lógicos têm curto-circuito |
| Listas | Homogêneas, mutáveis, com `.push(valor)` e `.length`; vazias não têm inferência nesta versão |
| Objetos | Campos fixos, acesso por ponto; variantes inline e `obj` |
| Classes | `cl`, construtor `.args`, `this` e métodos; sem herança |
| Campos de classe | Declarados por atribuições diretas no construtor, fora de condicionais/laços |
| Referências | Listas, objetos e instâncias são compartilhados por referência |
| Strings | Imutáveis, UTF-8; comparação lexicográfica por bytes; sem NUL embutido |
| Imports | `import $arquivo.slai` ou `import "pasta/arquivo.slai"`, relativos ao arquivo importador |
| Módulos | Importa o arquivo inteiro para o mesmo namespace; carrega uma vez; ciclos e colisões são erros |
| `sys.log` | Um argumento `int`, `bool` ou `string`, seguido de quebra de linha |

Os exemplos originais não foram reescritos para esconder erros ou dependências.
O exemplo de condições exige que o programa declare `morrer` e `continuar`.

## Análise e tooling

Nomes, aridade de chamadas diretas e sintaxe são verificados inclusive nas
funções não utilizadas. A análise de tipos dos corpos genéricos e de métodos
ocorre quando eles são especializados por chamadas. Assim, `check` não prova
a validade de todas as futuras combinações de tipos de uma função não chamada.

A tabela de símbolos inclui declarações, parâmetros, variáveis e referências
dos corpos especializados. Os layouts de registros incluem campos e métodos.
`inspect` disponibiliza esses dados em JSON; ainda não é um índice completo
de referências a propriedades nem uma API de refatoração.

Diagnósticos do compilador vão para stderr e retornam código 1. O formato JSON
usa UTF-8 mesmo quando redirecionado no Windows. Erros de runtime imprimem uma
mensagem e terminam com código 1; são guardas de execução, não o sistema de
exceções proposto nos documentos.

## Backend e ABI

O encoder emite diretamente opcodes, ModR/M, SIB e deslocamentos. Valores da SIR
usam slots de pilha; é uma alocação simples, sem alocador otimizado de registradores.
O backend preserva RBP, mantém a stack alinhada em 16 bytes e reserva shadow space.
Os primeiros quatro argumentos usam RCX/RDX/R8/R9; os demais usam a pilha.
Retornos usam RAX. Variáveis globais ficam em `.data`.

O PE contém `.text`, `.rdata`, `.idata`, `.data` e `.pdata`, com `UNWIND_INFO`
para os frames emitidos. Referências a dados e código são RIP-relative.
O formato tem base fixa e relocations desativadas nesta versão. Usa NX,
mas ainda não implementa ASLR, símbolos de debug nem stack probing; frames
maiores que 4000 bytes são rejeitados explicitamente. A especialização tem
limite de 256 instâncias por compilação.

DLLs de sistema importadas:

- `KERNEL32.dll`: `ExitProcess` e `SetConsoleOutputCP`.
- `msvcrt.dll`: `printf`, `fflush`, `malloc`, `realloc` e `strcmp`.

A CRT do Windows fornece pequenas rotinas de biblioteca em runtime; **não é
usada como compilador ou backend**. O PE continua sendo gerado integralmente
pela Slai. Strings são impressas com formato fixo `%s`, inclusive quando o
texto do usuário contém `%`.

## Memória e otimização

Listas usam descritor estável `{length, capacity, data}` e crescimento geométrico.
O descritor permanece válido para todos os aliases após `realloc`.
Objetos e instâncias usam um bloco de campos de oito bytes.
Alocações são verificadas; falha de memória termina o processo.

O heap é liberado pelo sistema ao término do processo. Não existe coleta
durante a execução, ownership ou reference counting. Loops que alocam
indefinidamente podem esgotar memória. O modelo definitivo continua pendente.

`--release` aplica constant folding de aritmética inteira em temporários
imutáveis. Não propaga variáveis mutáveis nem elimina guardas de divisão.
Inlining, escape analysis, eliminação de código morto, vetorização e outras
otimizações do roadmap permanecem futuras.

## Fora da implementação atual

- `await`, `fetch`, promises e concorrência;
- `?!`, propagação `?` e `error`;
- API `sys.ia` executada por programas Slai e suas operações de edição;
- closures, funções como valores, overload explícito e lambdas;
- floats, generics explícitos, interfaces e herança;
- construção recursiva da mesma especialização de classe;
- gerenciamento definitivo de memória;
- comandos `fmt`/`test`, debugger e integração com editor.

Esses recursos não são simulados. As sintaxes reconhecidas produzem diagnóstico
de recurso indisponível; símbolos de bibliotecas inexistentes produzem erro de nome.

## Referências do formato de saída

- [Microsoft: Windows x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention)
- [Microsoft: PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
- [Microsoft: x64 exception handling](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64)

## Validação

Execute `python -m unittest discover -s tests -v` no diretório do repositório.
A suíte compara os programas nativos em compilação normal e otimizada e cobre:

- exemplos originais executáveis e uma aplicação integrada;
- strings UTF-8, inteiros extremos, divisão e curto-circuito;
- recursão, recursão mútua, especialização e mais de quatro argumentos;
- globals, ordem de avaliação e chamadas aninhadas;
- listas aninhadas, crescimento, aliases e limites;
- classes, métodos, objetos retornados e campos mutáveis;
- imports, ciclos, escopos e diagnósticos estruturados;
- assinaturas, seções e determinismo do PE gerado.
