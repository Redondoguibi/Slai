# Slai — Roadmap

> Roadmap inicial de uma linguagem pessoal. Não é um cronograma.

## Decisões arquiteturais fixadas

Por enquanto:

- alvo único: **Windows x64**;
- compilação AOT;
- geração direta de **x86-64 machine code**;
- geração de executável **PE** pela própria toolchain Slai;
- sem LLVM;
- sem Cranelift;
- sem transpilar para C/C++/JavaScript;
- sem bytecode;
- sem JIT;
- sem interpretador como etapa oficial da linguagem.

## Fase 0 — Design

Fechar primeiro:

- `$`;
- prefixo `.`;
- tipos primitivos;
- mutabilidade;
- retornos;
- condicionais;
- laços;
- erros;
- extensão;
- módulos/imports;
- classes/objetos;
- memória.

Documentação:

- `docs/SLAI_SPEC.md`
- `docs/SYNTAX.md`
- `docs/AI_INTERFACE.md`
- `docs/PERFORMANCE_AND_COMPILATION.md`
- `examples/`

## Fase 1 — Lexer

Transformar código Slai em tokens.

Inclui:

- identificadores;
- números;
- strings;
- comentários;
- `=`;
- `=>`;
- `:`;
- `;`;
- `.`;
- `$`;
- parênteses;
- colchetes;
- operadores;
- palavras-chave.

## Fase 2 — Parser

Construir AST para:

- variáveis;
- chamadas;
- funções;
- if;
- for;
- listas;
- objetos;
- classes;
- imports.

## Fase 3 — Semântica

Criar:

- escopos;
- símbolos;
- resolução de nomes;
- referências;
- validação semântica;
- base para `sys.ia`.

## Fase 4 — Tipos

Implementar:

- inferência;
- tipos explícitos;
- erros de tipo;
- representação interna dos tipos.

## Fase 5 — SIR

Criar a **Slai Intermediate Representation**.

A SIR é interna à Slai e serve para:

- simplificar geração de machine code;
- aplicar otimizações;
- fornecer dados estruturados para tooling/`sys.ia`.

Não é LLVM IR nem bytecode executável.

## Fase 6 — Backend x86-64 mínimo

Primeiro objetivo real de execução:

```text
Slai
 ↓
SIR
 ↓
x86-64 machine code
```

Implementar inicialmente:

- encoding de instruções;
- registradores;
- operações aritméticas;
- stack frames;
- chamadas de função;
- Windows x64 calling convention;
- retorno de funções.

## Fase 7 — Gerador PE

Gerar um executável Windows diretamente.

Implementar:

- DOS header/stub mínimo;
- PE signature;
- COFF header;
- Optional Header PE32+;
- Section Table;
- `.text`;
- `.rdata`;
- `.data` quando necessário;
- `.idata`/imports quando necessário;
- entry point;
- alinhamentos;
- RVAs.

Meta:

```bash
slai build hello.slai
```

produzir:

```text
hello.exe
```

sem compilador ou linker externo como backend.

## Fase 8 — Runtime mínimo e `sys`

Implementar apenas o necessário.

Primeiras metas:

- `sys.log`;
- strings;
- memória básica;
- acesso às APIs necessárias do Windows.

O runtime deve permanecer pequeno.

## Fase 9 — Recursos da linguagem

Adicionar progressivamente:

- listas;
- objetos;
- classes;
- imports;
- múltiplos arquivos;
- erros;
- async.

## Fase 10 — Otimizador Slai

Adicionar otimizações próprias:

- constant folding;
- constant propagation;
- dead code elimination;
- inlining;
- escape analysis;
- stack allocation;
- especialização;
- devirtualization;
- loop optimizations;
- vectorization futuramente.

## Fase 11 — `sys.ia`

Primeira versão candidata:

```text
inspect
refs
errors
rename
preview
apply
```

Ela reutilizará AST, símbolos, tipos e SIR do próprio compilador.

## Fase 12 — Tooling

Possíveis comandos:

```text
slai build
slai run
slai check
slai fmt
slai test
```

`slai run` deverá compilar para código nativo e então executar o binário; não implica interpretador/JIT.

## Regra geral

> Primeiro tornar a Slai excelente para uso pessoal no Windows x64. Portabilidade não é prioridade atual.
