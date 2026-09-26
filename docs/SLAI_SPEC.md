# Slai — Especificação Inicial

> **Status:** rascunho vivo da linguagem.  
> Este documento separa decisões confirmadas de propostas ainda pendentes.

## 1. Identidade

**Nome oficial:** Slai

A Slai é, por enquanto, uma linguagem pessoal, projetada principalmente para o próprio autor usar.

Objetivos já definidos:

- ser especialmente boa para IA/agentes programadores;
- continuar utilizável por pessoas, de iniciantes a programadores experientes;
- ter sintaxe própria, curta e legível;
- expor recursos específicos para agentes por meio de `sys.ia`;
- ser especialmente adequada para criação de aplicações;
- priorizar velocidade, startup rápido e executáveis nativos.

## 2. Plataforma e compilação — decisão oficial

Por enquanto, a Slai suporta **somente Windows x64**.

Não é requisito atual suportar:

- Linux;
- macOS;
- ARM/ARM64;
- WebAssembly;
- outras ABIs ou formatos de executável.

A Slai será uma linguagem **AOT nativa** e deverá gerar **machine code x86-64 diretamente**.

Pipeline oficial de alto nível:

```text
Slai Source
    ↓
Lexer
    ↓
Parser
    ↓
AST
    ↓
Análise semântica
    ↓
SIR
    ↓
Otimizador Slai
    ↓
Gerador x86-64 da Slai
    ↓
Machine Code
    ↓
Gerador PE da Slai
    ↓
.exe Windows x64
```

### Regra arquitetural

A Slai **não utilizará LLVM em nenhum momento**.

Também não utilizará como backend intermediário:

- Cranelift;
- C;
- C++;
- JavaScript;
- bytecode de VM;
- JIT;
- outro compilador para produzir o código final.

A SIR é apenas uma representação intermediária **interna da própria Slai** e não contradiz a geração direta de machine code.

A intenção é que:

```bash
slai build app.slai
```

produza diretamente:

```text
app.exe
```

sem depender de um backend externo.

## 3. Comentários

Uma linha:

```slai
? comentário de uma linha
```

Várias linhas:

```slai
/?
Comentário de
várias
linhas
?/
```

## 4. Variáveis e tipos

Inferência:

```slai
idade = 14
```

Tipo explícito:

```slai
nome, string = "Alex"
```

Portanto, o modelo atual é híbrido: tipos podem ser inferidos ou explicitados.

Outro exemplo fornecido:

```slai
vida = 100
.nome = "Alex"
```

**Pendente:** significado formal do prefixo `.`.

Também permanecem pendentes:

- mutabilidade padrão;
- mudança de tipo;
- constantes;
- tipos primitivos;
- conversões entre tipos.

## 5. Funções

Uma linha:

```slai
func somar($a, $b) => a + b
```

Bloco:

```slai
func somar(a, b):
    a + b
```

A seta `=>` é reservada para formas de uma linha.

Pendente:

- significado exato de `$` em parâmetros;
- retorno implícito;
- tipagem de parâmetros e retorno;
- overload;
- lambdas;
- closures.

## 6. Condicionais

```slai
if:
    vida <= 0 => morrer()
    el => continuar()
```

- `if:` inicia o bloco;
- `=>` associa condição e ação de uma linha;
- `el` representa o caso alternativo.

## 7. Laços

```slai
for i=0; <10:
    sys.log(i)
```

Pendente:

- incremento;
- foreach;
- while;
- break/continue;
- ranges.

## 8. Sistema nativo `sys`

`sys` agrupa funcionalidades nativas/sistema.

Exemplo já definido:

```slai
sys.log(i)
```

`sys.ia` é reservado para funcionalidades destinadas especificamente a IA e agentes.

## 9. Listas

```slai
jogadores = [$Ana, $Lucas, $Pedro]
jogadores.push("João")
sys.log(jogadores$0)
```

Atualmente:

- listas usam `[ ... ]`;
- `.push(...)` adiciona;
- `lista$indice` foi proposto para indexação.

O significado geral de `$` ainda precisa ser formalizado.

## 10. Objetos

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

## 11. Classes

```slai
cl Player:
    .args($name):
        this.name = name
        this.health = 100

    .damage(amount) => this.health -= amount
```

Atualmente:

- `cl` declara classe;
- `.args(...)` é a forma proposta para inicialização;
- `this` referencia a instância;
- métodos podem começar por `.`;
- métodos de uma linha podem usar `=>`.

## 12. Imports

A intenção é importar o arquivo inteiro:

```slai
import $player.<extensão>
```

A extensão oficial ainda não foi definida.

## 13. Código assíncrono

`await` permanece explícito:

```slai
func carregar():
    .resposta = await fetch(url)
    .dados = await resposta.json()
    return dados
```

Ainda não foi definida uma palavra-chave `async`.

## 14. Erros — proposta não confirmada

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

Esse sistema continua em proposta.

## 15. IA e agentes — `sys.ia`

A Slai pretende permitir que agentes compreendam e modifiquem projetos semanticamente.

Exemplos conceituais:

```slai
sys.ia.inspect(Player)
sys.ia.refs(Player.health)
sys.ia.rename(Player.health, hp)
sys.ia.errors()
sys.ia.dependencies(Player)
```

Edição estrutural:

```slai
sys.ia.edit(Player.damage):
    .param armor, int
```

Preview/aplicação:

```slai
change = sys.ia.rename(Player.health, hp)

sys.ia.preview(change)
sys.ia.apply(change)
```

Veja `AI_INTERFACE.md`.

## 16. Integração entre compilador e IA

A base semântica do compilador poderá ser compartilhada com `sys.ia`:

```text
AST
 ↓
Semantic AST
 ↓
símbolos / tipos / referências
 ↓
SIR
```

Isso permite que agentes trabalhem com símbolos e relações reais do programa, em vez de apenas editar texto.

## 17. Questões em aberto prioritárias

1. significado formal de `$`;
2. significado formal de `.`;
3. mutabilidade e constantes;
4. tipos primitivos;
5. retorno implícito;
6. condicionais em múltiplas linhas;
7. semântica de `for`;
8. sistema de erros;
9. extensão oficial;
10. módulos/pacotes;
11. modelo de memória;
12. generics;
13. interfaces/traits;
14. concorrência;
15. `sys.ia`;
16. API semântica;
17. CLI/tooling;
18. organização do runtime;
19. estratégia de register allocation;
20. geração PE e integração com APIs/DLLs do Windows.

## 18. Estado do documento

Itens marcados como pendentes ou propostas não devem ser tratados como decisões definitivas.
