# Slai — Roadmap

> Roadmap inicial para uma linguagem pessoal. Não é um cronograma e pode mudar conforme o design evoluir.

## Fase 0 — Design da linguagem

Objetivo: estabilizar uma primeira versão coerente da Slai antes de implementar o compilador.

Prioridades:

- definir `$`;
- definir o prefixo `.`;
- definir tipos primitivos;
- definir mutabilidade;
- definir retorno de funções;
- definir condicionais em bloco;
- definir laços;
- fechar sistema de erros;
- escolher extensão dos arquivos;
- definir módulos/imports;
- definir modelo básico de classes/objetos.

Arquivos principais:

- `SLAI_SPEC.md`
- `SYNTAX.md`
- `AI_INTERFACE.md`
- `examples/`

## Fase 1 — Lexer

Objetivo: transformar código-fonte Slai em tokens.

Primeiros tokens esperados:

- identificadores;
- números;
- strings;
- `?`;
- `/?` e `?/`;
- `=`;
- `=>`;
- `:`;
- `;`;
- `.`;
- `$`;
- parênteses;
- colchetes;
- operadores matemáticos e comparativos;
- palavras-chave.

## Fase 2 — Parser

Objetivo: transformar tokens em uma árvore sintática.

Primeiros elementos:

- variáveis;
- chamadas;
- funções;
- `if`;
- `for`;
- listas;
- objetos;
- classes;
- imports.

## Fase 3 — AST e modelo semântico

Objetivo: representar programas de forma estruturada.

Essa fase deve considerar desde cedo a futura integração com `sys.ia`.

Elementos:

- nós da AST;
- símbolos;
- escopos;
- referências;
- resolução de nomes.

## Fase 4 — Sistema de tipos

Objetivo: implementar inferência e tipos explícitos.

Primeira meta:

```slai
idade = 14
nome, string = "Alex"
```

O compilador deve compreender os tipos mesmo quando não são escritos explicitamente.

## Fase 5 — Interpretador inicial

Objetivo: conseguir executar programas Slai simples rapidamente, antes de buscar performance máxima.

Primeiros recursos:

- variáveis;
- operações;
- funções;
- condicionais;
- laços;
- listas;
- objetos;
- `sys.log`.

## Fase 6 — Classes e módulos

Objetivo:

- `cl`;
- `.args`;
- métodos;
- `this`;
- imports;
- múltiplos arquivos.

## Fase 7 — Erros e async

Objetivo:

- fechar e implementar o sistema de erros;
- implementar `await`;
- definir o modelo de funções assíncronas.

## Fase 8 — `sys.ia`

Objetivo: disponibilizar a primeira API semântica da Slai para agentes.

Primeira versão candidata:

```text
inspect
refs
errors
rename
preview
apply
```

## Fase 9 — Tooling

Possíveis ferramentas:

```text
slai run
slai check
slai fmt
slai test
```

Ainda não são decisões definitivas.

Também:

- formatter;
- diagnostics;
- LSP;
- integração com editor.

## Fase 10 — Compilação nativa

Somente depois de a linguagem estar utilizável e coerente.

Decisões futuras:

- LLVM;
- Cranelift;
- backend próprio;
- bytecode;
- JIT/AOT.

## Regra geral

Para este estágio do projeto:

> Primeiro tornar a Slai agradável e coerente para uso pessoal. Depois expandir ambições de ecossistema, distribuição e adoção pública.
