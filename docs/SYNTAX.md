# Slai — Sintaxe

> Referência rápida da sintaxe atualmente definida.  
> Itens marcados como **pendentes** ainda não são definitivos.

## Comentários

```slai
? comentário de uma linha
```

```slai
/?
comentário
de várias linhas
?/
```

## Variáveis

```slai
idade = 14
nome, string = "Alex"
```

Outro formato já proposto:

```slai
.nome = "Alex"
```

**Pendente:** significado formal do prefixo `.`.

## Funções

Uma linha:

```slai
func somar($a, $b) => a + b
```

Bloco:

```slai
func somar(a, b):
    a + b
```

`=>` é usado para funções de uma linha.

## Condicionais

```slai
if:
    vida <= 0 => morrer()
    el => continuar()
```

## Laços

```slai
for i=0; <10:
    sys.log(i)
```

## Listas

```slai
jogadores = [$Ana, $Lucas, $Pedro]
jogadores.push("João")

sys.log(jogadores$0)
```

## Objetos

Inline:

```slai
jogador => nome: "Alex"; vida: 100; nivel: 5
```

Bloco:

```slai
obj jogador:
    nome: "Alex"
    vida: 100
    nivel: 5
```

Acesso:

```slai
sys.log(jogador.nome)
```

## Classes

```slai
cl Player:
    .args($name):
        this.name = name
        this.health = 100

    .damage(amount) => this.health -= amount
```

## Imports

```slai
import $player.<extensão>
```

O arquivo inteiro é importado.

**Pendente:** extensão oficial e regras de módulos.

## Assíncrono

```slai
func carregar():
    .resposta = await fetch(url)
    .dados = await resposta.json()
    return dados
```

`await` permanece explícito.

## Sistema

```slai
sys.log("Olá")
```

`sys` é o namespace reservado para funcionalidades nativas/sistema.

## Erros — proposta

```slai
usuario = carregarUsuario(10) ?! erro:
    sys.log(erro)
```

```slai
usuario = carregarUsuario(10)?
```

```slai
error UsuarioNaoEncontrado
```

## IA

```slai
sys.ia.inspect(Player)
sys.ia.refs(Player.health)
sys.ia.rename(Player.health, hp)
sys.ia.errors()
```

Veja `AI_INTERFACE.md`.

## Plataforma

A sintaxe é compilada AOT diretamente para **machine code x86-64** e empacotada como executável **PE para Windows x64**.

A Slai não usa LLVM, Cranelift, C/C++, bytecode ou JIT como backend.
