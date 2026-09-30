import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import test_compiler as harness
from slai.frontend import SlaiError
from slai.ia import apply, preview
from slai.native import build_pe
from slai.semantic import compile_file


CLASSES = '''cl Player:
    .args():
        this.health = 100
    .damage(amount) => this.health -= amount
cl Enemy:
    .args():
        this.health = 50
player = Player()
enemy = Enemy()
player.damage(10)
sys.log(player.health)
sys.log(enemy.health)
? Player.health is a comment
sys.log("Player.health is text")
'''


class SemanticIATests(unittest.TestCase):
    def project(self, source=CLASSES):
        temporary = tempfile.TemporaryDirectory(prefix="slai ia ")
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name) / "main.slai"
        path.write_text(source, encoding="utf-8", newline="")
        return path, compile_file(path)

    def test_field_rename_is_bound_to_correct_class(self):
        path, compiler = self.project()
        change = compiler.index.rename("Player.health", "hp")
        after = change["files"][0]["after"]
        self.assertIn("this.hp = 100", after)
        self.assertIn("this.hp -= amount", after)
        self.assertIn("sys.log(player.hp)", after)
        self.assertIn("this.health = 50", after)
        self.assertIn("sys.log(enemy.health)", after)
        self.assertIn('"Player.health is text"', after)
        self.assertIn("? Player.health is a comment", after)
        self.assertEqual(path.read_text(encoding="utf-8"), CLASSES)
        self.assertIn("-        this.health", preview(change))
        apply(change)
        compile_file(path)

    def test_class_rename_does_not_rename_this(self):
        path, compiler = self.project()
        change = compiler.index.rename("Player", "Avatar")
        self.assertIn("this.health = 100", change["files"][0]["after"])
        apply(change)
        compile_file(path)

    def test_method_rename(self):
        path, compiler = self.project()
        change = compiler.index.rename("Player.damage", "hit")
        self.assertIn(".hit(amount)", change["files"][0]["after"])
        self.assertIn("player.hit(10)", change["files"][0]["after"])
        apply(change)
        compile_file(path)

    def test_edit_parameter_updates_calls(self):
        path, compiler = self.project()
        change = compiler.index.add_parameter("Player.damage", "armor", "int")
        after = change["files"][0]["after"]
        self.assertIn(".damage(amount, armor: int)", after)
        self.assertIn("player.damage(10, 0)", after)
        apply(change)
        compile_file(path)

    def test_generic_ambiguous_field_rename_refused(self):
        _, compiler = self.project(CLASSES + "func health(x) => x.health\nsys.log(health(player))\nsys.log(health(enemy))\n")
        with self.assertRaisesRegex(SlaiError, "ambígua"):
            compiler.index.rename("Player.health", "hp")

    def test_unknown_generic_receiver_refused(self):
        _, compiler = self.project(CLASSES + "func unused(x) => x.health\n")
        with self.assertRaisesRegex(SlaiError, "ambígua"):
            compiler.index.rename("Player.health", "hp")

    def test_shadowed_parameter_rename(self):
        path, compiler = self.project("x = 10\nfunc identity(x) => x\nsys.log(identity(x))\n")
        parameter = next(e for e in compiler.index.entries.values() if e.kind == "parameter")
        change = compiler.index.rename(parameter.id, "value")
        self.assertEqual(change["files"][0]["after"], "x = 10\nfunc identity(value) => value\nsys.log(identity(x))\n")
        apply(change)
        compile_file(path)

    def test_collision_and_reserved_name(self):
        _, compiler = self.project()
        for name in ("damage", "return"):
            with self.subTest(name=name), self.assertRaises(SlaiError):
                compiler.index.rename("Player.health", name)

    def test_stale_change_preserves_modified_file(self):
        path, compiler = self.project()
        change = compiler.index.rename("Player.health", "hp")
        path.write_text(CLASSES + "? user edit\n", encoding="utf-8")
        with self.assertRaisesRegex(SlaiError, "alterado"):
            apply(change)
        self.assertIn("? user edit", path.read_text(encoding="utf-8"))

    def test_multifile_bom_crlf_and_rollback(self):
        path, _ = self.project()
        other = path.parent / "other.slai"
        other.write_bytes(b"\xef\xbb\xbffunc value() => 42\r\n")
        path.write_bytes(b"import $other.slai\r\nsys.log(value())\r\n")
        compiler = compile_file(path)
        change = compiler.index.rename("value", "answer")
        original = {p: p.read_bytes() for p in (path, other)}
        real_replace = Path.replace
        calls = 0

        def fail_second(source, target):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("injected I/O failure")
            return real_replace(source, target)

        with patch.object(Path, "replace", fail_second), self.assertRaises(OSError):
            apply(change)
        for p, raw in original.items():
            self.assertEqual(p.read_bytes(), raw)
        apply(change)
        self.assertEqual(other.read_bytes(), b"\xef\xbb\xbffunc answer() => 42\r\n")
        compile_file(path)


@unittest.skipUnless(harness.WINDOWS_X64, "Native tests require Windows x64")
class NativeIATests(unittest.TestCase):
    def test_native_queries(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "main.slai"
            source.write_text(CLASSES + 'sys.log(sys.ia.inspect(Player))\nsys.log(sys.ia.refs(Player.health))\nsys.log(sys.ia.errors())\n', encoding="utf-8")
            exe = source.with_suffix(".exe")
            exe.write_bytes(build_pe(compile_file(source)))
            result = subprocess.run([str(exe)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.decode("utf-8").splitlines()
            self.assertEqual(json.loads(lines[-3])["kind"], "class")
            self.assertGreaterEqual(len(json.loads(lines[-2])), 2)
            self.assertEqual(json.loads(lines[-1]), [])

    def test_native_preview_apply_and_stale_snapshot(self):
        with tempfile.TemporaryDirectory(prefix="slai ç ") as directory:
            source = Path(directory) / "main.slai"
            original = CLASSES + 'change = sys.ia.rename(Player.health, hp)\nsys.ia.apply(change)\n'
            source.write_bytes(original.encode("utf-8"))
            compiler = compile_file(source)
            # Compilation must not mutate the source, even with apply in the AST.
            self.assertEqual(source.read_bytes(), original.encode("utf-8"))
            exe = source.with_suffix(".exe")
            exe.write_bytes(build_pe(compiler))
            result = subprocess.run([str(exe)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn("this.hp = 100", source.read_text(encoding="utf-8"))
            self.assertIn("this.health = 50", source.read_text(encoding="utf-8"))
            modified = source.read_bytes()
            result = subprocess.run([str(exe)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(source.read_bytes(), modified)

    def test_native_edit_parameter(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "main.slai"
            source.write_text(CLASSES + 'change = sys.ia.edit(Player.damage):\n    .param armor, int\nsys.ia.apply(change)\n', encoding="utf-8")
            exe = source.with_suffix(".exe")
            exe.write_bytes(build_pe(compile_file(source)))
            result = subprocess.run([str(exe)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn("player.damage(10, 0)", source.read_text(encoding="utf-8"))
