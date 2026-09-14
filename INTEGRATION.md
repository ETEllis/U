# Implementation interface — stage 0

All modules use Python 3.13+ standard library unless explicitly pinned. U programs are parsed and interpreted directly; never call Python eval or exec on program text. The active checkout is /Users/edwardellis/Developer/U; the new BiDi oracle checkout is its sibling.

## Source interface (frontend owner)

`u.source.parse(text: str) -> dict` returns `{version, imports: [str], definitions: [{name, params: [{name, type}], type, body}], source: str}`.
Expressions are dictionaries with `kind`: `literal` (value, optional literal_type), `name` (name), `call` (callee, args), `member` (object, name), `lambda` (params, body), `list` (items), `tuple` (items), `record` (fields mapping), `block` (statements of `{kind:'let',name,value}` or `{kind:'expr',value}`, result). All syntax has source spans when useful. Type annotations use the same expression grammar.

`u.source.format_source(text)` produces a semantics-preserving deterministic format. Preserve exact original bytes separately. `u.graph.elaborate(module)` returns the six-tag graph plus deterministic bindings, profile/adaptor evidence and explicit unsupported obligations. `u.checker.check(module)` returns JSON-friendly status/diagnostics; do not certify unchecked domain types. Root may supply canonical functions from `u.evidence`.

## Evaluation interface (runtime owner)

`u.evaluator.Evaluator(module, *, capabilities=(), budget=100000)`; `.run(entry, args=[])` returns a Python value. `.evaluate(expr, env)` evaluates an expression; `.invoke(callable_value, args)` applies a closure/native operation. `.register(name, callable)` registers root-owned CDC/proof operators. Builtin functions receive ordinary values; lazy region/staging primitives use explicit evaluator hooks. `Evaluator` imports `u.cdc.register(evaluator)` and `u.proof.register(evaluator)` when those modules become available. No ambient IO from parsing/checking. The console capability is explicitly granted at runtime. Errors are typed exceptions or outcome dictionaries and must retain distinctions.

## Ownership

- Frontend worker: u/source.py, u/graph.py, u/checker.py, tests/test_frontend.py, docs/ELABORATION.md, spec/grammar.ebnf.
- Runtime worker: u/evaluator.py, u/theories/*, tests/test_runtime.py, docs/RUNTIME.md. Values are defined here; tell root their representation.
- Publication worker: README.md, landing/*, paper/*, CITATION.cff, docs/ARCHITECTURE.md. No invented implementation counts; root supplies final evidence.
- Root: all other files, CDC, proof, evidence/resource/admission core, CLI/LSP, package manager, native/WASM fragments, verification, CI, preservation, capability ledger, commits and remote.
