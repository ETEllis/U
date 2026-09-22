# Native implementation interface and ownership

Current authority: Edward requires the U implementation itself to live in U, a reproducible self-build, the smallest justified native bridge, and public-facing sites/readmes/papers for U and BiDi. Three verification passes are required. This document is implementation coordination, not public product copy.

## Compiler protocol

The self-hosted source is `compiler/compiler.u`, using the established U surface with explicit header, definitions, typed lambdas, calls, records, lists and blocks. It owns tokenization, parsing, validation, C lowering and driver command dispatch. It must not call a host parser or embed source in a host-language interpreter. `bootstrap/seed.py` may translate parsed U AST into C once, using the preserved reference parser. The ordinary build must subsequently use the compiled U compiler. Generated C is a machine realization, not handwritten language semantics.

Command protocol for the compiled compiler: `uc emit INPUT.u OUTPUT.c [ENTRY]`, `uc check INPUT.u`, `uc run INPUT.u [ENTRY] [JSON_ARGS]`, `uc build INPUT.u OUTPUT [ENTRY]`, `uc fmt INPUT.u`. The compiler reads `U_ROOT` or resolves its own executable root for `native/runtime.h` and library locations. C compilation invokes `cc` with argv, not shell interpolation. Root may provide a wrapper for build/run initially, but the source-to-C transform and subsequent self-compilation must be U-owned.

Runtime ABI agreed between seed and U compiler: see `native/ABI.md` written by native worker. Both compilers must emit byte-identical C for the compiler source. Compiler-generated C functions represent U lexical closures directly and call registered low-level operations; no interpreted U AST/source payload.

## Primitive surface required by U compiler and U libraries

Use exact named calls for the following. Publish availability and any additions through `native/ABI.md` and communicate across workers immediately.

- Integer/natural arithmetic/comparison, bool operations, text concat/equality and numeric conversion.
- `text.length`, `text.at`, `text.slice`, `text.from_code`, `text.code`, `text.join`, `text.parse_int`, `text.parse_real`. Compiler lexical byte processing may use explicit `bytes` primitives if text indices otherwise count codepoints.
- `list.length`, `list.get`, `list.cons`, `list.head_or`, `list.tail_or_empty`, `list.reverse`, `list.append`, `list.map`, `list.filter`, `list.fold_left`; structural `nat.rec`.
- `record.get`, `record.has`, `record.keys`; tuples and field projection. Records/lists are classical values; physical or sealed evidence carriers are not freely reflected.
- `cell.new`, `cell.get`, `cell.set` for explicit managed compiler state; `buffer.new`, `buffer.push`, `buffer.get`, `buffer.set`, `buffer.length`, `buffer.freeze` for efficient compiler construction.
- `value.select(condition, yes, no)` selects already-evaluated arguments, so conditional U code can pass closures and invoke the selected one. `value.kind`, `value.equal` for inspecting compiler data.
- `core.fix_partial` supports explicit tail continuation; `partial.done`, `partial.value`; do not discard non-tail continuation contexts.
- `sys.args()`, `sys.read(path)`, `sys.write(path,text)`, `sys.print(text)`, `sys.exit(code)`, `sys.exec(argv)` with actual argv, `sys.getenv(name)`. Source compilation/formatting itself is inert except explicitly selected driver IO. Ordinary program IO remains capability-qualified.
- `json.parse`, `json.encode` may initially be low-level codecs but must be reported as host semantic debt and migrated to U if feasible.
- `math.sin/cos/exp/log/sqrt/fma/abs`, float32 conversion and explicit seeded random numeric primitive for domain library realization; no host Bayesian/quantum/CDC algorithms.

## Library linkage

Root owns U source modules under `stdlib/` and `cdc/` and their integration tests. Compiler supports explicit input concatenation after duplicate version/import headers are handled, or a manifest-driven list of modules. Preserve the canonical 22 original sources unchanged; any amended executable examples carry a documented revision. Names in compiled user source resolve to U library definitions before native primitives. Native missing operation must fail explicitly, never fabricate values.

## Ownership

- native-bootstrap worker: `native/*`, `bootstrap/seed.py`, `bootstrap/build.sh`, `tests/native_bootstrap/*`. Supplies ABI promptly; no other files.
- self-compiler worker: `compiler/*`, `tests/selfhost/*`, `docs/SELF_HOSTING.md`. Coordinates ABI with native-bootstrap worker; no other files.
- sites-voice worker: U `landing/*`, `README.md`, `paper/*`; BiDi `landing/*`, `README.md`, `paper/arxiv/*`; can add presentation-only artifacts under each `assets/`. Read and use Edward voice required references. No semantic runtime edits, commits, or remote mutations.
- root: U libraries, source migration, launchers, schemas/evidence/tests/CI, native registry integration by coordination, hosting/publication and Git commits. Other work occurs concurrently: preserve others' edits.
