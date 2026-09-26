# Slai — AI Interface

> Design inicial de `sys.ia`.

## Objetivo

`sys.ia` é o espaço reservado para recursos projetados especificamente para IA e agentes programadores.

A meta é permitir que um agente trabalhe com o projeto de maneira **semântica**, não apenas como texto bruto.

```text
Class(Player)
Property(health)
Method(damage)
Parameter(amount)
```

## Princípio

> Funcionalidades especificamente destinadas a agentes de IA ficam sob `sys.ia`.

A sintaxe normal da Slai continua voltada para humanos.

## Inspeção

```slai
sys.ia.inspect(Player)
sys.ia.inspect(Player.damage)
```

Pode expor:

- classes;
- propriedades;
- métodos;
- parâmetros;
- tipos;
- relações entre símbolos.

## Referências

```slai
sys.ia.refs(Player.health)
```

A consulta deve localizar referências semanticamente ligadas ao símbolo correto.

## Renomeação semântica

```slai
sys.ia.rename(Player.health, hp)
```

Não deve ser uma simples substituição textual.

## Diagnósticos

```slai
sys.ia.errors()
```

Possíveis dados estruturados:

```text
arquivo
símbolo
tipo esperado
tipo recebido
origem do valor
relações afetadas
```

## Dependências

```slai
sys.ia.dependencies(Player)
```

## Edição estrutural

```slai
sys.ia.edit(Player.damage):
    .param armor, int
```

## Preview e aplicação

```slai
change = sys.ia.rename(Player.health, hp)

sys.ia.preview(change)
sys.ia.apply(change)
```

## Operações candidatas

Leitura:

```text
sys.ia.inspect(...)
sys.ia.refs(...)
sys.ia.tree(...)
sys.ia.type(...)
sys.ia.errors(...)
sys.ia.dependencies(...)
```

Modificação:

```text
sys.ia.rename(...)
sys.ia.edit(...)
sys.ia.create(...)
sys.ia.remove(...)
sys.ia.move(...)
```

Controle:

```text
sys.ia.preview(...)
sys.ia.apply(...)
```

Nem todas estão oficialmente fechadas.

## API semântica do compilador

A Slai terá seu próprio frontend, análise semântica, SIR e gerador x86-64.

Essa infraestrutura poderá fornecer a `sys.ia`:

- AST;
- símbolos;
- tipos;
- referências;
- dependências;
- call graph;
- diagnósticos;
- SIR;
- operações de refatoração.

O fato de a Slai gerar machine code diretamente não impede essa interface: `sys.ia` trabalha sobre as representações semânticas existentes **antes** da geração de bytes x86-64.

## Plataforma atual

A Slai é, por enquanto, **Windows x64 only**.

`sys.ia` não precisa neste estágio lidar com diferenças semânticas ou de tooling entre múltiplas plataformas.
