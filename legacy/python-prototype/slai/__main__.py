"""Command-line interface: all runs compile and launch a real Windows executable."""
from __future__ import annotations

import argparse
from dataclasses import asdict
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

from . import __version__
from .frontend import SlaiError
from .native import build_pe
from .semantic import compile_file
from .optimizer import optimize_program
from .ia import preview, apply as apply_change


def main(argv=None):
    # Deterministic encoding even when Windows redirects stdout/stderr to pipes.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(prog="slai", description="Compilador nativo Slai para Windows x64")
    parser.add_argument("--version", action="version", version=f"Slai {__version__}")
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("build", "run", "check", "inspect", "sir"):
        command = sub.add_parser(name)
        command.add_argument("source", type=Path)
        command.add_argument("--json", action="store_true", help="Saída estruturada para agentes")
        if name in ("build", "run", "sir"):
            command.add_argument("--release", action="store_true", help="Ativar constant folding na SIR")
        if name == "build":
            command.add_argument("-o", "--output", type=Path)
    command = sub.add_parser("ia", help="Consultas e mudanças semânticas")
    command.add_argument("source", type=Path)
    command.add_argument("operation", choices=("inspect", "refs", "tree", "type", "dependencies", "errors", "rename", "edit"))
    command.add_argument("target", nargs="?")
    command.add_argument("name", nargs="?")
    command.add_argument("--type", choices=("int", "bool", "string"), default="int")
    command.add_argument("--apply", action="store_true")
    command.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        compiler = compile_file(args.source)
        if args.command == "ia":
            if args.operation in ("rename", "edit"):
                if not args.target or not args.name:
                    parser.error("rename/edit exigem símbolo e novo nome")
                change = (compiler.index.rename(args.target, args.name) if args.operation == "rename"
                          else compiler.index.add_parameter(args.target, args.name, args.type))
                result = apply_change(change) if args.apply else dict(change=change, preview=preview(change))
                print(json.dumps(result, ensure_ascii=False, indent=2) if args.json or args.apply else result["preview"])
            else:
                if args.apply:
                    parser.error("--apply é válido apenas para rename/edit")
                print(json.dumps(compiler.index.query(args.operation, args.target), ensure_ascii=False, indent=2))
            return 0
        if getattr(args, "release", False):
            optimize_program(compiler)
        if args.command == "inspect":
            payload = dict(version=__version__, sources=list(compiler.sources),
                           symbols=[asdict(s) for s in compiler.symbols],
                           records={k: dict(name=v.name, fields=v.fields, methods=list(v.methods))
                                    for k, v in compiler.records.items()},
                           specializations=[f.name for f in compiler.functions], diagnostics=[])
            payload["semantic_index"] = compiler.index.query("inspect")
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        elif args.command == "sir":
            print(json.dumps([asdict(f) for f in compiler.functions], ensure_ascii=False, indent=2))
        elif args.command == "check":
            print(json.dumps(dict(ok=True, diagnostics=[])) if args.json else "Slai: verificação concluída.")
        else:
            if args.command == "run" and (os.name != "nt" or platform.machine().lower() not in ("amd64", "x86_64")):
                raise OSError("slai run exige Windows x64; build pode gerar PE em outros hosts.")
            executable = build_pe(compiler)
            if args.command == "build":
                output = (args.output or args.source.with_suffix(".exe")).resolve()
                if output.suffix.lower() != ".exe":
                    raise OSError("O arquivo de saída deve ter extensão .exe.")
                if str(output) in compiler.sources:
                    raise OSError("A saída não pode sobrescrever código-fonte.")
                # Atomic replacement: failed compilation never truncates existing output.
                with tempfile.NamedTemporaryFile(dir=output.parent, delete=False, suffix=".tmp") as temp:
                    temp.write(executable)
                    temporary = Path(temp.name)
                try:
                    temporary.replace(output)
                finally:
                    temporary.unlink(missing_ok=True)
                print(json.dumps(dict(ok=True, output=str(output), bytes=len(executable))) if args.json
                      else f"Slai: {output} ({len(executable)} bytes)")
            else:
                with tempfile.TemporaryDirectory(prefix="slai-") as folder:
                    output = Path(folder) / "program.exe"
                    output.write_bytes(executable)
                    return subprocess.run([str(output)]).returncode
        return 0
    except SlaiError as exc:
        if args.command == "ia" and args.operation == "errors":
            print(json.dumps([exc.diagnostic()], ensure_ascii=False, indent=2))
            return 1
        print(json.dumps(dict(ok=False, diagnostics=[exc.diagnostic()]), ensure_ascii=False) if args.json else str(exc), file=sys.stderr)
        return 1
    except (OSError, UnicodeError) as exc:
        print(json.dumps(dict(ok=False, error=str(exc)), ensure_ascii=False) if args.json else f"Slai: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
