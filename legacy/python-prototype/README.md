# Protótipo histórico

Esta pasta preserva a implementação anterior em Python, seus testes e os textos
anteriores à migração. Ela **não faz parte da toolchain ativa** e não é chamada
por `slai.cmd`, pelo compilador Assembly ou pelos programas gerados.

A escolha de Python não correspondia ao requisito do autor. A implementação
ativa foi reescrita em `../../asm/`, com testes nativos em `../../scripts/`.
Este material serve para comparação e recuperação de trabalho anterior.
Os testes históricos conservam suas referências originais a `examples/`;
não são a suíte de validação da versão Assembly.
