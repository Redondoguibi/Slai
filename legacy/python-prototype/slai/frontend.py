"""Indentation-sensitive lexer and parser. No host-language evaluation."""
from __future__ import annotations

import ast
from dataclasses import dataclass, field


@dataclass(frozen=True)
class Token:
    kind: str
    text: str
    file: str
    line: int
    column: int
    start: int = 0
    end: int = 0


class SlaiError(Exception):
    def __init__(self, token: Token, message: str, code: str = "E001"):
        self.token, self.message, self.code = token, message, code
        super().__init__(f"{token.file}:{token.line}:{token.column}: {code}: {message}")

    def diagnostic(self):
        return dict(file=self.token.file, line=self.token.line,
                    column=self.token.column, code=self.code, message=self.message)


@dataclass
class Node:
    kind: str
    token: Token
    value: object = None
    children: list[Node] = field(default_factory=list)
    data: dict = field(default_factory=dict)


def lex(source: str, file: str = "<source>") -> list[Token]:
    result, indents, brackets = [], [0], []
    i, line, column, bol = 0, 1, 1, True

    def tok(kind, text, start, ln, col):
        result.append(Token(kind, text, file, ln, col, start, start + len(text)))

    def fail(message):
        raise SlaiError(Token("", "", file, line, column, i, i), message, "ELEX")

    while i < len(source):
        if bol and not brackets:
            start = i
            while i < len(source) and source[i] == " ":
                i += 1
            column = i - start + 1
            if i == len(source):
                break
            if source[i] == "\t":
                fail("Use espaços, não tabs, na indentação.")
            if source[i] not in "\r\n?" and not source.startswith("/?", i):
                width = i - start
                if width > indents[-1]:
                    indents.append(width)
                    tok("INDENT", "", i, line, column)
                while width < indents[-1]:
                    indents.pop()
                    tok("DEDENT", "", i, line, column)
                if width != indents[-1]:
                    fail("Indentação não corresponde a um bloco anterior.")
            bol = False
        c = source[i]
        if c in " \r\t":
            i, column = i + 1, column + 1
            continue
        if c == "\n":
            if not brackets and result and result[-1].kind not in ("NEWLINE", "INDENT", "DEDENT"):
                tok("NEWLINE", "\n", i, line, column)
            i, line, column, bol = i + 1, line + 1, 1, True
            continue
        if source.startswith("/?", i):
            end = source.find("?/", i + 2)
            if end < 0:
                fail("Comentário de bloco não terminado (esperado ?/).")
            comment = source[i:end + 2]
            count = comment.count("\n")
            if count:
                if not brackets and result and result[-1].kind not in ("NEWLINE", "INDENT", "DEDENT"):
                    tok("NEWLINE", "\n", i, line, column)
                line += count
                column = len(comment.rsplit("\n", 1)[-1]) + 1
            else:
                column += len(comment)
            i = end + 2
            continue
        if c == "?":
            if source.startswith("?!", i):
                tok("?!", "?!", i, line, column)
                i, column = i + 2, column + 2
                continue
            if i and not source[i - 1].isspace():
                tok("?", "?", i, line, column)
                i, column = i + 1, column + 1
                continue
            while i < len(source) and source[i] != "\n":
                i, column = i + 1, column + 1
            continue
        start, ln, col = i, line, column
        if c in "\"'":
            quote = c
            i += 1
            while i < len(source) and source[i] != quote:
                if source[i] in "\r\n":
                    fail("String não terminada na mesma linha.")
                if source[i] == "\\":
                    i += 1
                i += 1
            if i >= len(source):
                fail("String não terminada.")
            i += 1
            kind = "STRING"
        elif c in "0123456789":
            while i < len(source) and source[i] in "0123456789":
                i += 1
            kind = "INT"
            if i < len(source) and source[i] == "." and i + 1 < len(source) and source[i + 1] in "0123456789":
                fail("float ainda não é suportado; use inteiros int64.")
        elif c.isalpha() or c == "_":
            while i < len(source) and (source[i].isalnum() or source[i] == "_"):
                i += 1
            kind = "NAME"
        else:
            pair = source[i:i + 2]
            if pair in ("=>", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "&&", "||"):
                kind, i = pair, i + 2
            elif c in "=:+-*/%<>!;.,$()[]":
                kind, i = c, i + 1
            else:
                fail(f"Caractere inesperado: {c!r}.")
            if kind in ("(", "["):
                brackets.append(kind)
            elif kind in (")", "]"):
                if not brackets or brackets.pop() != {")": "(", "]": "["}[kind]:
                    fail("Delimitador sem abertura correspondente.")
        text = source[start:i]
        tok(kind, text, start, ln, col)
        column += len(text)
        bol = False
    if brackets:
        fail("Delimitador não terminado.")
    if result and result[-1].kind != "NEWLINE":
        tok("NEWLINE", "", i, line, column)
    while len(indents) > 1:
        indents.pop()
        tok("DEDENT", "", i, line, column)
    tok("EOF", "", i, line, column)
    return result


class Parser:
    precedence = {"or": 1, "||": 1, "and": 2, "&&": 2,
                  "==": 3, "!=": 3, "<": 4, "<=": 4, ">": 4, ">=": 4,
                  "+": 5, "-": 5, "*": 6, "/": 6, "%": 6}

    def __init__(self, tokens: list[Token]):
        self.tokens, self.pos = tokens, 0

    @property
    def current(self):
        return self.tokens[self.pos]

    def at(self, text):
        return self.current.kind == text or self.current.text == text

    def take(self):
        t = self.current
        self.pos += 1
        return t

    def accept(self, text):
        return self.take() if self.at(text) else None

    def expect(self, text):
        if not self.at(text):
            raise SlaiError(self.current, f"Esperado {text!r}; encontrado {self.current.text or self.current.kind!r}.", "EPARSE")
        return self.take()

    def program(self):
        nodes = []
        while not self.at("EOF"):
            if self.accept("NEWLINE"):
                continue
            nodes.append(self.statement())
        return nodes

    def block(self):
        self.expect(":")
        self.expect("NEWLINE")
        self.expect("INDENT")
        nodes = []
        while not self.at("DEDENT") and not self.at("EOF"):
            if not self.accept("NEWLINE"):
                nodes.append(self.statement())
        self.expect("DEDENT")
        return nodes

    def function(self, method=False):
        t = self.expect("." if method else "func")
        name = self.expect("NAME")
        self.expect("(")
        params = []
        if not self.at(")"):
            while True:
                self.accept("$")
                p = self.expect("NAME")
                annotation = self.expect("NAME").text if self.accept(":") else None
                params.append((p, annotation))
                if not self.accept(","):
                    break
        self.expect(")")
        if self.accept("=>"):
            body = [self.simple()]
            self.expect("NEWLINE")
        else:
            body = self.block()
        return Node("func", name, name.text, body, {"params": params, "method": method})

    def statement(self):
        t = self.current
        if self.at("func"):
            return self.function()
        if self.accept("cl"):
            name = self.expect("NAME")
            self.expect(":")
            self.expect("NEWLINE")
            self.expect("INDENT")
            methods = []
            while not self.at("DEDENT") and not self.at("EOF"):
                if not self.accept("NEWLINE"):
                    methods.append(self.function(True))
            self.expect("DEDENT")
            return Node("class", name, name.text, methods)
        if self.accept("obj"):
            name = self.expect("NAME")
            self.expect(":")
            self.expect("NEWLINE")
            self.expect("INDENT")
            fields = []
            while not self.at("DEDENT") and not self.at("EOF"):
                key = self.expect("NAME")
                self.expect(":")
                fields.append(Node("field", key, key.text, [self.expression()]))
                self.expect("NEWLINE")
            self.expect("DEDENT")
            return Node("assign", name, "=", [Node("name", name, name.text), Node("object", t, children=fields)])
        if self.accept("if"):
            self.expect(":")
            self.expect("NEWLINE")
            self.expect("INDENT")
            branches, seen_else = [], False
            while not self.at("DEDENT") and not self.at("EOF"):
                branch = self.current
                if seen_else:
                    raise SlaiError(branch, "el deve ser a última alternativa.")
                if self.accept("el"):
                    cond, seen_else = None, True
                else:
                    cond = self.expression()
                if self.accept("=>"):
                    body = [self.simple()]
                    self.expect("NEWLINE")
                else:
                    body = self.block()
                branches.append(Node("branch", branch, children=body, data={"condition": cond}))
            self.expect("DEDENT")
            return Node("if", t, children=branches)
        if self.accept("for"):
            name = self.expect("NAME")
            self.expect("=")
            initial = self.expression()
            self.expect(";")
            if self.current.text in ("<", "<=", ">", ">=", "!=", "=="):
                op = self.take()
                condition = Node("binary", op, op.text, [Node("name", name, name.text), self.expression()])
            else:
                condition = self.expression()
            update = self.simple() if self.accept(";") else None
            body = self.block()
            return Node("for", name, name.text, body, dict(initial=initial, condition=condition, update=update))
        if self.accept("import"):
            if self.at("STRING"):
                path = self.string(self.take())
            else:
                self.expect("$")
                parts = [self.expect("NAME").text]
                while self.accept("."):
                    parts.append(self.expect("NAME").text)
                path = ".".join(parts)
            self.expect("NEWLINE")
            return Node("import", t, path)
        node = self.simple()
        candidate = node.children[-1] if node.kind in ("assign", "expr") else None
        if self.at(":") and candidate and candidate.kind == "call":
            callee = candidate.children[0]
            if callee.kind == "member" and callee.value == "edit":
                self.expect(":")
                self.expect("NEWLINE")
                self.expect("INDENT")
                params = []
                while not self.at("DEDENT") and not self.at("EOF"):
                    self.expect(".")
                    self.expect("param")
                    name = self.expect("NAME")
                    self.expect(",")
                    type = self.expect("NAME")
                    params.append((name.text, type.text))
                    self.expect("NEWLINE")
                self.expect("DEDENT")
                candidate.data["add_parameters"] = params
                return node
        if self.accept("?!"):
            error_name = self.expect("NAME")
            return Node("catch", t, error_name.text, [node] + self.block(), {"error_token": error_name})
        self.expect("NEWLINE")
        return node

    def simple(self):
        t = self.current
        if self.accept("return"):
            values = [] if self.at("NEWLINE") else [self.expression()]
            return Node("return", t, children=values)
        if self.at("break") or self.at("continue"):
            return Node(self.take().text, t)
        if self.accept("error"):
            if self.at("NAME"):
                name = self.take()
                message = Node("string", name, name.text)
            else:
                message = self.expression()
            return Node("error", t, children=[message])
        self.accept(".")  # provisional local-name prefix
        left = self.expression()
        annotation = None
        if self.accept(","):
            annotation = self.expect("NAME").text
        if self.current.text in ("=", "+=", "-=", "*=", "/=", "%="):
            op = self.take()
            return Node("assign", t, op.text, [left, self.expression()], {"annotation": annotation})
        if self.accept("=>"):
            fields = []
            while True:
                key = self.expect("NAME")
                self.expect(":")
                fields.append(Node("field", key, key.text, [self.expression()]))
                if not self.accept(";"):
                    break
            return Node("assign", t, "=", [left, Node("object", t, children=fields)])
        if annotation:
            raise SlaiError(t, "Uma anotação de tipo precisa de atribuição.")
        return Node("expr", t, children=[left])

    @staticmethod
    def string(t):
        try:
            value = ast.literal_eval(t.text)
            if "\0" in value or any(0xD800 <= ord(c) <= 0xDFFF for c in value):
                raise ValueError()
            return value
        except (ValueError, SyntaxError):
            raise SlaiError(t, "String inválida (NUL e surrogates não são suportados).", "ELEX") from None

    def expression(self, minimum=0, postfix=True):
        t = self.take()
        if t.kind == "INT":
            node = Node("int", t, int(t.text))
        elif t.kind == "STRING":
            node = Node("string", t, self.string(t))
        elif t.text in ("true", "false"):
            node = Node("bool", t, t.text == "true")
        elif t.text == "await":
            node = Node("await", t, children=[self.expression(7)])
        elif t.text in ("-", "+", "!", "not"):
            node = Node("unary", t, t.text, [self.expression(7)])
        elif t.kind == "NAME":
            node = Node("name", t, t.text)
        elif t.kind == "$":
            value = self.expect("NAME")
            node = Node("string", value, value.text)
        elif t.kind == "(":
            node = self.expression()
            self.expect(")")
        elif t.kind == "[":
            values = []
            if not self.at("]"):
                while True:
                    values.append(self.expression())
                    if not self.accept(","):
                        break
                    if self.at("]"):
                        break
            self.expect("]")
            node = Node("list", t, children=values)
        else:
            raise SlaiError(t, f"Expressão esperada; encontrado {t.text or t.kind!r}.", "EPARSE")
        while True:
            if postfix and self.accept("?"):
                node = Node("propagate", t, children=[node])
            elif postfix and self.accept("."):
                field = self.expect("NAME")
                node = Node("member", field, field.text, [node])
            elif postfix and self.accept("$"):
                node = Node("index", t, children=[node, self.expression(7, postfix=False)])
            elif postfix and self.accept("("):
                args = []
                if not self.at(")"):
                    while True:
                        args.append(self.expression())
                        if not self.accept(","):
                            break
                self.expect(")")
                node = Node("call", t, children=[node] + args)
            elif self.current.text in self.precedence and self.precedence[self.current.text] >= minimum:
                op = self.take()
                right = self.expression(self.precedence[op.text] + 1)
                node = Node("binary", op, op.text, [node, right])
            else:
                break
        return node


def parse(source: str, file: str = "<source>") -> list[Node]:
    return Parser(lex(source, file)).program()
