# Slai — AI Interface

> Design inicial de `sys.ia`.
> Este documento descreve a direção atual. Operações individuais ainda poderão mudar.

## Objetivo

`sys.ia` é o espaço reservado para recursos projetados especificamente para IA e agentes programadores.

A meta é permitir que um agente trabalhe com o projeto de maneira **semântica**, não apenas como texto bruto.

Em vez de pensar apenas em:

```text
linha 42, coluna 7
```

um agente poderá trabalhar com elementos como:

```text
Class(Player)
Property(health)
Method(damage)
Parameter(amount)
```

## Princípio

> Funcionalidades especificamente destinadas a agentes de IA ficam sob `sys.ia`.

A sintaxe normal da Slai continua sendo voltada para humanos.

## Inspeção

```slai
sys.ia.inspect(Player)
sys.ia.inspect(Player.damage)
```

Objetivo:

- obter estrutura de classes;
- propriedades;
- métodos;
- parâmetros;
- tipos;
- relações entre símbolos.

## Referências

```slai
sys.ia.refs(Player.health)
```

Objetivo: localizar todas as referências semanticamente ligadas ao símbolo correto.

Isso deve permitir distinguir:

```text
Player.health
player.health
this.health
```

de itens diferentes como:

```text
Enemy.health
healthPotion
"health"
```

## Renomeação semântica

```slai
sys.ia.rename(Player.health, hp)
```

A renomeação deve operar sobre o símbolo e suas referências corretas, não por simples substituição textual.

## Diagnósticos

```slai
sys.ia.errors()
```

A intenção é que diagnósticos possam ser retornados em formato estruturado para facilitar compreensão e correção automática.

Possíveis dados:

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

Objetivo: descobrir relações entre símbolos, módulos e arquivos.

## Edição estrutural

Forma conceitual:

```slai
sys.ia.edit(Player.damage):
    .param armor, int
```

A intenção é permitir alterações sobre elementos da estrutura do programa.

## Preview e aplicação

Alterações de agentes não devem necessariamente ser aplicadas de imediato.

Forma proposta:

```slai
change = sys.ia.rename(Player.health, hp)

sys.ia.preview(change)
sys.ia.apply(change)
```

### `preview`

Mostra o que será alterado.

### `apply`

Aplica a mudança preparada.

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

## API semântica

Uma direção importante da Slai é expor uma API semântica oficial do compilador.

Ela poderá disponibilizar:

- AST;
- símbolos;
- tipos;
- referências;
- dependências;
- diagnósticos;
- relações de chamada;
- operações de refatoração.

Isso permitiria que IDEs e agentes compartilhassem a mesma base semântica.

## Exemplo conceitual

Código:

```slai
cl Player:
    .args($name):
        this.name = name
        this.health = 100

    .damage(amount) => this.health -= amount
```

Um agente poderia consultar:

```slai
sys.ia.inspect(Player)
```

e obter uma estrutura equivalente a:

```text
Player
type: class

properties:
    name
    health

methods:
    args(name)
    damage(amount)
```

Essa interface é uma das características centrais planejadas para a Slai.
