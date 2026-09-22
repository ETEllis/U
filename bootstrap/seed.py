#!/usr/bin/env python3
"""One-time independent U-to-C seed. Python is not a runtime dependency.

The seed imports only the historical source parser. It emits direct lexical
closures, ordered value construction and ordinary C calls; never an AST walker.
The authoritative compiler is compiler/compiler.u, rebuilt by its own output.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'bootstrap'))
from reference_source import parse, semantic_ast  # noqa: E402


def quote(text: str) -> str:
    """Fixed-width octal escapes preserve every UTF-8 byte, including NUL."""
    return '"' + ''.join(f'\\{byte:03o}' for byte in text.encode('utf-8')) + '"'


class Seed:
    def __init__(self):
        self.counter = 0
        self.functions: list[str | None] = []

    def symbol(self, prefix='v'):
        value = f'{prefix}{self.counter}'
        self.counter += 1
        return value

    def assign(self, code, lines):
        value = self.symbol()
        lines.append(f'V {value}={code};')
        return value

    def descriptor(self, params, body):
        return json.dumps(semantic_ast(dict(params=params, body=body)), ensure_ascii=False,
                          separators=(',', ':'), allow_nan=False)

    def function(self, params, body):
        index = len(self.functions)
        self.functions.append(None)
        lines = ['E scope=u_env(env);', '(void)argc;', '(void)argv;']
        for i, parameter in enumerate(params):
            lines.append(f'u_bind(scope,{quote(parameter["name"])},argv[{i}]);')
        result = self.expression(body, 'scope', lines)
        lines.append(f'return {result};')
        self.functions[index] = f'static V f{index}(E env,size_t argc,V*argv){{\n' + '\n'.join(lines) + '\n}\n'
        return index

    def closure(self, index, scope, params, body):
        descriptor = self.descriptor(params, body)
        return (f'u_annotate(u_closure(f{index},{scope},{len(params)}),'
                f'u_descriptor({quote(descriptor)},{len(descriptor.encode("utf-8"))}))')

    def expression(self, node, scope, lines):
        kind = node['kind']
        if kind == 'literal':
            value = node['value']
            if isinstance(value, bool):
                code = f'u_bool({int(value)})'
            elif isinstance(value, int):
                code = f'u_int({quote(str(value))})'
            elif isinstance(value, float):
                code = f'u_real({quote(repr(value))})'
            elif isinstance(value, str):
                code = f'u_literal({quote(value)},{len(value.encode("utf-8"))})'
            else:
                raise ValueError(f'unsupported literal {value!r}')
        elif kind == 'name':
            code = f'u_lookup({scope},{quote(node["name"])})'
        elif kind == 'member':
            obj = self.expression(node['object'], scope, lines)
            code = f'u_member({obj},{quote(node["name"])})'
        elif kind == 'call':
            callee = self.expression(node['callee'], scope, lines)
            arguments = [self.expression(arg, scope, lines) for arg in node['args']]
            items = '(V[]){' + ','.join(arguments) + '}' if arguments else 'NULL'
            code = f'u_call({callee},{len(arguments)},{items})'
        elif kind in ('list', 'tuple'):
            values = [self.expression(item, scope, lines) for item in node['items']]
            items = '(V[]){' + ','.join(values) + '}' if values else 'NULL'
            code = 'u_unit()' if kind == 'tuple' and not values else f'u_{kind}({len(values)},{items})'
        elif kind == 'record':
            names = list(node['fields'])
            values = [self.expression(node['fields'][name], scope, lines) for name in names]
            keys = '(const char*[]){' + ','.join(quote(name) for name in names) + '}' if names else 'NULL'
            items = '(V[]){' + ','.join(values) + '}' if values else 'NULL'
            code = f'u_record({len(names)},{keys},{items})'
        elif kind == 'lambda':
            index = self.function(node['params'], node['body'])
            code = self.closure(index, scope, node['params'], node['body'])
        elif kind == 'block':
            block_scope = self.symbol('s')
            lines.append(f'E {block_scope}=u_env({scope});')
            for statement in node['statements']:
                result = self.expression(statement['value'], block_scope, lines)
                if statement['kind'] == 'let':
                    next_scope = self.symbol('s')
                    lines.append(f'E {next_scope}=u_env({block_scope});')
                    block_scope = next_scope
                    lines.append(f'u_bind({block_scope},{quote(statement["name"])},{result});')
                else:
                    lines.append(f'(void){result};')
            return self.expression(node['result'], block_scope, lines)
        else:
            raise ValueError(f'unsupported seed expression: {kind}')
        return self.assign(code, lines)

    def emit(self, module, entry='main'):
        lines = ['u_runtime_init(argc,argv);', 'E root=u_env(NULL);']
        for definition in module['definitions']:
            if definition['is_function']:
                index = self.function(definition['params'], definition['body'])
                closure = self.closure(index, 'root', definition['params'], definition['body'])
                lines.append(f'u_bind(root,{quote(definition["name"])},{closure});')
        for definition in module['definitions']:
            if not definition['is_function']:
                index = self.function([], definition['body'])
                initializer = self.closure(index, 'root', [], definition['body'])
                lines.append(f'u_bind_lazy(root,{quote(definition["name"])},{initializer});')
        lines.append(f'return u_finish(u_entry(root,{quote(entry)}));')
        declarations = ''.join(f'static V f{i}(E,size_t,V*);\n' for i in range(len(self.functions)))
        return ('#include "runtime.h"\n' + declarations + ''.join(self.functions)
                + 'int main(int argc,char**argv){\n' + '\n'.join(lines) + '\n}\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('entry_positional', nargs='?')
    parser.add_argument('--entry', default='main')
    options = parser.parse_args()
    module = parse(options.input.read_text(encoding='utf-8'))
    output = Seed().emit(module, options.entry_positional or options.entry)
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(output, encoding='utf-8')


if __name__ == '__main__':
    main()
