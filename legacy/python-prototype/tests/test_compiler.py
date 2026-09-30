"""Frontend contracts and end-to-end execution of generated PE files."""
import json
import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile
import unittest

from slai.frontend import SlaiError, lex, parse
from slai.native import build_pe
from slai.semantic import Compiler, compile_file, optimize
from slai.optimizer import optimize_program


ROOT = Path(__file__).resolve().parents[1]
WINDOWS_X64 = os.name == "nt" and platform.machine().lower() in ("amd64", "x86_64")


def compile_source(source, release=False):
    compiler = Compiler().analyze(parse(source))
    if release:
        optimize_program(compiler)
    return compiler


class FrontendTests(unittest.TestCase):
    def test_comments_strings_and_locations(self):
        tokens = lex('? one\n/? many\nlines ?/\nnome = "Olá ? /?"\n', "test.slai")
        name = next(t for t in tokens if t.text == "nome")
        self.assertEqual((name.file, name.line, name.column), ("test.slai", 4, 1))
        self.assertEqual(parse('x = "Olá ? /?"')[0].children[1].value, "Olá ? /?")

    def test_precedence(self):
        node = parse("x = 2 + 3 * 4")[0].children[1]
        self.assertEqual(node.value, "+")
        self.assertEqual(node.children[1].value, "*")

    def test_multiline_list(self):
        self.assertEqual(len(parse("x = [\n  1,\n  2,\n]\n")[0].children[1].children), 2)

    def test_invalid_sources(self):
        sources = ["x = (1", "x = [1)", 'x = "unterminated', "/? unfinished",
                   "for i=0; <2:\n\tx = 2", "x = 1.5", "x = missing",
                   'x, int = "a"', 'x = 1\nx = "a"', "return 1", "break",
                   "sys.log()", "x = []", 'x = [1, "a"]', 'sys.log("\\0")',
                   "func f(a, a) => a", "x = 9223372036854775808",
                   "func f() => missing", "func f(a: float) => a", "func f(f) => f",
                   "x = 1?", "x = ²",
                   "x = 1\nx.nope = 2", "sys.ia.unknown()",
                   "func f():\n    return await fetch(url)"]
        for source in sources:
            with self.subTest(source=source), self.assertRaises(SlaiError):
                compile_source(source)

    def test_branch_local_not_definitely_assigned(self):
        with self.assertRaisesRegex(SlaiError, "Nome não definido"):
            compile_source("if:\n    false => x = 1\nsys.log(x)")

    def test_missing_return_path(self):
        with self.assertRaisesRegex(SlaiError, "Nem todos"):
            compile_source("func f(x):\n    if:\n        x > 0 => return x\nsys.log(f(1))")

    def test_import_once_and_cycle(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            (path / "math.slai").write_text("func soma(a, b) => a + b\n", encoding="utf-8")
            (path / "main.slai").write_text("import $math.slai\nimport $math.slai\nsys.log(soma(1, 2))\n", encoding="utf-8")
            self.assertEqual(len(compile_file(path / "main.slai").sources), 2)
            (path / "math.slai").write_text("import $main.slai\n", encoding="utf-8")
            with self.assertRaisesRegex(SlaiError, "Ciclo"):
                compile_file(path / "main.slai")

    def test_pe_structure(self):
        image = build_pe(compile_source('sys.log("Hello")'))
        self.assertEqual(image[:2], b"MZ")
        pe = struct.unpack_from("<I", image, 0x3C)[0]
        self.assertEqual(image[pe:pe + 4], b"PE\0\0")
        self.assertEqual(struct.unpack_from("<H", image, pe + 4)[0], 0x8664)
        self.assertEqual(struct.unpack_from("<H", image, pe + 24)[0], 0x20B)
        self.assertEqual(len(image) % 512, 0)
        self.assertIn(b"KERNEL32.dll\0", image)
        self.assertIn(b".pdata\0", image)
        self.assertEqual(image, build_pe(compile_source('sys.log("Hello")')))

    def test_json_diagnostic(self):
        result = subprocess.run([sys.executable, "-m", "slai", "check", str(ROOT / "examples/conditions.slai"), "--json"],
                                cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
        self.assertEqual(result.returncode, 1)
        data = json.loads(result.stderr)
        self.assertEqual(data["diagnostics"][0]["code"], "ENAME")

    def test_failed_build_preserves_existing_executable(self):
        with tempfile.TemporaryDirectory() as folder:
            source, output = Path(folder) / "bad.slai", Path(folder) / "bad.exe"
            source.write_text("sys.log(undefined)", encoding="utf-8")
            output.write_bytes(b"previous executable")
            result = subprocess.run([sys.executable, "-m", "slai", "build", str(source), "--json"],
                                    cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
            self.assertEqual(result.returncode, 1)
            self.assertFalse(json.loads(result.stderr)["ok"])
            self.assertEqual(output.read_bytes(), b"previous executable")

    def test_inspection_and_sir(self):
        for command in ("inspect", "sir"):
            result = subprocess.run([sys.executable, "-m", "slai", command, "examples/application.slai"],
                                    cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads(result.stdout)
            if command == "inspect":
                player = next(s for s in payload["symbols"] if s["name"] == "Player")
                self.assertEqual(player["kind"], "class")
                self.assertTrue(player["references"])
                self.assertTrue(any(r["fields"].get("health") == "int" for r in payload["records"].values()))
            else:
                self.assertTrue(any(i["op"] == "call" for f in payload for i in f["instructions"]))


@unittest.skipUnless(WINDOWS_X64, "Generated executables require Windows x64")
class NativeTests(unittest.TestCase):
    def execute(self, source, expected, status=0):
        for release in (False, True):
            with self.subTest(release=release), tempfile.TemporaryDirectory() as folder:
                path = Path(folder) / "test.exe"
                path.write_bytes(build_pe(compile_source(source, release)))
                result = subprocess.run([str(path)], capture_output=True, timeout=10)
                self.assertEqual(result.returncode, status, result.stderr)
                self.assertEqual(result.stdout.decode("utf-8").replace("\r\n", "\n"), expected)

    def test_existing_examples(self):
        expected = {"hello": "Hello, Slai!\n", "variables": "Alex\n14\n",
                    "loops": "".join(f"{n}\n" for n in range(10)),
                    "lists": "Ana\n", "objects": "Alex\n100\n"}
        for name, output in expected.items():
            with self.subTest(example=name):
                self.execute((ROOT / "examples" / f"{name}.slai").read_text(encoding="utf-8"), output)

    def test_imports_execute_once_and_native_cli(self):
        with tempfile.TemporaryDirectory(prefix="slai teste ") as folder:
            path = Path(folder)
            (path / "module.slai").write_text('sys.log("Módulo")\nfunc double(x) => x * 2\n', encoding="utf-8")
            (path / "main.slai").write_text('import $module.slai\nimport "module.slai"\nsys.log(double(21))\n', encoding="utf-8")
            build = subprocess.run([str(ROOT / "slai.cmd"), "build", str(path / "main.slai"), "--release", "--json"],
                                   cwd=path, capture_output=True, text=True, encoding="utf-8", timeout=10)
            self.assertEqual(build.returncode, 0, build.stderr)
            self.assertTrue(json.loads(build.stdout)["ok"])
            result = subprocess.run([str(path / "main.exe")], cwd=path, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout.decode("utf-8").replace("\r\n", "\n"), "Módulo\n42\n")

    def test_arithmetic_signed_64_bit(self):
        self.execute("sys.log(2 + 3 * 4)\nsys.log(-7 / 3)\nsys.log(-7 % 3)\nsys.log(7 / -3)\n"
                     "sys.log(9223372036854775807 + 1)\nsys.log(-9223372036854775808)\n"
                     "sys.log(123456789012345 * 7)\n", "14\n-2\n-1\n-2\n-9223372036854775808\n-9223372036854775808\n864197523086415\n")

    def test_recursion_multiple_stack_arguments(self):
        self.execute("func fact(n):\n    if:\n        n <= 1 => return 1\n        el => return n * fact(n - 1)\n"
                     "func total(a,b,c,d,e,f,g,h) => a+b+c+d+e+f+g+h\n"
                     "sys.log(fact(10))\nsys.log(total(1,2,3,4,5,6,7,fact(3)))\n", "3628800\n34\n")

    def test_globals_and_evaluation_order(self):
        self.execute("x = 1\nfunc change():\n    x = 5\n    return 2\nfunc pair(a,b) => a*10+b\n"
                     "sys.log(pair(x, change()))\nx = 1\nsys.log(x + change())\nsys.log(x)\n", "12\n3\n5\n")

    def test_compound_assignment_evaluation_order(self):
        self.execute("x = 1\na = [1]\nfunc change():\n    x = 5\n    a$0 = 5\n    return 2\n"
                     "x += change()\nsys.log(x)\na$0 = 1\na$0 += change()\nsys.log(a$0)\n", "3\n3\n")

    def test_specialized_functions_and_mutual_recursion(self):
        self.execute('func identity(a) => a\nsys.log(identity(42))\nsys.log(identity("Olá"))\n'
                     'func even(n):\n    if:\n        n == 0 => return true\n        el => return odd(n-1)\n'
                     'func odd(n):\n    if:\n        n == 0 => return false\n        el => return even(n-1)\n'
                     'sys.log(even(10))\nsys.log(odd(10))\n', "42\nOlá\ntrue\nfalse\n")

    def test_nested_calls_do_not_clobber_registers(self):
        self.execute("func f(a,b,c,d,e,five) => a + b*10 + c*100 + d*1000 + e*10000 + five*100000\n"
                     "func id(x) => x\nsys.log(f(id(1),id(2),id(3),id(4),id(5),id(6)))\n", "654321\n")

    def test_string_bool_comparisons_and_short_circuit(self):
        self.execute('sys.log("á" == "á")\nsys.log("a" < "b")\nsys.log("b" < "a")\n'
                     'sys.log(not false)\nsys.log(1 != 2)\nsys.log(false and (1 / 0))\n'
                     'sys.log(true or (1 / 0))\nsys.log(0 or 12)\n', "true\ntrue\nfalse\ntrue\ntrue\nfalse\ntrue\ntrue\n")

    def test_lists_reallocation_aliases_and_mutation(self):
        self.execute("a = [0]\nb = a\nfor i=1; <100:\n    a.push(i)\n"
                     "a$50 += 5\nsys.log(b.length)\nsys.log(b$50)\nsys.log(a$99)\n", "100\n55\n99\n")

    def test_nested_lists(self):
        self.execute("a = [[1,2],[3,4]]\nsys.log(a$1$0)\na$0$1 = 9\nsys.log(a$0$1)\n", "3\n9\n")

    def test_class_objects_methods_and_returned_objects(self):
        self.execute('cl Player:\n    .args($name):\n        this.name = name\n        this.health = 100\n'
                     '    .damage(amount) => this.health -= amount\n    .alive() => this.health > 0\n'
                     'func create(name) => Player(name)\na = create("João")\nb = create("Ana")\n'
                     'a.damage(20)\nsys.log(a.name)\nsys.log(a.health)\nsys.log(b.health)\nsys.log(a.alive())\n'
                     'players = [a,b]\nsys.log(players$1.name)\n', "João\n80\n100\ntrue\nAna\n")

    def test_inline_objects(self):
        self.execute('jogador => nome: "Alex"; vida: 100; nivel: 5\n'
                     'jogador.vida -= 10\nsys.log(jogador.nome)\nsys.log(jogador.vida)\n', "Alex\n90\n")

    def test_nested_loops_break_continue_and_explicit_step(self):
        self.execute("total = 0\nfor i=0; <5:\n    if:\n        i == 2 => continue\n"
                     "    for j=0; <5:\n        if:\n            j == 3 => break\n        total += 1\n"
                     "sys.log(total)\nfor k=3; >0; k -= 1:\n    sys.log(k)\n", "12\n3\n2\n1\n")

    def test_runtime_errors(self):
        for source, message in [("sys.log(1 / 0)", "divisao por zero"),
                                ("sys.log(-9223372036854775808 / -1)", "overflow na divisao int64"),
                                ("a = [1]\nsys.log(a$1)", "indice fora da lista"),
                                ("a = [1]\na$(-1) = 3", "indice de lista negativo")]:
            with self.subTest(source=source):
                self.execute(source, f"Slai runtime: {message}\n", 1)

    def test_application(self):
        self.execute((ROOT / "examples/application.slai").read_text(encoding="utf-8"),
                     "Alex\n75\n42\n3628800\nAna\nLucas\nPedro\nJoão\nMaria\nPartida concluída!\n")


if __name__ == "__main__":
    unittest.main()
