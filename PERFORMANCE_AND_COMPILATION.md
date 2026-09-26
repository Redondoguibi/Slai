# Slai — Performance e Compilação Nativa

> Documento de direção técnica para performance, geração de código nativo e arquitetura de compilação da Slai.

## Objetivo

A Slai está sendo projetada com foco em aplicações rápidas e executáveis nativos.

A meta é permitir algo como:

```bash
slai build app.slai
```

gerando diretamente um executável nativo do sistema operacional, por exemplo:

```text
app.exe
```

no Windows.

A Slai não deve depender obrigatoriamente de:

- JVM;
- Node.js;
- Python;
- uma VM pesada;
- um runtime grande apenas para iniciar aplicações comuns.

A intenção é buscar:

- startup rápido;
- baixo overhead;
- performance previsível;
- binários nativos;
- otimizações agressivas em modo release.

---

## 1. Gerar machine code diretamente não garante mais performance

Uma distinção importante:

```text
Slai → machine code próprio
```

não é automaticamente mais rápido do que:

```text
Slai → LLVM IR → machine code
```

A velocidade do programa final depende principalmente de:

- qualidade das otimizações;
- modelo de memória;
- alocações;
- calling conventions;
- inlining;
- escape analysis;
- especialização;
- vectorization;
- qualidade do backend;
- eficiência do runtime.

Portanto, um backend próprio mal otimizado pode gerar programas mais lentos do que LLVM.

---

## 2. Arquitetura proposta do compilador

A arquitetura desejada é:

```text
Código Slai
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
Slai Intermediate Representation
    ↓
Otimizador Slai
    ↓
Backend
    ↓
Machine Code
```

A Slai deverá possuir uma representação intermediária própria chamada provisoriamente de:

```text
SIR
Slai Intermediate Representation
```

Exemplo:

```slai
func soma(a, b) => a + b
```

poderia se transformar conceitualmente em:

```text
func soma(int a, int b):
    %0 = add a, b
    return %0
```

A SIR deve separar o frontend da Slai do backend de código nativo.

Isso permite trocar ou adicionar backends sem reconstruir a linguagem inteira.

---

## 3. Estratégia inicial de backend

### Primeira etapa

```text
SIR
 ↓
LLVM
 ↓
x86-64 / ARM64
```

LLVM cuidaria inicialmente de tarefas complexas como:

- seleção de instruções;
- register allocation;
- otimizações de baixo nível;
- geração de object files;
- suporte a múltiplas arquiteturas;
- suporte a múltiplos sistemas operacionais.

Isso permite concentrar o desenvolvimento inicial no que realmente define a Slai:

- sintaxe;
- tipos;
- semântica;
- memória;
- classes;
- módulos;
- erros;
- `sys`;
- `sys.ia`;
- otimizações próprias.

---

## 4. Backend nativo próprio no futuro

A Slai poderá futuramente ter um backend próprio:

```text
SIR
 ↓
Slai Optimizer
 ↓
Slai x86-64 Backend
```

e depois:

```text
SIR
 ↓
Slai ARM64 Backend
```

Esse backend poderia gerar machine code diretamente.

Entretanto, não deve ser prioridade inicial.

Um backend próprio exige implementar ou controlar áreas como:

- instruction selection;
- register allocation;
- stack layout;
- stack frames;
- calling conventions;
- ABI;
- relocations;
- object files;
- debug symbols;
- x86-64;
- ARM64;
- Windows x64 ABI;
- Linux ABI;
- macOS ABI;
- formatos como PE/COFF, ELF e Mach-O.

Isso é uma quantidade enorme de engenharia e não deve atrasar o desenvolvimento da linguagem em si.

---

## 5. Estratégia de evolução

Uma direção possível:

```text
Slai v0.x
SIR → LLVM

Slai v1.x+
SIR → LLVM
    ↘ Slai Native Backend experimental
```

O backend próprio pode começar experimentalmente.

Quando ele atingir qualidade suficiente para determinados casos, poderá ser utilizado de forma opcional.

A escolha do backend padrão pode ser revisada no futuro.

---

# Performance

## 6. AOT nativo

A Slai deve priorizar compilação Ahead-of-Time.

```text
.slai
  ↓
Slai Compiler
  ↓
native executable
```

Objetivo:

- sem VM obrigatória;
- startup rápido;
- executáveis nativos;
- boa integração com aplicações desktop.

---

## 7. Runtime pequeno

Aplicações Slai devem evitar depender de um runtime excessivamente grande.

Objetivo:

```text
usuário abre aplicação
        ↓
processo inicia
        ↓
janela aparece
```

com o mínimo de infraestrutura intermediária possível.

A existência de um runtime Slai não está descartada, mas ele deve ser pequeno e justificar seu custo.

---

## 8. Zero-cost abstractions

A Slai deve permitir abstrações de alto nível sem necessariamente pagar por elas em runtime.

Exemplo conceitual:

```slai
players.filter(...).map(...)
```

quando possível, poderia ser convertido em algo equivalente a:

```text
for player:
    if condição:
        resultado.push(transform(player))
```

evitando:

- coleções intermediárias;
- alocações desnecessárias;
- múltiplas iterações.

A filosofia desejada é:

> abstrações devem ser eliminadas pelo compilador quando ele puder provar que isso é seguro e equivalente.

---

## 9. Escape analysis e alocação inteligente

Exemplo:

```slai
func criar():
    ponto = Point(10, 20)
    return ponto.x
```

Se o compilador provar que `ponto` não escapa da função, ele poderá preferir:

```text
stack allocation
```

em vez de:

```text
heap allocation
```

Objetivo:

- reduzir heap allocations;
- reduzir gerenciamento de memória;
- melhorar cache locality;
- diminuir trabalho do runtime.

---

## 10. Especialização de generics

Exemplo:

```text
List<int>
List<float>
```

A Slai poderá gerar versões especializadas para tipos concretos:

```text
List_int
List_float
```

Isso evita depender obrigatoriamente de estruturas genéricas baseadas em boxing ou dispatch dinâmico.

Objetivos:

- menos overhead;
- melhor uso de memória;
- mais oportunidades de otimização;
- melhor vectorization.

---

## 11. Inlining

Funções pequenas poderão ser inlined quando apropriado.

Exemplo:

```slai
func double(x) => x * 2

a = double(10)
```

poderá acabar equivalente a:

```text
a = 20
```

após otimizações.

O compilador deverá decidir quando inline melhora performance sem aumentar excessivamente o binário.

---

## 12. Constant folding e constant propagation

Expressões conhecidas em compile time deverão ser resolvidas durante a compilação.

Exemplo:

```slai
x = 10 * 20 + 4
```

poderá ser reduzido para:

```text
x = 204
```

antes da execução.

Outras otimizações futuras podem incluir:

- dead code elimination;
- common subexpression elimination;
- loop optimization;
- devirtualization;
- vectorization;
- whole-program optimization;
- link-time optimization.

---

# Memória

## 13. Modelo de memória ainda não fechado

Para buscar performance próxima de linguagens de sistemas sem prejudicar a ergonomia, a Slai não deve escolher um modelo de memória apenas pela facilidade de implementação.

Opções consideradas:

### Garbage Collector

Vantagens:

- ergonomia simples;
- gerenciamento automático.

Desvantagens possíveis:

- runtime maior;
- pausas;
- menor previsibilidade.

### Reference Counting / ARC

Ideia:

```text
objeto:
references = 3
```

quando chega a zero:

```text
free
```

Vantagens:

- liberação previsível.

Custos:

- incrementos/decrementos;
- ciclos precisam de tratamento.

### Ownership

Vantagens:

- potencial de alta performance;
- gerenciamento estático;
- segurança.

Desvantagem:

- pode aumentar muito a complexidade para o programador.

---

## 14. Direção preferida: modelo automático e híbrido

A direção atualmente considerada mais interessante é um modelo automático.

O usuário escreveria:

```slai
player = Player()
```

e o compilador tentaria decidir internamente entre estratégias como:

```text
stack
move
borrow
unique heap
shared
```

Quando o compilador provar que não é necessário gerenciamento dinâmico:

```text
zero runtime memory management
```

Quando um objeto realmente precisar ser compartilhado, algum mecanismo de ownership compartilhado ou reference counting poderá ser utilizado.

A meta seria aproximar:

```text
ergonomia de linguagem de aplicações
+
performance de linguagem de sistemas
```

Esse modelo ainda precisa ser projetado e validado antes de virar uma decisão oficial.

---

# Modos de compilação

## 15. Desenvolvimento

```bash
slai run
```

Prioridades:

- compilação rápida;
- feedback imediato;
- diagnósticos;
- otimizações suficientes para desenvolvimento.

Não é necessário executar todas as otimizações pesadas a cada alteração.

---

## 16. Release

```bash
slai build --release
```

Prioridades:

- performance;
- redução de overhead;
- otimizações agressivas.

Possíveis etapas:

```text
inline
escape analysis
dead code elimination
constant propagation
devirtualization
vectorization
whole-program optimization
link-time optimization
```

O modo release pode aceitar tempos de compilação maiores em troca de executáveis melhores.

---

# Relação com sys.ia

## 17. SIR e análise semântica também beneficiam agentes

A arquitetura:

```text
Source
 ↓
AST
 ↓
Semantic AST
 ↓
SIR
```

não serve apenas para geração de código.

Ela também pode fornecer a base semântica usada por:

```slai
sys.ia
```

Agentes poderão consultar informações derivadas de:

- AST;
- tabela de símbolos;
- tipos;
- referências;
- call graph;
- dependências;
- diagnósticos;
- SIR.

Isso permite que `sys.ia` compreenda o programa muito melhor do que uma ferramenta baseada apenas em texto.

---

# Direção atual

A meta técnica da Slai é:

> Compilar aplicações para executáveis nativos rápidos, mantendo uma linguagem confortável para desenvolvimento de apps.

A estratégia atual recomendada é:

```text
Slai Source
    ↓
AST
    ↓
Semantic Analysis
    ↓
SIR
    ↓
Slai Optimizer
    ↓
LLVM inicialmente
    ↓
Machine Code
```

com a possibilidade futura de:

```text
SIR
 ↓
Slai Native Backend
 ↓
Machine Code
```

O backend próprio é uma ambição válida, mas não deve impedir que a Slai se torne utilizável antes disso.
