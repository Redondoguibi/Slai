# Slai

- O compilador e o runtime são escritos em Assembly x86-64. Não substitua
  componentes por Python, C/C++, LLVM, uma VM ou outro backend.
- FASM monta a toolchain em desenvolvimento. `slai build` emite os bytes x64
  e o PE diretamente, sem executar FASM ou outro compilador.
- O código ativo fica em `asm/`; `legacy/python-prototype/` é somente referência
  histórica e não participa de build, testes atuais ou execução.
- Preserve os exemplos originais e as decisões em aberto da especificação.
  Registre convenções provisórias e limites em `docs/IMPLEMENTATION.md`.
- Validação no Windows x64: `scripts/build.ps1`, `scripts/test.ps1`,
  `scripts/test-ia.ps1` e `scripts/test-pe.ps1`. Os testes usam PowerShell 7.
- Refatorações devem usar vínculos semânticos, preservar comentários/strings,
  recusar referências ambíguas e validar snapshots antes de escrever fontes.
