"""Name/type analysis and lowering to typed, non-executable Slai IR."""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import json

from .frontend import Node, SlaiError, Token, parse


@dataclass(frozen=True)
class Value:
    slot: int
    type: str


@dataclass
class Instruction:
    op: str
    out: Value | None = None
    args: tuple = ()


@dataclass
class Function:
    name: str
    parameters: list[Value] = field(default_factory=list)
    instructions: list[Instruction] = field(default_factory=list)
    slots: int = 0
    result: str | None = None
    complete: bool = False

    def temp(self, type):
        self.slots += 1
        return Value(self.slots, type)

    def emit(self, op, out=None, *args):
        ins = Instruction(op, out, args)
        self.instructions.append(ins)
        return out


@dataclass
class Record:
    name: str
    fields: dict[str, str] = field(default_factory=dict)
    methods: dict[str, Node] = field(default_factory=dict)


@dataclass
class Symbol:
    id: int
    name: str
    kind: str
    type: str
    token: Token
    references: list[Token] = field(default_factory=list)


class Context:
    def __init__(self, compiler, function, constructor=None):
        self.compiler, self.function = compiler, function
        self.variables: dict[str, Value] = {}
        self.symbols: dict[str, Symbol] = {}
        self.loops = []
        self.constructor = constructor
        self.depth = 0
        self.error_target = None

    def check_error(self, force_propagation=False):
        self.function.emit("error_check", None, None if force_propagation else self.error_target)

    def fail(self, node, message, code="ETYPE"):
        raise SlaiError(node.token, message, code)

    def emit(self, op, type=None, *args):
        out = self.function.temp(type) if type else None
        self.function.emit(op, out, *args)
        return out

    def constant(self, type, value):
        return self.emit("const", type, value)

    def symbol(self, name, type, token, kind="variable"):
        symbol = self.compiler.add_symbol(name, kind, type, token)
        self.symbols[name] = symbol
        return symbol

    def bind(self, name, value, token):
        if name in ("sys", "this", "true", "false") or name in self.compiler.definitions:
            raise SlaiError(token, f"Nome reservado ou já declarado: {name}.", "ENAME")
        if self is self.compiler.main:
            self.compiler.globals += 1
            target = Value(-self.compiler.globals, value.type)
        else:
            target = self.function.temp(value.type)
        self.variables[name] = target
        self.symbol(name, value.type, token)
        self.function.emit("copy", target, value)
        return target

    def lookup(self, node):
        name = node.value
        owner = self if name in self.variables else self.compiler.main
        if name not in owner.variables:
            self.fail(node, f"Nome não definido: {name}.", "ENAME")
        owner.symbols[name].references.append(node.token)
        return owner.variables[name]

    def require(self, value, type, node):
        if value.type != type:
            self.fail(node, f"Esperado {type}; recebido {value.type}.")
        return value

    def truth(self, value, node):
        if value.type not in ("int", "bool"):
            self.fail(node, "Condição deve ser bool ou int.")
        return value

    def expression(self, node):
        k = node.kind
        if k == "propagate":
            if node.children[0].kind not in ("call", "await"):
                self.fail(node, "? deve seguir uma chamada ou await.")
            saved = self.error_target
            self.error_target = None
            result = self.expression(node.children[0])
            self.error_target = saved
            return result
        if k == "await":
            task = self.expression(node.children[0])
            if not task.type.startswith("task["):
                self.fail(node, "await exige uma task produzida por sys.spawn.")
            result = self.emit("await", task.type[5:-1], task)
            self.check_error()
            return result
        if k in ("int", "string", "bool"):
            if k == "int" and not -(1 << 63) <= node.value < (1 << 63):
                self.fail(node, "Literal fora do intervalo int64.")
            return self.constant(k, node.value)
        if k == "name":
            value = self.lookup(node)
            # Snapshot mutable storage before evaluating later operands/arguments.
            return self.emit("copy", value.type, value)
        if k == "unary":
            child = node.children[0]
            if node.value == "-" and child.kind == "int" and child.value == 1 << 63:
                return self.constant("int", -(1 << 63))
            value = self.expression(child)
            if node.value in ("!", "not"):
                self.truth(value, node)
                return self.emit("not", "bool", value)
            self.require(value, "int", node)
            return value if node.value == "+" else self.emit("neg", "int", value)
        if k == "binary":
            left = self.expression(node.children[0])
            if node.value in ("and", "or", "&&", "||"):
                self.truth(left, node)
                result = self.constant("bool", node.value in ("or", "||"))
                done = self.compiler.label()
                self.function.emit("branch", None, left, done, node.value in ("or", "||"))
                right = self.truth(self.expression(node.children[1]), node)
                normalized = self.emit("bool", "bool", right)
                self.function.emit("copy", result, normalized)
                self.function.emit("label", None, done)
                return result
            right = self.expression(node.children[1])
            return self.binary(node.value, left, right, node)
        if k == "list":
            if not node.children:
                self.fail(node, "Lista vazia ainda não permite inferir o tipo dos elementos.")
            values = [self.expression(n) for n in node.children]
            type = values[0].type
            for value in values:
                self.require(value, type, node)
            result = self.emit("list_new", f"list[{type}]")
            for value in values:
                self.function.emit("list_push", None, result, value)
            return result
        if k == "index":
            collection = self.expression(node.children[0])
            index = self.require(self.expression(node.children[1]), "int", node)
            if not collection.type.startswith("list["):
                self.fail(node, "Indexação $ exige uma lista.")
            return self.emit("list_get", collection.type[5:-1], collection, index)
        if k == "object":
            type = f"object@{node.token.file}:{node.token.start}:{self.function.name}"
            record = self.compiler.records.setdefault(type, Record(type))
            values = []
            seen = set()
            for field_node in node.children:
                if field_node.value in seen:
                    self.fail(field_node, f"Campo duplicado: {field_node.value}.")
                seen.add(field_node.value)
                value = self.expression(field_node.children[0])
                record.fields[field_node.value] = value.type
                values.append(value)
            result = self.emit("alloc", type, max(8, len(values) * 8))
            for i, value in enumerate(values):
                self.function.emit("store_field", None, result, i * 8, value)
            return result
        if k == "member":
            obj = self.expression(node.children[0])
            if obj.type.startswith("list[") and node.value == "length":
                return self.emit("load_field", "int", obj, 0)
            record = self.compiler.records.get(obj.type)
            if not record or node.value not in record.fields:
                self.fail(node, f"Campo {node.value} não existe em {obj.type}.", "ENAME")
            return self.emit("load_field", record.fields[node.value], obj, list(record.fields).index(node.value) * 8)
        if k == "call":
            return self.call(node)
        self.fail(node, f"Expressão não suportada: {k}.", "EUNSUPPORTED")

    def binary(self, op, left, right, node):
        self.require(right, left.type, node)
        if op in ("==", "!=", "<", "<=", ">", ">="):
            if left.type not in ("int", "bool", "string"):
                self.fail(node, "Comparação requer int, bool ou string.")
            return self.emit("compare", "bool", op, left, right)
        self.require(left, "int", node)
        return self.emit("binary", "int", op, left, right)

    def call(self, node):
        callee, *args = node.children
        from .ia import ia_operation, preview
        operation = ia_operation(node)
        if operation:
            index = self.compiler.index
            if operation in ("inspect", "refs", "tree", "type", "dependencies", "errors"):
                if len(args) != (0 if operation == "errors" else 1):
                    self.fail(node, f"Aridade inválida de sys.ia.{operation}.", "EIA")
                payload = index.query(operation, args[0] if args else None)
                return self.constant("string", json.dumps(payload, ensure_ascii=False, sort_keys=True))
            if operation == "rename":
                if len(args) != 2 or args[1].kind not in ("name", "string"):
                    self.fail(node, "rename exige símbolo e novo nome literal.", "EIA")
                change = index.rename(args[0], args[1].value)
            elif operation == "edit":
                params = node.data.get("add_parameters", [])
                if len(args) != 1 or len(params) != 1:
                    self.fail(node, "edit exige um símbolo e uma diretiva .param por mudança.", "EIA")
                change = index.add_parameter(args[0], *params[0])
            elif operation in ("preview", "apply"):
                if len(args) != 1:
                    self.fail(node, f"{operation} exige uma mudança.", "EIA")
                value = self.expression(args[0])
                if not value.type.startswith("change:"):
                    self.fail(node, "Esperada mudança criada por sys.ia.rename/edit.", "EIA")
                change_id = int(value.type.split(":")[1])
                change = self.compiler.changes[change_id]
                if operation == "preview":
                    return self.constant("string", preview(change))
                self.function.emit("ia_apply", None, change_id)
                self.check_error()
                return self.constant("bool", True)
            else:
                self.fail(node, f"Operação sys.ia não implementada: {operation}.", "EUNSUPPORTED")
            change_id = len(self.compiler.changes)
            self.compiler.changes.append(change)
            return self.constant(f"change:{change_id}", change_id)
        if callee.kind == "member" and callee.children[0].kind == "name" and callee.children[0].value == "sys":
            if callee.value == "spawn":
                if not args or args[0].kind != "name" or args[0].value not in self.compiler.definitions:
                    self.fail(node, "sys.spawn exige uma função e seus argumentos.")
                declaration = self.compiler.definitions[args[0].value]
                if declaration.kind != "func":
                    self.fail(node, "sys.spawn exige uma função, não uma classe.")
                values = [self.expression(a) for a in args[1:]]
                function = self.compiler.instantiate(declaration, values)
                result = self.emit("spawn", f"task[{function.result or 'int'}]", function.name, tuple(values))
                self.check_error()
                return result
            if callee.value in ("assert", "sleep"):
                if len(args) != 1:
                    self.fail(node, f"sys.{callee.value} exige um argumento.")
                value = self.expression(args[0])
                if callee.value == "assert":
                    self.truth(value, node)
                    self.function.emit("assert", None, value, f"{node.token.file}:{node.token.line}: assertion failed")
                    self.check_error()
                else:
                    self.require(value, "int", node)
                    self.function.emit("sleep", None, value)
                return self.constant("void", 0)
            if callee.value != "log":
                self.fail(callee, "Somente sys.log está disponível no runtime atual.", "EUNSUPPORTED")
            if len(args) != 1:
                self.fail(node, "sys.log exige exatamente um argumento.")
            value = self.expression(args[0])
            if value.type not in ("int", "bool", "string"):
                self.fail(node, "sys.log aceita int, bool ou string.")
            self.function.emit("log", None, value)
            return self.constant("void", 0)
        if callee.kind == "member":
            obj = self.expression(callee.children[0])
            if obj.type.startswith("list[") and callee.value == "push":
                if len(args) != 1:
                    self.fail(node, "push exige um argumento.")
                value = self.require(self.expression(args[0]), obj.type[5:-1], node)
                self.function.emit("list_push", None, obj, value)
                return self.constant("void", 0)
            record = self.compiler.records.get(obj.type)
            if not record or callee.value not in record.methods or callee.value == "args":
                self.fail(callee, f"Método não definido: {callee.value}.", "ENAME")
            values = [obj] + [self.expression(a) for a in args]
            function = self.compiler.instantiate(record.methods[callee.value], values, obj.type)
        elif callee.kind == "name":
            declaration = self.compiler.definitions.get(callee.value)
            if declaration is None:
                self.fail(callee, f"Função/classe não definida: {callee.value}.", "ENAME")
            self.compiler.definition_symbols[callee.value].references.append(callee.token)
            values = [self.expression(a) for a in args]
            if declaration.kind == "class":
                function = self.compiler.construct(declaration, values)
            else:
                function = self.compiler.instantiate(declaration, values)
        else:
            self.fail(callee, "Funções como valores e closures ainda não são suportadas.", "EUNSUPPORTED")
        # Unknown recursive return types use the documented int64 convention.
        if function.result is None:
            function.result = "int"
        result = self.emit("call", function.result, function.name, tuple(values))
        self.check_error()
        return result

    def assign(self, node):
        left, right = node.children
        if left.kind not in ("name", "member", "index"):
            self.fail(left, "Alvo de atribuição inválido.")
        # Resolve the lvalue once, before evaluating a potentially effectful RHS.
        obj = index = offset = None
        if left.kind == "member":
            obj = self.expression(left.children[0])
            record = self.compiler.records.get(obj.type)
            if not record:
                self.fail(left, "Atribuição de campo exige objeto/classe.")
            if left.value not in record.fields:
                if obj.type != self.constructor or self.depth:
                    self.fail(left, "Campos novos só podem ser declarados diretamente no construtor.")
        elif left.kind == "index":
            obj = self.expression(left.children[0])
            index = self.require(self.expression(left.children[1]), "int", left)
            if not obj.type.startswith("list["):
                self.fail(left, "Indexação exige lista.")
        previous = None
        if node.value != "=":
            if left.kind == "name":
                target = self.lookup(left)
                previous = self.emit("copy", target.type, target)
            elif left.kind == "index":
                previous = self.emit("list_get", obj.type[5:-1], obj, index)
            else:
                if left.value not in record.fields:
                    self.fail(left, "Campo deve ser inicializado antes da atribuição composta.")
                offset = list(record.fields).index(left.value) * 8
                previous = self.emit("load_field", record.fields[left.value], obj, offset)
        value = self.expression(right)
        if value.type == "void":
            self.fail(right, "Uma operação sem retorno não pode ser atribuída.")
        if previous:
            value = self.binary(node.value[0], previous, value, node)
        annotation = node.data.get("annotation")
        if annotation:
            if annotation not in ("int", "bool", "string"):
                self.fail(node, f"Tipo explícito desconhecido: {annotation}.")
            self.require(value, annotation, node)
        if left.kind == "name":
            if left.value in self.variables or (self is not self.compiler.main and left.value in self.compiler.main.variables):
                target = self.lookup(left)
                self.require(value, target.type, node)
                self.function.emit("copy", target, value)
            else:
                self.bind(left.value, value, left.token)
        elif left.kind == "member":
            if left.value in record.fields:
                self.require(value, record.fields[left.value], node)
            else:
                record.fields[left.value] = value.type
            offset = list(record.fields).index(left.value) * 8
            self.function.emit("store_field", None, obj, offset, value)
        else:
            self.require(value, obj.type[5:-1], node)
            self.function.emit("list_set", None, obj, index, value)

    def body(self, nodes, implicit=False):
        terminated = False
        for i, node in enumerate(nodes):
            if terminated:
                # Diagnose unreachable code too; it must still be well formed.
                self.statement(node)
                continue
            if implicit and i == len(nodes) - 1 and node.kind == "expr":
                terminated = self.statement(Node("return", node.token, children=node.children))
            else:
                terminated = self.statement(node)
        return terminated

    def statement(self, node):
        k = node.kind
        if k == "error":
            value = self.require(self.expression(node.children[0]), "string", node)
            self.function.emit("error_set", None, value)
            self.function.emit("error_jump", None, self.error_target)
            return True
        if k == "catch":
            handler, done = self.compiler.label(), self.compiler.label()
            previous = self.error_target
            original_vars, original_symbols = self.variables.copy(), self.symbols.copy()
            self.error_target = handler
            self.statement(node.children[0])
            self.error_target = previous
            success_vars, success_symbols = self.variables.copy(), self.symbols.copy()
            self.function.emit("jump", None, done)
            self.function.emit("label", None, handler)
            self.variables, self.symbols = original_vars.copy(), original_symbols.copy()
            # Existing destination storage can be assigned by the handler, but
            # cannot be read until that assignment definitely occurs.
            target = node.children[0].children[0] if node.children[0].kind == "assign" else None
            new_name = target.value if target and target.kind == "name" and target.value not in original_vars else None
            error_value = self.emit("error_take", "string")
            if node.value in self.variables or node.value == new_name:
                self.fail(node, "O nome do erro deve ser novo no handler.")
            self.bind(node.value, error_value, node.data["error_token"])
            self.depth += 1
            exits = self.body(node.children[1:])
            self.depth -= 1
            if new_name and new_name in self.variables:
                value = self.require(self.variables[new_name], success_vars[new_name].type, node)
                self.function.emit("copy", success_vars[new_name], value)
            elif new_name and not exits:
                success_vars.pop(new_name, None)
                success_symbols.pop(new_name, None)
            self.variables, self.symbols = success_vars, success_symbols
            self.function.emit("label", None, done)
            return False
        if k in ("func", "class", "import"):
            if self is not self.compiler.main or self.depth:
                self.fail(node, "Declarações de função/classe/import são permitidas apenas no topo.", "ESCOPE")
            return False
        if k == "assign":
            self.assign(node)
        elif k == "expr":
            self.expression(node.children[0])
        elif k == "return":
            if self is self.compiler.main:
                self.fail(node, "return fora de função.", "ESCOPE")
            if self.constructor:
                self.fail(node, "O construtor retorna a instância automaticamente.")
            value = self.expression(node.children[0]) if node.children else self.constant("void", 0)
            if self.function.result is not None:
                self.require(value, self.function.result, node)
            self.function.result = value.type
            self.function.emit("return", None, value)
            return True
        elif k == "if":
            done, outcomes = self.compiler.label(), []
            for branch in node.children:
                next_branch = self.compiler.label()
                condition = branch.data["condition"]
                if condition is not None:
                    value = self.truth(self.expression(condition), condition)
                    self.function.emit("branch", None, value, next_branch, False)
                saved_vars, saved_symbols = self.variables.copy(), self.symbols.copy()
                self.depth += 1
                outcomes.append(self.body(branch.children))
                self.depth -= 1
                self.variables, self.symbols = saved_vars, saved_symbols
                self.function.emit("jump", None, done)
                self.function.emit("label", None, next_branch)
            self.function.emit("label", None, done)
            return bool(outcomes) and all(outcomes) and node.children[-1].data["condition"] is None
        elif k == "for":
            initial = self.require(self.expression(node.data["initial"]), "int", node)
            saved_vars, saved_symbols = self.variables.copy(), self.symbols.copy()
            if node.value in self.variables:
                counter = self.require(self.variables[node.value], "int", node)
                self.function.emit("copy", counter, initial)
            else:
                counter = self.bind(node.value, initial, node.token)
            start, step, done = (self.compiler.label() for _ in range(3))
            self.function.emit("label", None, start)
            condition = self.truth(self.expression(node.data["condition"]), node)
            self.function.emit("branch", None, condition, done, False)
            self.loops.append((step, done))
            self.depth += 1
            self.body(node.children)
            self.depth -= 1
            self.loops.pop()
            self.function.emit("label", None, step)
            if node.data["update"]:
                self.statement(node.data["update"])
            else:
                increment = self.binary("+", counter, self.constant("int", 1), node)
                self.function.emit("copy", counter, increment)
            self.function.emit("jump", None, start)
            self.function.emit("label", None, done)
            self.variables, self.symbols = saved_vars, saved_symbols
        elif k in ("break", "continue"):
            if not self.loops:
                self.fail(node, f"{k} fora de laço.", "ESCOPE")
            self.function.emit("jump", None, self.loops[-1][k == "break"])
            return True
        else:
            self.fail(node, f"Comando não suportado: {k}.", "EUNSUPPORTED")
        return False


class Compiler:
    def __init__(self):
        self.definitions, self.definition_symbols = {}, {}
        self.records: dict[str, Record] = {}
        self.functions: list[Function] = []
        self.instances = {}
        self.symbols: list[Symbol] = []
        self.sources: dict[str, str] = {}
        self.changes = []
        self.index = None
        self.globals = self.labels = 0
        self.entry = Function("main")
        self.functions.append(self.entry)
        self.main = Context(self, self.entry)

    def label(self):
        self.labels += 1
        return f"L{self.labels}"

    def add_symbol(self, name, kind, type, token):
        for symbol in self.symbols:
            if symbol.name == name and symbol.kind == kind and symbol.token == token:
                types = symbol.type.split(" | ")
                if type not in types:
                    symbol.type = " | ".join(types + [type])
                return symbol
        symbol = Symbol(len(self.symbols), name, kind, type, token)
        self.symbols.append(symbol)
        return symbol

    def load(self, path: Path, visiting=None, loaded=None):
        path = path.resolve()
        visiting = set() if visiting is None else visiting
        loaded = set() if loaded is None else loaded
        t = Token("", "", str(path), 1, 1)
        if path in visiting:
            raise SlaiError(t, "Ciclo de imports detectado.", "EIMPORT")
        if path in loaded:
            return []
        try:
            with path.open(encoding="utf-8-sig", newline="") as stream:
                source = stream.read()
        except (OSError, UnicodeError) as exc:
            raise SlaiError(t, f"Não foi possível ler o arquivo: {exc}.", "EIMPORT") from None
        self.sources[str(path)] = source
        nodes = parse(source, str(path))
        visiting.add(path)
        result = []
        for node in nodes:
            if node.kind == "import":
                target = path.parent / str(node.value)
                if target.suffix != ".slai":
                    raise SlaiError(node.token, "Imports usam provisoriamente arquivos .slai.", "EIMPORT")
                result.extend(self.load(target, visiting, loaded))
            else:
                result.append(node)
        visiting.remove(path)
        loaded.add(path)
        return result

    def analyze(self, nodes):
        for node in nodes:
            if node.kind in ("func", "class"):
                if node.value in self.definitions or node.value in ("sys", "this", "true", "false"):
                    raise SlaiError(node.token, f"Declaração duplicada/reservada: {node.value}.", "ENAME")
                self.definitions[node.value] = node
                self.definition_symbols[node.value] = self.add_symbol(node.value, node.kind, node.kind, node.token)
                declarations = node.children if node.kind == "class" else [node]
                names = set()
                for declaration in declarations:
                    if declaration.value in names:
                        raise SlaiError(declaration.token, "Método duplicado.", "ENAME")
                    names.add(declaration.value)
                    params = [p.text for p, _ in declaration.data["params"]]
                    if len(params) != len(set(params)) or any(p in ("sys", "this", "true", "false") for p in params):
                        raise SlaiError(declaration.token, "Parâmetro duplicado ou reservado.", "ENAME")
        for declaration in self.definitions.values():
            functions = declaration.children if declaration.kind == "class" else [declaration]
            for function in functions:
                for token, annotation in function.data["params"]:
                    if token.text in self.definitions:
                        raise SlaiError(token, "Parâmetro conflita com nome de função/classe.", "ENAME")
                    if annotation is not None and annotation not in ("int", "bool", "string"):
                        raise SlaiError(token, f"Tipo explícito desconhecido: {annotation}.", "ETYPE")
        validate_names(nodes, self.definitions)
        from .ia import ProjectIndex
        self.index = ProjectIndex(nodes, self.sources)
        self.main.body(nodes)
        self.entry.emit("return", None, self.main.constant("int", 0))
        self.entry.result, self.entry.complete = "int", True
        return self

    def instantiate(self, node, values, receiver=None, constructor=None):
        params = node.data["params"]
        if len(values) != len(params) + bool(receiver):
            raise SlaiError(node.token, f"{node.value} espera {len(params)} argumento(s).", "EARITY")
        key = (node.token.file, node.token.start, tuple(v.type for v in values), receiver, constructor)
        if key in self.instances:
            return self.instances[key]
        if len(self.instances) >= 256:
            raise SlaiError(node.token, "Limite de 256 especializações por compilação atingido.", "ELIMIT")
        function = Function(f"fn{len(self.instances)}_{node.value}")
        self.instances[key] = function
        self.functions.append(function)
        ctx = Context(self, function, constructor)
        specifications = ([(Token("NAME", "this", node.token.file, node.token.line, node.token.column), receiver)] if receiver else []) + params
        for (token, annotation), value in zip(specifications, values):
            if annotation:
                ctx.require(value, annotation, node)
            slot = function.temp(value.type)
            function.parameters.append(slot)
            ctx.variables[token.text] = slot
            ctx.symbol(token.text, value.type, token, "parameter")
        if constructor:
            instance = ctx.emit("alloc", constructor, 8)
            allocation = function.instructions[-1]
            ctx.variables["this"] = instance
            ctx.symbol("this", constructor, node.token, "parameter")
        terminated = ctx.body(node.children, implicit=not constructor)
        if constructor:
            allocation.args = (max(8, len(self.records[constructor].fields) * 8),)
            function.result = constructor
            function.emit("return", None, instance)
        elif not terminated:
            if function.result not in (None, "void"):
                raise SlaiError(node.token, "Nem todos os caminhos da função retornam um valor.", "ERETURN")
            function.result = "void"
            function.emit("return", None, ctx.constant("void", 0))
        function.complete = True
        if function.result is None:
            function.result = "int"  # error-only functions have no successful value
        return function

    def construct(self, node, values):
        type = f"class:{node.value}[{','.join(v.type for v in values)}]"
        if type not in self.records:
            self.records[type] = Record(node.value, methods={n.value: n for n in node.children})
        record = self.records[type]
        ctor = record.methods.get("args")
        if ctor is None:
            ctor = Node("func", node.token, "args", [], {"params": []})
        result = self.instantiate(ctor, values, constructor=type)
        if not result.complete:
            raise SlaiError(node.token, "Construção recursiva da mesma classe ainda não é suportada.", "EUNSUPPORTED")
        return result


def validate_names(nodes, definitions):
    """Check names in every body, including uninstantiated generic functions.

    Type inference specializes called functions; name/arity errors must not hide
    in unused declarations. Block-local names never leak into other scopes.
    """
    global_names = {n.children[0].value for n in nodes
                    if n.kind == "assign" and n.children[0].kind == "name"}

    def expression(node, names):
        from .ia import ia_operation
        operation = ia_operation(node)
        if operation:
            if operation in ("preview", "apply"):
                for child in node.children[1:]:
                    expression(child, names)
            return  # symbolic arguments are resolved against ProjectIndex
        if node.kind == "name":
            if node.value not in names:
                raise SlaiError(node.token, f"Nome não definido: {node.value}.", "ENAME")
        elif node.kind == "call":
            callee = node.children[0]
            if callee.kind == "name" and callee.value in definitions:
                declaration = definitions[callee.value]
                params = declaration.data.get("params", [])
                if declaration.kind == "class":
                    constructor = next((n for n in declaration.children if n.value == "args"), None)
                    params = constructor.data["params"] if constructor else []
                if len(node.children) - 1 != len(params):
                    raise SlaiError(node.token, f"{callee.value} espera {len(params)} argumento(s).", "EARITY")
            for child in node.children:
                expression(child, names)
        else:
            for child in node.children:
                expression(child, names)

    def body(statements, names, in_function=False, loops=0):
        for node in statements:
            k = node.kind
            if k == "catch":
                success = names.copy()
                body([node.children[0]], success, in_function, loops)
                handler = names | {node.value}
                body(node.children[1:], handler, in_function, loops)
                names.update(success & handler)
            elif k == "assign":
                left, right = node.children
                expression(right, names)
                if left.kind == "name":
                    if node.value != "=" and left.value not in names:
                        raise SlaiError(left.token, f"Nome não definido: {left.value}.", "ENAME")
                    names.add(left.value)
                else:
                    expression(left, names)
            elif k == "for":
                expression(node.data["initial"], names)
                inner = names | {node.value}
                expression(node.data["condition"], inner)
                body(node.children, inner, in_function, loops + 1)
                if node.data["update"]:
                    body([node.data["update"]], inner, in_function, loops + 1)
            elif k == "if":
                for branch in node.children:
                    if branch.data["condition"]:
                        expression(branch.data["condition"], names)
                    body(branch.children, names.copy(), in_function, loops)
            elif k in ("func", "class"):
                if in_function or loops:
                    raise SlaiError(node.token, "Declaração aninhada não suportada.", "ESCOPE")
                declarations = node.children if k == "class" else [node]
                for declaration in declarations:
                    parameters = {p.text for p, _ in declaration.data["params"]}
                    scope = global_names | set(definitions) | parameters | {"sys"}
                    if k == "class":
                        scope.add("this")
                    body(declaration.children, scope, True)
            elif k in ("break", "continue") and not loops:
                raise SlaiError(node.token, f"{k} fora de laço.", "ESCOPE")
            elif k == "return" and not in_function:
                raise SlaiError(node.token, "return fora de função.", "ESCOPE")
            else:
                for child in node.children:
                    expression(child, names)

    body(nodes, set(definitions) | {"sys"})


def compile_file(path):
    compiler = Compiler()
    return compiler.analyze(compiler.load(Path(path)))


def signed(value):
    return ((value + (1 << 63)) % (1 << 64)) - (1 << 63)


def optimize(function):
    """Fold immutable temporary constants; never propagate mutable storage."""
    counts = {}
    for ins in function.instructions:
        if ins.out:
            counts[ins.out.slot] = counts.get(ins.out.slot, 0) + 1
    constants = {}
    for ins in function.instructions:
        if ins.op == "const" and counts[ins.out.slot] == 1:
            constants[ins.out.slot] = ins.args[0]
        elif ins.op == "binary":
            op, a, b = ins.args
            if a.slot in constants and b.slot in constants:
                x, y = constants[a.slot], constants[b.slot]
                if op in ("/", "%") and (y == 0 or (x == -(1 << 63) and y == -1)):
                    continue  # keep checked runtime failure
                quotient = (abs(x) // abs(y)) * (-1 if (x < 0) != (y < 0) else 1) if op in ("/", "%") else 0
                value = {"+": lambda: x + y, "-": lambda: x - y, "*": lambda: x * y,
                         "/": lambda: quotient, "%": lambda: x - quotient * y}[op]()
                ins.op, ins.args = "const", (signed(value),)
                if counts[ins.out.slot] == 1:
                    constants[ins.out.slot] = ins.args[0]
