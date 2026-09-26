# Slai — Performance e Compilação Nativa

> Direção técnica oficial para compilação e performance.

## 1. Decisão central

A Slai será uma linguagem **AOT nativa para Windows x64**.

Ela gerará **x86-64 machine code diretamente**.

Não haverá LLVM em nenhuma versão planejada da arquitetura atual.

Também não haverá como backend:

- Cranelift;
- GCC;
- Clang;
- MSVC como compilador de saída;
- C/C++;
- JavaScript;
- JVM;
- bytecode executável;
- JIT.

A toolchain Slai será responsável pela transformação do programa até o executável final.

## 2. Plataforma

Alvo único atual:

```text
OS: Windows
Architecture: x86-64
Executable format: PE32+
ABI: Windows x64
```

Linux, macOS, ARM64 e outros alvos ficam fora do escopo atual.

Essa limitação é intencional: permite concentrar engenharia e otimização em um único ambiente.

## 3. Pipeline

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
    ↓
Otimizador Slai
    ↓
Instruction Selection
    ↓
Register Allocation
    ↓
x86-64 Encoding
    ↓
Machine Code
    ↓
PE Builder
    ↓
.exe
```

## 4. SIR

A **SIR — Slai Intermediate Representation** é uma representação interna criada pela própria Slai.

Ela não é uma VM, bytecode de execução ou backend externo.

Exemplo conceitual:

```slai
func soma(a, b) => a + b
```

pode virar internamente:

```text
func soma:
    t0 = add a, b
    ret t0
```

Depois o compilador Slai converte a SIR diretamente para instruções x86-64.

## 5. Gerador x86-64

O backend da Slai deverá controlar diretamente:

- instruction selection;
- encoding de opcodes;
- ModR/M e SIB quando aplicável;
- registradores;
- stack frames;
- alinhamento de stack;
- parâmetros;
- valores de retorno;
- chamadas;
- branches/jumps;
- constantes;
- endereçamento;
- relocations necessárias.

## 6. Windows x64 ABI

A primeira implementação deverá seguir a calling convention do Windows x64.

O compilador terá de gerenciar corretamente aspectos como:

- registradores de argumentos;
- registrador de retorno;
- shadow space;
- alinhamento da stack;
- caller-saved/callee-saved registers.

A ABI é parte fundamental para interoperar com APIs e DLLs do Windows.

## 7. Geração PE direta

A intenção é a Slai produzir o `.exe` diretamente, não apenas um arquivo intermediário para outro linker finalizar.

O PE Builder deverá lidar com estruturas necessárias como:

- DOS header/stub mínimo;
- PE signature;
- COFF File Header;
- PE32+ Optional Header;
- Section Table;
- `.text`;
- `.rdata`;
- `.data` quando necessário;
- `.idata` quando necessário;
- entry point;
- RVA/file offsets;
- section/file alignment;
- import directory.

Pipeline desejado:

```text
machine code + dados + imports
              ↓
        Slai PE Builder
              ↓
            app.exe
```

## 8. APIs do Windows

Gerar machine code diretamente não significa evitar o sistema operacional.

Aplicações poderão importar DLLs e APIs do Windows, por exemplo, quando necessário.

A Slai deverá ser capaz de construir sua Import Table e gerar chamadas compatíveis com a ABI.

Isso é diferente de depender de outro compilador.

## 9. Runtime pequeno

A Slai pode possuir um runtime próprio, mas ele deve ser pequeno e justificar cada responsabilidade.

Possíveis responsabilidades:

- strings;
- memória;
- `sys.log`;
- async futuramente;
- suporte básico de linguagem.

Objetivo:

```text
abrir app
 ↓
processo inicia
 ↓
app executa rapidamente
```

## 10. Modelo de memória — ainda em projeto

O modelo definitivo ainda não foi decidido.

Possibilidades estudadas:

- ownership;
- reference counting;
- gerenciamento automático híbrido;
- stack allocation agressiva;
- heap apenas quando necessário.

Direção considerada interessante:

```slai
player = Player()
```

e o compilador decide, quando possível:

```text
stack
move
borrow
unique heap
shared
```

A meta é esconder complexidade desnecessária sem sacrificar performance.

## 11. Escape analysis

Se um valor não escapar de um escopo, o compilador poderá mantê-lo na stack ou até eliminá-lo.

Exemplo:

```slai
func criar():
    ponto = Point(10, 20)
    return ponto.x
```

O compilador poderá evitar heap allocation para `ponto`.

## 12. Zero-cost abstractions

Abstrações de alto nível devem, quando possível, desaparecer durante compilação.

Exemplo conceitual:

```slai
players.filter(...).map(...)
```

pode ser fusionado em um único loop, evitando coleções intermediárias.

## 13. Especialização

Generics poderão ser especializados por tipo concreto.

```text
List<int>
List<float>
```

podem produzir representações específicas, evitando boxing e dispatch desnecessário.

## 14. Inlining e constantes

Exemplo:

```slai
func double(x) => x * 2
a = double(10)
```

pode ser reduzido durante otimização.

Também:

```slai
x = 10 * 20 + 4
```

pode virar:

```text
x = 204
```

antes da geração final.

## 15. Otimizações planejadas

O otimizador da própria Slai poderá incluir:

- constant folding;
- constant propagation;
- dead code elimination;
- inlining;
- escape analysis;
- stack allocation;
- common subexpression elimination;
- loop optimizations;
- devirtualization;
- specialization;
- vectorization;
- whole-program optimization.

## 16. Desenvolvimento e release

Desenvolvimento:

```bash
slai build
```

Prioriza compilação rápida e diagnósticos.

Execução conveniente:

```bash
slai run
```

significa:

```text
compilar nativamente
 ↓
gerar executável
 ↓
executar
```

Não significa interpretar.

Release:

```bash
slai build --release
```

pode ativar otimizações mais custosas.

## 17. Relação com `sys.ia`

AST, símbolos, tipos e SIR poderão alimentar `sys.ia`.

```text
Source
 ↓
AST
 ↓
Semantic Analysis
 ↓
SIR
 ├────────→ sys.ia/tooling
 ↓
Optimizer
 ↓
x86-64
```

A interface de IA trabalha com a estrutura semântica do programa antes que ela seja reduzida a machine code.

## 18. Consequência da escolha

Ao rejeitar LLVM e backends externos, a Slai assume responsabilidade direta por áreas difíceis de compiladores, incluindo:

- code generation;
- register allocation;
- ABI;
- executable generation;
- otimizações;
- debug information futuramente.

Essa complexidade é uma escolha consciente.

A vantagem é controle total do pipeline:

```text
Slai Source
   ↓
tecnologia da própria Slai
   ↓
x86-64 machine code
   ↓
Windows .exe
```

Essa é a direção oficial atual.
