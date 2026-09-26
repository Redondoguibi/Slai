# Slai — Sintaxe

> Referência rápida da sintaxe atualmente definida.
> Itens marcados como **pendentes** ainda não são parte definitiva da linguagem.

## Comentários

### Uma linha

```slai
? comentário de uma linha
```

### Várias linhas

```slai
/?
Comentário de
várias
linhas
?/
```

## Variáveis

Inferência de tipo:

```slai
idade = 14
```

Tipo explícito:

```slai
nome, string = "Alex"
```

Outro formato já proposto:

```slai
.nome = "Alex"
```

**Pendente:** definir formalmente o significado do prefixo `.`.

## Funções

Função de uma linha:

```slai
func somar($a, $b) => a + b
```

Função em bloco:

```slai
func somar(a, b):
    a + b
```

A seta `=>` é usada para funções de uma linha.

**Pendente:** significado de `$` nos parâmetros e regras de retorno implícito.

## Condicionais

```slai
if:
    vida <= 0 => morrer()
    el => continuar()
```

- `if:` inicia a condição.
- `=>` permite uma ação de uma linha.
- `el` representa o caso alternativo.

## Laços

```slai
for i=0; <10:
    sys.log(i)
```

**Pendente:** definir incremento, ranges, `while`, `break` e `continue`.

## Listas

```slai
jogadores = [$Ana, $Lucas, $Pedro]
jogadores.push("João")

sys.log(jogadores$0)
```

**Pendente:** definir formalmente `$` em valores e indexação.

## Objetos

Forma inline:

```slai
jogador => nome: "Alex"; vida: 100; nivel: 5
```

Forma em bloco:

```slai
obj jogador:
    nome: "Alex"
    vida: 100
    nivel: 5
```

Acesso:

```slai
sys.log(jogador.nome)
sys.log(jogador.vida)
```

## Classes

```slai
cl Player:
    .args($name):
        this.name = name
        this.health = 100

    .damage(amount) => this.health -= amount
```

Elementos atuais:

- `cl` declara uma classe.
- `.args(...)` é a forma proposta para inicialização.
- `this` referencia a instância atual.
- Métodos podem começar com `.`.
- Métodos de uma linha podem usar `=>`.

## Imports

A intenção atual é importar o arquivo inteiro:

```slai
import $player.<extensão>
```

**Pendente:** extensão oficial da Slai e regras do sistema de módulos.

## Assíncrono

```slai
func carregar():
    .resposta = await fetch(url)
    .dados = await resposta.json()
    return dados
```

`await` permanece explícito.

**Pendente:** definir se funções assíncronas precisam de marcação própria.

## Logs e sistema

```slai
sys.log("Olá")
```

`sys` é o namespace reservado para funcionalidades nativas/sistema.

## Erros — proposta

O sistema abaixo ainda não foi confirmado.

Tratamento local:

```slai
usuario = carregarUsuario(10) ?! erro:
    sys.log(erro)
```

Propagação:

```slai
usuario = carregarUsuario(10)?
```

Erro manual:

```slai
error UsuarioNaoEncontrado
```

## IA

Recursos específicos para agentes ficam sob:

```slai
sys.ia
```

Exemplos conceituais:

```slai
sys.ia.inspect(Player)
sys.ia.refs(Player.health)
sys.ia.rename(Player.health, hp)
sys.ia.errors()
```

Veja `AI_INTERFACE.md` para detalhes.
