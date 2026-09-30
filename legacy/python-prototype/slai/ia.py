"""Semantic project index and reviewed source changes shared by CLI and sys.ia.

Bindings come from AST scopes and record flow, never a text-name replacement.
Generic member uses with multiple possible owners are deliberately non-renamable.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass, field
import difflib
import hashlib
import os
from pathlib import Path
import tempfile

from .frontend import Node, SlaiError, Token, lex


RESERVED = set("sys this true false if el for func cl obj import return break continue await error not and or".split())


def key(token):
    return token.file, token.start, token.end


def ia_operation(node):
    if node.kind != "call":
        return None
    callee = node.children[0]
    if callee.kind != "member":
        return None
    owner = callee.children[0]
    if owner.kind == "member" and owner.value == "ia" and owner.children[0].kind == "name" and owner.children[0].value == "sys":
        return callee.value
    return None


@dataclass
class Entry:
    id: str
    name: str
    kind: str
    owner: str
    token: Token
    types: set[str] = field(default_factory=set)
    refs: dict = field(default_factory=dict)
    members: dict = field(default_factory=dict)
    parameters: list[str] = field(default_factory=list)
    dependencies: set[str] = field(default_factory=set)


class ProjectIndex:
    def __init__(self, nodes, sources):
        self.nodes, self.sources = nodes, sources
        self.entries: dict[str, Entry] = {}
        self.globals, self.bindings, self.functions, self.returns = {}, {}, {}, {}
        self.unresolved_members = set()
        self.changed = False
        self._declare()
        # A monotone fixed point propagates record types through aliases,
        # parameters, return values and calls, including unused class methods.
        for _ in range(128):
            self.changed = False
            self.unresolved_members.clear()
            self.walk(self.nodes, self.globals.copy(), "", None)
            for identity, (node, cls) in list(self.functions.items()):
                entry = self.entries[identity]
                env = self.globals.copy()
                env.update({self.entries[p].name: p for p in entry.parameters})
                if cls:
                    env["this"] = cls
                self.walk(node.children, env, identity, cls)
                if node.children and node.children[-1].kind == "expr":
                    types = self.expression(node.children[-1].children[0], env, identity)
                    self.grow_return(identity, types)
            if not self.changed:
                break
        else:
            raise SlaiError(nodes[0].token, "Índice semântico excedeu limite de convergência.", "ELIMIT")

    def add(self, name, kind, owner, token):
        identity = f"{token.file}:{token.start}:{kind}:{owner}"
        if identity not in self.entries:
            self.entries[identity] = Entry(identity, name, kind, owner, token)
            self.changed = True
        self.bind(token, identity)
        return identity

    def bind(self, token, identity):
        self.bindings.setdefault(key(token), set()).add(identity)
        entry = self.entries[identity]
        if key(token) != key(entry.token):
            entry.refs[key(token)] = token

    def grow(self, identity, types):
        entry = self.entries[identity]
        if not types <= entry.types:
            entry.types |= types
            self.changed = True

    def grow_return(self, identity, types):
        previous = self.returns.setdefault(identity, set())
        if not types <= previous:
            previous.update(types)
            self.changed = True

    def _declare_function(self, node, owner="", cls=None):
        identity = self.add(node.value, "method" if cls else "function", owner, node.token)
        self.functions[identity] = node, cls
        entry = self.entries[identity]
        for token, annotation in node.data["params"]:
            param = self.add(token.text, "parameter", identity, token)
            entry.parameters.append(param)
            if annotation:
                self.grow(param, {annotation})
        return identity

    def _declare(self):
        for node in self.nodes:
            if node.kind == "class":
                cls = self.add(node.value, "class", "", node.token)
                self.globals[node.value] = cls
                self.grow(cls, {cls})
                for method in node.children:
                    identity = self._declare_function(method, cls, cls)
                    self.entries[cls].members[method.value] = identity
                    if method.value == "args":
                        for stmt in method.children:
                            if stmt.kind == "assign":
                                target = stmt.children[0]
                                if target.kind == "member" and target.children[0].kind == "name" and target.children[0].value == "this":
                                    self.member(cls, target, "property")
            elif node.kind == "func":
                self.globals[node.value] = self._declare_function(node)
        for node in self.nodes:
            if node.kind == "assign" and node.children[0].kind == "name":
                target = node.children[0]
                if target.value not in self.globals:
                    self.globals[target.value] = self.add(target.value, "variable", "", target.token)

    def member(self, record, node, kind="property"):
        entry = self.entries[record]
        if node.value not in entry.members:
            entry.members[node.value] = self.add(node.value, kind, record, node.token)
        identity = entry.members[node.value]
        self.bind(node.token, identity)
        return identity

    def expression(self, node, env, owner):
        op = ia_operation(node)
        if op:
            args = node.children[1:]
            if args and op in ("inspect", "refs", "type", "tree", "dependencies", "rename", "edit"):
                self.expression(args[0], env, owner)
            return {"change" if op in ("rename", "edit") else "string"}
        if node.kind in ("int", "bool", "string"):
            return {node.kind}
        if node.kind == "name":
            identity = env.get(node.value)
            if identity:
                if node.value != "this":
                    self.bind(node.token, identity)
                if owner and identity != owner:
                    self.entries[owner].dependencies.add(identity)
                return self.entries[identity].types.copy()
            return set()
        if node.kind == "object":
            identity = self.add(f"object@{node.token.line}", "object", owner, node.token)
            self.grow(identity, {identity})
            for child in node.children:
                field_id = self.member(identity, child)
                self.grow(field_id, self.expression(child.children[0], env, owner))
            return {identity}
        if node.kind == "member":
            types = self.expression(node.children[0], env, owner)
            result, resolved = set(), False
            for type in types:
                record = self.entries.get(type)
                if record and node.value in record.members:
                    identity = record.members[node.value]
                    self.bind(node.token, identity)
                    if owner:
                        self.entries[owner].dependencies.add(identity)
                    result |= self.entries[identity].types
                    resolved = True
            # Known Slai builtins do not participate in user renames.
            base = node.children[0]
            builtin = base.kind == "name" and base.value == "sys"
            builtin |= base.kind == "member" and base.value == "ia"
            if not resolved and not builtin and node.value not in ("length", "push"):
                self.unresolved_members.add(key(node.token))
            return result
        if node.kind == "call":
            callee, *args = node.children
            self.expression(callee, env, owner)
            arguments = [self.expression(a, env, owner) for a in args]
            spawn = callee.kind == "member" and callee.value == "spawn" and callee.children[0].kind == "name" and callee.children[0].value == "sys"
            target_token = args[0].token if spawn and args else callee.token
            if spawn:
                arguments = arguments[1:]
            result = set()
            for identity in self.bindings.get(key(target_token), set()):
                entry = self.entries[identity]
                target = identity
                if entry.kind == "class":
                    result.add(identity)
                    target = entry.members.get("args")
                if target and target in self.functions:
                    function = self.entries[target]
                    for parameter, types in zip(function.parameters, arguments):
                        self.grow(parameter, types)
                    result |= self.returns.get(target, set())
                    if owner:
                        self.entries[owner].dependencies.add(target)
            return {f"task[{t}]" for t in result} if spawn else result
        if node.kind == "list":
            types = set()
            for child in node.children:
                types |= self.expression(child, env, owner)
            return {f"list[{t}]" for t in types}
        if node.kind == "index":
            types = self.expression(node.children[0], env, owner)
            self.expression(node.children[1], env, owner)
            return {t[5:-1] for t in types if t.startswith("list[")}
        if node.kind == "await":
            return {t[5:-1] for t in self.expression(node.children[0], env, owner) if t.startswith("task[")}
        types = set()
        for child in node.children:
            types |= self.expression(child, env, owner)
        if node.kind == "binary" and node.value in ("==", "!=", "<", "<=", ">", ">=", "and", "or", "&&", "||"):
            return {"bool"}
        return types

    def walk(self, nodes, env, owner, cls):
        for node in nodes:
            if node.kind in ("func", "class", "import"):
                continue
            if node.kind == "assign":
                target, value = node.children
                types = self.expression(value, env, owner)
                if target.kind == "name":
                    if target.value not in env:
                        env[target.value] = self.add(target.value, "variable", owner, target.token)
                    identity = env[target.value]
                    self.bind(target.token, identity)
                    self.grow(identity, types)
                elif target.kind == "member":
                    receiver_types = self.expression(target.children[0], env, owner)
                    for type in receiver_types:
                        if type in self.entries and target.value in self.entries[type].members:
                            identity = self.entries[type].members[target.value]
                            self.bind(target.token, identity)
                            self.grow(identity, types)
                    self.expression(target, env, owner)
                else:
                    self.expression(target, env, owner)
            elif node.kind == "return":
                if node.children:
                    self.grow_return(owner, self.expression(node.children[0], env, owner))
            elif node.kind == "if":
                for branch in node.children:
                    if branch.data["condition"]:
                        self.expression(branch.data["condition"], env, owner)
                    self.walk(branch.children, env.copy(), owner, cls)
            elif node.kind == "for":
                inner = env.copy()
                if node.value not in inner:
                    inner[node.value] = self.add(node.value, "variable", owner, node.token)
                self.grow(inner[node.value], {"int"})
                self.expression(node.data["initial"], env, owner)
                self.expression(node.data["condition"], inner, owner)
                self.walk(node.children, inner, owner, cls)
                if node.data["update"]:
                    self.walk([node.data["update"]], inner, owner, cls)
            elif node.kind == "catch":
                self.walk([node.children[0]], env, owner, cls)
                inner = env.copy()
                inner[node.value] = self.add(node.value, "variable", owner, node.data["error_token"])
                self.grow(inner[node.value], {"string"})
                self.walk(node.children[1:], inner, owner, cls)
            else:
                for child in node.children:
                    self.expression(child, env, owner)

    def resolve(self, target):
        if isinstance(target, Node):
            candidates = self.bindings.get(key(target.token), set())
            if len(candidates) == 1:
                return self.entries[next(iter(candidates))]
            token = target.token
            text = target.value
        else:
            token = Token("", "", "<sys.ia>", 1, 1)
            text = target
            if target in self.entries:
                return self.entries[target]
            parts = target.split(".")
            identity = self.globals.get(parts[0])
            for part in parts[1:]:
                if identity:
                    identity = self.entries[identity].members.get(part)
            if identity:
                return self.entries[identity]
        raise SlaiError(token, f"Símbolo inexistente ou ambíguo: {text}.", "EIA")

    def describe(self, entry):
        return dict(id=entry.id, name=entry.name, kind=entry.kind, owner=entry.owner,
                    type=sorted(entry.types), declaration=asdict(entry.token),
                    references=[asdict(t) for t in sorted(entry.refs.values(), key=key)],
                    members={n: self.describe(self.entries[i]) for n, i in entry.members.items()},
                    parameters=[self.entries[i].name for i in entry.parameters])

    def query(self, operation, target=None):
        if operation == "errors":
            return []
        if target is None:
            return [self.describe(e) for e in self.entries.values()]
        entry = self.resolve(target)
        if operation in ("inspect", "tree"):
            return self.describe(entry)
        if operation == "refs":
            return self.describe(entry)["references"]
        if operation == "type":
            return sorted(entry.types)
        if operation == "dependencies":
            roots = {entry.id} | set(entry.members.values())
            dependencies = set().union(*(self.entries[i].dependencies for i in roots)) - roots
            return [dict(id=i, name=self.entries[i].name, kind=self.entries[i].kind) for i in sorted(dependencies)]
        raise SlaiError(entry.token, f"Consulta sys.ia desconhecida: {operation}.", "EIA")

    def rename(self, target, new_name):
        entry = self.resolve(target)
        if not new_name.isidentifier() or new_name in RESERVED:
            raise SlaiError(entry.token, f"Nome inválido/reservado: {new_name}.", "EIA")
        if entry.kind == "method" and entry.name == "args":
            raise SlaiError(entry.token, "O construtor args não pode ser renomeado.", "EIA")
        if entry.kind in ("property", "method"):
            members = self.entries[entry.owner].members
            if new_name in members and members[new_name] != entry.id:
                raise SlaiError(entry.token, "Novo nome colide com outro membro.", "EIA")
            # A generic unresolved .health could bind to this class in a future
            # call. Refuse partial renames instead of silently missing the use.
            ambiguous = any(k in self.unresolved_members for k in self._member_tokens(entry.name))
        else:
            ambiguous = False
            if any(e.name == new_name and e.id != entry.id and e.kind not in ("property", "method", "object") for e in self.entries.values()):
                raise SlaiError(entry.token, "Renomeação pode capturar ou colidir com outro símbolo.", "EIA")
        tokens = [entry.token] + list(entry.refs.values())
        if ambiguous or any(self.bindings.get(key(t), set()) != {entry.id} for t in tokens):
            raise SlaiError(entry.token, "Referência genérica ambígua; especialize o código antes de renomear.", "EIA")
        edits = {}
        for token in tokens:
            edits.setdefault(token.file, {})[(token.start, token.end)] = new_name
        return self.change("rename", edits, dict(symbol=entry.id, old=entry.name, new=new_name))

    def _member_tokens(self, name):
        for file, source in self.sources.items():
            tokens = lex(source, file)
            for i, token in enumerate(tokens):
                if token.text == name and i and tokens[i - 1].kind == ".":
                    yield key(token)

    def add_parameter(self, target, name, type):
        entry = self.resolve(target)
        if entry.id not in self.functions:
            raise SlaiError(entry.token, "Edição de parâmetros exige função ou método.", "EIA")
        if not name.isidentifier() or name in RESERVED or type not in ("int", "bool", "string"):
            raise SlaiError(entry.token, "Parâmetro ou tipo inválido.", "EIA")
        if any(self.entries[p].name == name for p in entry.parameters):
            raise SlaiError(entry.token, "Parâmetro duplicado.", "EIA")
        tokens = lex(self.sources[entry.token.file], entry.token.file)
        after = next(i for i, t in enumerate(tokens) if key(t) == key(entry.token))
        closing = next(t for t in tokens[after:] if t.kind == ")")
        prefix = ", " if entry.parameters else ""
        edits = {entry.token.file: {(closing.start, closing.start): f"{prefix}{name}: {type}"}}
        default = {"int": "0", "bool": "false", "string": '""'}[type]

        def visit(node):
            if node.kind == "call" and not ia_operation(node):
                callee = node.children[0]
                spawn = callee.kind == "member" and callee.value == "spawn" and callee.children[0].kind == "name" and callee.children[0].value == "sys"
                target = node.children[1] if spawn and len(node.children) > 1 else callee
                candidates = self.bindings.get(key(target.token), set())
                if entry.id in candidates:
                    if candidates != {entry.id}:
                        raise SlaiError(target.token, "Chamada genérica ambígua para edição de parâmetro.", "EIA")
                    call_tokens = lex(self.sources[callee.token.file], callee.token.file)
                    start = next(i for i, t in enumerate(call_tokens) if key(t) == key(callee.token)) + 1
                    if call_tokens[start].kind != "(":
                        raise SlaiError(callee.token, "Chamada indireta não editável.", "EIA")
                    depth = 0
                    for token in call_tokens[start:]:
                        if token.kind == "(":
                            depth += 1
                        elif token.kind == ")":
                            depth -= 1
                            if depth == 0:
                                prefix = ", " if len(node.children) > 1 else ""
                                edits.setdefault(token.file, {})[(token.start, token.start)] = prefix + default
                                break
            for child in node.children:
                visit(child)
            for item in node.data.values():
                if isinstance(item, Node):
                    visit(item)

        for node in self.nodes:
            visit(node)
        return self.change("edit", edits,
                           dict(symbol=entry.id, parameter=name, type=type))

    def change(self, operation, edits, detail):
        files = []
        for file, replacements in sorted(edits.items()):
            before = self.sources[file]
            after = before
            for (start, end), text in sorted(replacements.items(), reverse=True):
                after = after[:start] + text + after[end:]
            if after == before:
                continue
            # Source offsets exclude a BOM, but change snapshots preserve it.
            path = Path(file)
            raw = path.read_bytes() if path.is_file() else before.encode("utf-8")
            bom = raw.startswith(b"\xef\xbb\xbf")
            expected = (b"\xef\xbb\xbf" if bom else b"") + before.encode("utf-8")
            if raw != expected:
                raise SlaiError(Token("", "", file, 1, 1), "Arquivo mudou durante a análise.", "ESTALE")
            files.append(dict(path=file, before=before, after=after, bom=bom,
                              sha256=hashlib.sha256(raw).hexdigest()))
        return dict(version=1, operation=operation, detail=detail, files=files)


def preview(change):
    return "".join("".join(difflib.unified_diff(f["before"].splitlines(keepends=True),
                                               f["after"].splitlines(keepends=True),
                                               fromfile=f["path"], tofile=f["path"])) for f in change["files"])


def encoded(file, which):
    return (b"\xef\xbb\xbf" if file["bom"] else b"") + file[which].encode("utf-8")


def apply(change):
    """Preflight every file, stage all writes, replace, rollback on I/O errors.

    Never rebuild a change against newer files. A stale snapshot is an error.
    Multi-file changes are rollback-capable, not crash-atomic transactions.
    """
    staged, committed = [], []
    try:
        for file in change["files"]:
            path = Path(file["path"])
            if path.is_symlink() or not path.is_file() or path.read_bytes() != encoded(file, "before"):
                raise SlaiError(Token("", "", str(path), 1, 1), "Arquivo alterado desde o preview; gere uma nova mudança.", "ESTALE")
        for file in change["files"]:
            path = Path(file["path"])
            with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".slai-", delete=False) as out:
                out.write(encoded(file, "after"))
                out.flush()
                os.fsync(out.fileno())
                staged.append((Path(out.name), path, file))
            os.chmod(out.name, path.stat().st_mode)
        for temporary, path, file in staged:
            if path.read_bytes() != encoded(file, "before"):
                raise SlaiError(Token("", "", str(path), 1, 1), "Arquivo alterado durante aplicação.", "ESTALE")
            temporary.replace(path)
            committed.append((path, file))
    except BaseException:
        for path, file in reversed(committed):
            path.write_bytes(encoded(file, "before"))
        raise
    finally:
        for temporary, _, _ in staged:
            temporary.unlink(missing_ok=True)
    return dict(applied=True, files=[f["path"] for f in change["files"]])
