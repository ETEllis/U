"""Lossless, bounded scanner and one shared parser for the U 0.1 surface.

Parsing is inert: it does not import U packages or execute annotations. Offsets
index Python Unicode code points; line and column coordinates are one-based.
"""
from __future__ import annotations

from dataclasses import dataclass
import json
import math

MAX_SOURCE = 2_000_000
MAX_TOKENS = 100_000
MAX_DEPTH = 128
MAX_INTEGER_DIGITS = 4096


class SourceError(ValueError):
    def __init__(self, code: str, message: str, span: dict):
        self.code, self.message, self.span = code, message, span
        super().__init__(f"{code} at {span['line']}:{span['column']}: {message}")

    def as_dict(self):
        return {"code": self.code, "message": self.message, "span": self.span}


@dataclass(frozen=True)
class Token:
    kind: str
    text: str
    start: int
    end: int
    line: int
    column: int

    @property
    def span(self):
        return dict(start=self.start, end=self.end, line=self.line, column=self.column)


def scan(source: str, *, include_trivia: bool = True) -> tuple[Token, ...]:
    """Return exact lexemes, including whitespace/comments for lossless clients."""
    if not isinstance(source, str):
        raise TypeError("U source must be text")
    if len(source) > MAX_SOURCE:
        raise SourceError("source_limit", "source exceeds the configured character limit", dict(start=0, end=0, line=1, column=1))
    tokens = []
    i, line, column = 0, 1, 1
    while i < len(source):
        start, ln, col = i, line, column
        c = source[i]
        if c.isspace():
            i += 1
            while i < len(source) and source[i].isspace():
                i += 1
            kind = "whitespace"
        elif source.startswith("//", i):
            i = source.find("\n", i)
            if i == -1:
                i = len(source)
            kind = "comment"
        elif source.startswith("/*", i):
            end = source.find("*/", i + 2)
            if end == -1:
                raise SourceError("unterminated_comment", "expected */", dict(start=start, end=len(source), line=ln, column=col))
            i, kind = end + 2, "comment"
        elif c == '"':
            i += 1
            while i < len(source):
                if source[i] == '"':
                    i += 1
                    break
                if source[i] == "\\":
                    i += 2
                elif ord(source[i]) < 32:
                    raise SourceError("invalid_string", "raw control character in string", dict(start=start, end=i + 1, line=ln, column=col))
                else:
                    i += 1
            else:
                raise SourceError("unterminated_string", "expected closing quote", dict(start=start, end=len(source), line=ln, column=col))
            try:
                value = json.loads(source[start:i])
                if any(0xD800 <= ord(ch) <= 0xDFFF for ch in value):
                    raise ValueError("unpaired Unicode surrogate")
            except (ValueError, json.JSONDecodeError) as exc:
                raise SourceError("invalid_string", str(exc), dict(start=start, end=i, line=ln, column=col)) from None
            kind = "string"
        elif c.isascii() and (c.isalpha() or c == "_"):
            i += 1
            while i < len(source) and source[i].isascii() and (source[i].isalnum() or source[i] == "_"):
                i += 1
            kind = "identifier"
        elif c.isascii() and c.isdigit():
            i += 1
            while i < len(source) and source[i].isascii() and source[i].isdigit():
                i += 1
            kind = "integer"
            if i < len(source) and source[i] == "." and i + 1 < len(source) and source[i + 1].isdigit():
                kind, i = "float", i + 1
                while i < len(source) and source[i].isascii() and source[i].isdigit():
                    i += 1
            if i < len(source) and source[i] in "eE":
                kind, i = "float", i + 1
                if i < len(source) and source[i] in "+-":
                    i += 1
                exp = i
                while i < len(source) and source[i].isascii() and source[i].isdigit():
                    i += 1
                if i == exp:
                    raise SourceError("invalid_number", "exponent requires digits", dict(start=start, end=i, line=ln, column=col))
            if i < len(source) and (source[i].isalpha() or source[i] == "_"):
                raise SourceError("invalid_number", "number and identifier need a separator", dict(start=start, end=i + 1, line=ln, column=col))
            if kind == "integer" and i - start > MAX_INTEGER_DIGITS:
                raise SourceError("number_limit", "integer literal exceeds digit limit", dict(start=start, end=i, line=ln, column=col))
        elif source[i:i + 2] in ("->", "=>", "#{"):
            i += 2
            kind = source[start:i]
        elif c in "()[]{},;:.=-":
            i += 1
            kind = c
        else:
            raise SourceError("invalid_character", f"unexpected character {c!r}", dict(start=start, end=start + 1, line=ln, column=col))
        lexeme = source[start:i]
        if include_trivia or kind not in ("whitespace", "comment"):
            tokens.append(Token(kind, lexeme, start, i, ln, col))
            if len(tokens) > MAX_TOKENS:
                raise SourceError("token_limit", "token budget exceeded", tokens[-1].span)
        newlines = lexeme.count("\n")
        if newlines:
            line += newlines
            column = len(lexeme.rsplit("\n", 1)[1]) + 1
        else:
            column += len(lexeme)
    tokens.append(Token("eof", "", i, i, line, column))
    return tuple(tokens)


def qualified_name(expr: dict) -> str | None:
    if expr.get("kind") == "name":
        return expr["name"]
    if expr.get("kind") == "member":
        parent = qualified_name(expr["object"])
        return f"{parent}.{expr['name']}" if parent else None
    return None


class Parser:
    def __init__(self, source: str):
        self.source = source
        self.tokens = scan(source, include_trivia=False)
        self.i = 0
        self.depth = 0

    @property
    def token(self):
        return self.tokens[self.i]

    def take(self, kind=None, text=None):
        token = self.token
        if kind is not None and token.kind != kind or text is not None and token.text != text:
            raise SourceError("expected_token", f"expected {text or kind}, found {token.text or 'end of source'}", token.span)
        if token.kind == "eof":
            raise SourceError("unexpected_end", "unexpected end of source", token.span)
        self.i += 1
        return token

    def accept(self, kind):
        if self.token.kind == kind:
            return self.take()
        return None

    def node(self, kind, start, **fields):
        return {"kind": kind, **fields, "span": {**start.span, "end": self.tokens[self.i - 1].end}}

    def module(self):
        self.take("identifier", "u")
        version = json.loads(self.take("string").text)
        if version != "etellis.u/0.1":
            raise SourceError("unsupported_version", f"unsupported surface version {version}", self.tokens[1].span)
        self.take(";")
        imports, definitions, names = [], [], set()
        while self.token.kind != "eof":
            start = self.take("identifier")
            if start.text == "use":
                if definitions:
                    raise SourceError("import_order", "imports precede definitions", start.span)
                profile = json.loads(self.take("string").text)
                if profile in imports:
                    raise SourceError("duplicate_import", f"duplicate import {profile}", start.span)
                imports.append(profile)
                self.take(";")
                continue
            if start.text != "def":
                raise SourceError("expected_definition", "expected use or def", start.span)
            name = self.take("identifier").text
            if name in names:
                raise SourceError("duplicate_definition", f"duplicate definition {name}", start.span)
            names.add(name)
            is_function = self.token.kind == "("
            if is_function:
                params = self.params()
                self.take("->")
            else:
                params = []
                self.take(":")
            annotation = self.expr()
            self.take("=")
            body = self.expr()
            self.take(";")
            definitions.append(dict(name=name, params=params, type=annotation, body=body, is_function=is_function,
                                    span={**start.span, "end": self.tokens[self.i - 1].end}))
        return dict(version=version, imports=imports, definitions=definitions, source=self.source)

    def params(self):
        self.take("(")
        params, names = [], set()
        while self.token.kind != ")":
            token = self.take("identifier")
            if token.text in names:
                raise SourceError("duplicate_parameter", f"duplicate parameter {token.text}", token.span)
            names.add(token.text)
            self.take(":")
            params.append(dict(name=token.text, type=self.expr(), span=token.span))
            if not self.accept(","):
                break
        self.take(")")
        return params

    def expr(self):
        self.depth += 1
        if self.depth > MAX_DEPTH:
            raise SourceError("nesting_limit", "expression nesting limit exceeded", self.token.span)
        try:
            start = self.token
            expression = self.atom()
            postfix_count = 0
            while self.token.kind in ("(", "."):
                postfix_count += 1
                if postfix_count > MAX_DEPTH:
                    raise SourceError("nesting_limit", "postfix chain nesting limit exceeded", self.token.span)
                if self.accept("."):
                    name = self.take("identifier").text
                    expression = self.node("member", start, object=expression, name=name)
                else:
                    self.take("(")
                    args = self.sequence(")")
                    expression = self.node("call", start, callee=expression, args=args)
            return expression
        finally:
            self.depth -= 1

    def sequence(self, end):
        items = []
        while self.token.kind != end:
            items.append(self.expr())
            if not self.accept(","):
                break
        self.take(end)
        return items

    def atom(self):
        token = self.token
        if token.kind == "identifier":
            self.take()
            if token.text == "fn":
                params = self.params()
                self.take("=>")
                return self.node("lambda", token, params=params, body=self.expr())
            if token.text in ("true", "false"):
                return self.node("literal", token, value=token.text == "true", literal_type="Bool")
            if token.text in ("use", "def", "let", "yield"):
                raise SourceError("reserved_word", f"{token.text} is not an expression", token.span)
            return self.node("name", token, name=token.text)
        if token.kind in ("integer", "float", "string", "-"):
            negative = bool(self.accept("-"))
            number = self.take()
            if negative and number.kind not in ("integer", "float"):
                raise SourceError("invalid_number", "minus requires a numeric literal", number.span)
            if number.kind == "string":
                value, typ = json.loads(number.text), "Text"
            elif number.kind == "integer":
                value, typ = int(number.text), "Int"
            elif number.kind == "float":
                value, typ = float(number.text), "F64"
                if not math.isfinite(value):
                    raise SourceError("invalid_number", "non-finite literal", number.span)
            else:
                raise SourceError("invalid_literal", "expected literal", number.span)
            if negative:
                value = -value
            return self.node("literal", token, value=value, literal_type=typ)
        if self.accept("["):
            return self.node("list", token, items=self.sequence("]"))
        if self.accept("("):
            if self.accept(")"):
                return self.node("tuple", token, items=[])
            first = self.expr()
            if self.accept(","):
                return self.node("tuple", token, items=[first, *self.sequence(")")])
            self.take(")")
            return first
        if self.accept("#{"):
            fields = {}
            while self.token.kind != "}":
                name = self.take("identifier")
                if name.text in fields:
                    raise SourceError("duplicate_field", f"duplicate field {name.text}", name.span)
                self.take(":")
                fields[name.text] = self.expr()
                if not self.accept(","):
                    break
            self.take("}")
            return self.node("record", token, fields=fields)
        if self.accept("{"):
            statements, names = [], set()
            while not (self.token.kind == "identifier" and self.token.text == "yield"):
                if self.token.kind == "}" or self.token.kind == "eof":
                    raise SourceError("missing_yield", "block requires a terminal yield expression", self.token.span)
                if self.token.kind == "identifier" and self.token.text == "let":
                    start = self.take()
                    name = self.take("identifier").text
                    if name in names:
                        raise SourceError("duplicate_binding", f"duplicate binding {name}", start.span)
                    names.add(name)
                    self.take("=")
                    statements.append(self.node("let", start, name=name, value=self.expr()))
                else:
                    start = self.token
                    statements.append(self.node("expr", start, value=self.expr()))
                self.take(";")
            self.take("identifier", "yield")
            result = self.expr()
            self.take(";")
            self.take("}")
            return self.node("block", token, statements=statements, result=result)
        raise SourceError("expected_expression", f"expected expression, found {token.text or 'end of source'}", token.span)


def parse(text: str) -> dict:
    try:
        return Parser(text).module()
    except RecursionError:
        raise SourceError("nesting_limit", "parser recursion budget exceeded", dict(start=0, end=0, line=1, column=1)) from None


def semantic_ast(value):
    """Remove source coordinates only; useful for lossless formatter validation."""
    if isinstance(value, dict):
        return {k: semantic_ast(v) for k, v in value.items() if k not in ("span", "source")}
    if isinstance(value, list):
        return [semantic_ast(v) for v in value]
    return value


def format_source(text: str) -> str:
    """Normalize token spacing while retaining comment/string lexemes exactly."""
    before = parse(text)
    output, line, indent, previous = [], "", 0, ""

    def flush():
        nonlocal line
        if line.strip():
            output.append("  " * indent + line.strip())
        line = ""

    for token in scan(text):
        kind, lexeme = token.kind, token.text
        if kind in ("whitespace", "eof"):
            continue
        if kind == "comment":
            if lexeme.startswith("//"):
                line += (" " if line else "") + lexeme
                flush()
            else:
                flush()
                output.append("  " * indent + lexeme)
            previous = "comment"
            continue
        if kind == "}":
            flush()
            indent -= 1
            line = "}"
        elif kind in ("{", "#{"):
            line += (" " if line and previous not in ("(", "[", ".") else "") + lexeme
            flush()
            indent += 1
        elif kind == ";":
            line = line.rstrip() + ";"
            flush()
        elif kind == ",":
            line = line.rstrip() + ", "
        elif kind == ":":
            line = line.rstrip() + ": "
        elif kind == ".":
            line = line.rstrip() + "."
        elif kind in (")", "]"):
            line = line.rstrip() + lexeme
        elif kind in ("(", "["):
            line += lexeme
        elif kind in ("=", "->", "=>"):
            line = line.rstrip() + " " + lexeme + " "
        else:
            needs_space = bool(line and not line.endswith((" ", "(", "[", ".", "-")))
            line += (" " if needs_space else "") + lexeme
        previous = kind
    flush()
    formatted = "\n".join(output) + "\n"
    if semantic_ast(before) != semantic_ast(parse(formatted)):
        raise SourceError("formatter_invariant", "formatting changed the parsed program", dict(start=0, end=0, line=1, column=1))
    return formatted
