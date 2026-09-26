# Slai — Especificação Inicial

> **Status:** rascunho vivo da linguagem.  
> Este documento registra apenas o que foi definido na conversa até agora e separa decisões confirmadas de propostas ainda pendentes.

## 1. Identidade

**Nome oficial:** Slai

A Slai está sendo projetada como uma linguagem de programação com foco especial em **IA e agentes programadores**, sem deixar de ser utilizável por pessoas.

Objetivos já definidos:

- Ser especialmente boa para IA/agentes programadores.
- Poder ser usada desde por iniciantes até por programadores experientes.
- Ter uma sintaxe própria, curta e legível.
- Expor recursos específicos para agentes por meio de `sys.ia`, sem poluir a sintaxe principal da linguagem.

Ainda não foi definido a qual família de linguagens a Slai deve se parecer mais.

---

## 2. Comentários

### Comentário de uma linha

```slai
? comentário de uma linha
```

### Comentário de várias linhas

```slai
/?
Comentário de
várias
linhas
?/
```

---

## 3. Variáveis e tipos

A Slai deve permitir inferência de tipo:

```slai
idade = 14
```

Também deve permitir declaração explícita de tipo:

```slai
nome, string = "Alex"
```

Portanto, o modelo escolhido até agora é **híbrido**: o tipo pode ser inferido ou explicitado.

Outro exemplo fornecido:

```slai
vida = 100
.nome = "Alex"
```

### Pendente

O significado exato do prefixo `.` em declarações como:

```slai
.nome = "Alex"
```

ainda precisa ser definido.

Também ainda não foi decidido:

- se variáveis são mutáveis por padrão;
- se uma variável pode mudar de tipo após criada;
- como funcionam constantes;
- quais são os tipos primitivos oficiais;
- regras de conversão entre tipos.

---

## 4. Funções

Palavra-chave escolhida:

```slai
func
```

### Função de uma linha

A seta `=>` é usada especificamente para funções de uma linha:

```slai
func somar($a, $b) => a + b
```

### Função em bloco

```slai
func somar(a, b):
    a + b
```

A sintaxe indica que blocos usam `:`.

### Pendente

Ainda precisa ser definido:

- o significado exato de `$a` / `$b` em parâmetros;
- se a última expressão de uma função é retornada automaticamente;
- quando `return` é obrigatório;
- sintaxe de tipos de parâmetros e retorno;
- sobrecarga;
- funções anônimas/lambdas;
- closures.

---

## 5. Condicionais

Sintaxe definida até agora:

```slai
if:
    vida <= 0 => morrer()
    el => continuar()
```

Características observadas:

- `if:` inicia o bloco condicional;
- `=>` pode associar uma condição a uma ação de uma linha;
- `el` representa o caso alternativo equivalente ao `else`.

### Pendente

Ainda não foi definido:

- como escrever múltiplas condições (`else if`);
- operadores booleanos oficiais;
- blocos condicionais com várias linhas;
- pattern matching.

---

## 6. Laços

Forma fornecida:

```slai
for i=0; <10:
    sys.log(i)
```

Esta forma representa uma repetição iniciada em `i = 0` enquanto `i < 10`.

### Pendente

Ainda precisa ser definido:

- incremento implícito ou explícito de `i`;
- `for each`;
- `while`;
- `break` e `continue`;
- ranges.

---

## 7. Sistema nativo `sys`

A linguagem utilizará o namespace/módulo `sys` para funcionalidades de sistema e runtime.

Exemplo já usado:

```slai
sys.log(i)
```

`sys.log` é o mecanismo apresentado até agora para saída/log.

Mais namespaces de `sys` ainda poderão ser definidos posteriormente.

---

## 8. Listas

Exemplo definido:

```slai
jogadores = [$Ana, $Lucas, $Pedro]
jogadores.push("João")
sys.log(jogadores$0)
```

Características atuais:

- listas usam `[ ... ]`;
- `.push(...)` adiciona um elemento;
- acesso por índice foi proposto como `lista$indice`.

Exemplo:

```slai
jogadores$0
```

### Pendente

O significado geral de `$` ainda não está fechado. Ele aparece em:

- parâmetros: `$a`;
- valores em listas: `$Ana`;
- indexação: `jogadores$0`;
- imports: `$player`.

Antes de estabilizar a sintaxe, será necessário definir uma regra única ou regras explicitamente separadas para `$`.

---

## 9. Objetos

Duas formas foram propostas.

### Forma inline

```slai
jogador => nome: "Alex"; vida: 100; nivel: 5
sys.log(jogador.nome)
```

### Forma em bloco

```slai
obj jogador:
    nome: "Alex"
    vida: 100
    nivel: 5

sys.log(jogador.vida)
```

O acesso a propriedades usa ponto:

```slai
jogador.nome
jogador.vida
```

### Pendente

Ainda precisa ser definido:

- diferença semântica entre a forma inline e `obj`;
- mutabilidade;
- tipos estruturais;
- métodos em objetos;
- herança/protótipos, se existirem.

---

## 10. Classes

Palavra-chave escolhida:

```slai
cl
```

Exemplo:

```slai
cl Player:
    .args($name):
        this.name = name
        this.health = 100

    .damage(amount) => this.health -= amount
```

Elementos observados:

- `cl Player:` declara uma classe;
- `.args(...)` foi proposto como forma de inicialização/construtor;
- `this` referencia a instância atual;
- métodos podem começar com `.`;
- métodos de uma linha também podem usar `=>`.

### Pendente

Ainda precisa ser definido:

- se `.args` é oficialmente o construtor;
- significado geral do prefixo `.` em membros;
- campos públicos/privados;
- herança;
- interfaces/traits;
- métodos estáticos;
- classes abstratas;
- generics.

---

## 11. Imports

A intenção atual é importar **o arquivo inteiro**.

Forma proposta:

```slai
import $player.<extensão>
```

A extensão oficial dos arquivos Slai ainda **não foi definida**.

Também ainda precisa ser decidido:

- se o nome do arquivo vira automaticamente um namespace;
- imports relativos/absolutos;
- pacotes;
- aliases;
- resolução de módulos.

---

## 12. Código assíncrono

A preferência atual mantém `await` explícito.

Exemplo fornecido:

```slai
func carregar():
    .resposta = await fetch(url)
    .dados = await resposta.json()
    return dados
```

Não foi definida uma palavra-chave `async` até agora.

### Pendente

Ainda precisa ser decidido:

- se funções assíncronas precisam ser marcadas;
- como o compilador determina que uma função é assíncrona;
- Promises/Futures/Tasks;
- concorrência e paralelismo;
- structured concurrency;
- comportamento de erros em operações assíncronas.

---

## 13. Erros — proposta ainda não confirmada

O sistema abaixo foi proposto, mas **ainda não foi confirmado como parte oficial da Slai**.

### Tratar erro localmente com `?!`

```slai
usuario = carregarUsuario(10) ?! erro:
    sys.log(erro)
```

Forma de uma linha:

```slai
usuario = carregarUsuario(10) ?! => sys.log("Erro")
```

### Propagar erro com `?`

```slai
func abrirPerfil(id):
    usuario = carregarUsuario(id)?
    foto = carregarFoto(usuario)?
    return foto
```

A ideia seria: se a operação falhar, o erro é devolvido automaticamente à função chamadora.

### Criar erro manualmente

```slai
error "Usuário não encontrado"
```

ou:

```slai
error UsuarioNaoEncontrado
```

Exemplo:

```slai
func dividir(a, b):
    if:
        b == 0 => error DivisaoPorZero

    return a / b
```

Este sistema ainda precisa ser aprovado, modificado ou substituído.

---

## 14. IA e agentes — `sys.ia`

A Slai terá recursos destinados especificamente a IA/agentes agrupados em:

```slai
sys.ia
```

A intenção é que agentes consigam compreender e modificar projetos **semanticamente**, em vez de depender apenas de edição de texto.

Regra conceitual atual:

> Funcionalidades especificamente destinadas a agentes de IA ficam sob `sys.ia`.

### Inspeção

```slai
sys.ia.inspect(Player)
sys.ia.inspect(Player.damage)
```

Objetivo: obter uma representação estruturada de classes, funções, propriedades, tipos, parâmetros e relações.

### Referências

```slai
sys.ia.refs(Player.health)
```

Objetivo: localizar semanticamente todas as referências ao símbolo correto, evitando substituições textuais acidentais.

### Renomeação semântica

```slai
sys.ia.rename(Player.health, hp)
```

A intenção é alterar apenas referências semanticamente ligadas a `Player.health`.

Por exemplo, poderia atualizar:

```text
this.health
player.health
Player.health
```

sem alterar automaticamente símbolos diferentes como:

```text
Enemy.health
healthPotion
"health"
```

### Diagnósticos

```slai
sys.ia.errors()
```

A intenção é fornecer erros de maneira estruturada, adequada tanto para humanos quanto para agentes.

### Dependências

```slai
sys.ia.dependencies(Player)
```

Objetivo: descobrir dependências e relações do símbolo/projeto.

### Edição estrutural

Forma conceitual proposta:

```slai
sys.ia.edit(Player.damage):
    .param armor, int
```

A ideia é permitir alterações sobre a estrutura semântica do código.

### Preview e aplicação

Foi proposta uma separação entre preparação e aplicação de mudanças:

```slai
change = sys.ia.rename(Player.health, hp)

sys.ia.preview(change)
sys.ia.apply(change)
```

Objetivo: permitir que agentes visualizem uma alteração antes de efetivá-la.

### Possíveis operações de `sys.ia`

Ainda como design inicial:

```text
sys.ia.inspect(...)
sys.ia.refs(...)
sys.ia.tree(...)
sys.ia.type(...)
sys.ia.errors(...)
sys.ia.dependencies(...)

sys.ia.rename(...)
sys.ia.edit(...)
sys.ia.create(...)
sys.ia.remove(...)
sys.ia.move(...)

sys.ia.preview(...)
sys.ia.apply(...)
```

Nem todas essas funções foram formalmente aprovadas individualmente; elas representam a direção atual do módulo.

---

## 15. Princípio de integração com IA

A intenção não é transformar a sintaxe normal da Slai em uma linguagem exclusiva para IA.

O código principal continua sendo escrito para humanos.

Exemplo:

```slai
func attack(enemy):
    damage = 20
    enemy.health -= damage
```

A inteligência específica para ferramentas e agentes fica disponível por `sys.ia`.

Uma direção arquitetural discutida é expor uma **API semântica oficial** da linguagem/compilador, permitindo que ferramentas trabalhem com elementos como:

```text
Class(Player)
Method(damage)
Parameter(amount)
Property(health)
```

em vez de apenas posições de texto como "linha 75, caractere 17".

Essa API poderá ser uma das características centrais da Slai.

---

## 16. Exemplo combinado do estado atual

O código abaixo é apenas uma demonstração da sintaxe atualmente discutida; não representa ainda uma gramática final.

```slai
? Exemplo Slai

/?
Programa de exemplo
com múltiplas linhas
?/

import $player.<extensão>

nome, string = "Alex"
vida = 100

jogadores = [$Ana, $Lucas, $Pedro]
jogadores.push("João")

func somar($a, $b) => a + b

func carregar():
    .resposta = await fetch(url)
    .dados = await resposta.json()
    return dados

obj jogador:
    nome: "Alex"
    vida: 100
    nivel: 5

cl Player:
    .args($name):
        this.name = name
        this.health = 100

    .damage(amount) => this.health -= amount

if:
    vida <= 0 => morrer()
    el => continuar()

for i=0; <10:
    sys.log(i)

sys.log(jogadores$0)
sys.log(jogador.nome)
```

---

## 17. Questões em aberto prioritárias

Antes de congelar a primeira versão da sintaxe, ainda precisamos decidir principalmente:

1. O significado formal de `$`.
2. O significado formal do prefixo `.`.
3. Mutabilidade e constantes.
4. Regras de tipos e mudança de tipo.
5. Tipos primitivos.
6. Retorno implícito vs. `return`.
7. Blocos de várias linhas em `if`.
8. Semântica exata de `for`.
9. Sistema definitivo de erros.
10. Extensão dos arquivos Slai.
11. Sistema de módulos e pacotes.
12. Modelo de memória.
13. Funções/classes genéricas.
14. Interfaces/traits.
15. Concorrência e paralelismo.
16. Modelo formal de `sys.ia`.
17. API semântica/AST pública para agentes.
18. Toolchain e CLI.
19. Compilação/interpretação/JIT.
20. Interoperabilidade com outras linguagens.

---

## 18. Estado do documento

Esta é a **primeira especificação consolidada** da Slai.

Ela deve ser atualizada conforme novas decisões forem tomadas. Itens marcados como **Pendente** ou **proposta ainda não confirmada** não devem ser tratados por agentes como decisões definitivas da linguagem.
