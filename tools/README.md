# Montador da toolchain

`scripts/bootstrap.ps1` obtém FASM 1.73.35 do site oficial, verifica o SHA-256
fixado no script e extrai a distribuição em `tools/fasm/`.

O FASM é usado por `scripts/build.ps1` para montar o compilador e os blobs do
runtime/imports. O executável `bin/slai.exe` e os programas que ele produz não
executam nem precisam distribuir FASM. Não há dependência de Python ou LLVM.

A licença do FASM está em `tools/fasm/LICENSE.TXT` após o bootstrap. A distribuição
baixada e os artefatos gerados ficam fora do controle de versão.

Download oficial: <https://flatassembler.net/download.php>.
