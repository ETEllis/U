# Source, elaboration, and checking

This is the newly implemented U stage-0 frontend. The preserved dossier and the final reconciliation are its design inputs. The missing historical grammar and checker have not been relabeled as recovered assets. `spec/grammar.ebnf` describes the single parser used by all 22 preserved programs.

## Source contract

`u.source.scan` returns frozen tokens with exact lexemes, Unicode code-point offsets, and one-based line/column coordinates. Whitespace and comments remain available, and concatenating token text reconstructs the exact source. `parse` returns the shared dictionary AST defined in `INTEGRATION.md`, including source spans and the untouched input. Function definitions carry `is_function` so a zero-argument function remains distinguishable from a value definition.

The scanner checks strings, escapes, non-finite literals, unknown characters, and resource limits. The recursive-descent parser checks delimiters, separators, required result expressions, duplicate declarations/bindings/fields, and version headers. Definition names resolve simultaneously; parameters and block bindings resolve lexically. A let initializer sees the preceding scope. Inner scopes can shadow outer names. The header token `u` remains usable as the SQL example's ordinary parameter name.

The formatter operates on tokens, preserves comment and string lexemes, and reparses its output. It compares ASTs after removing only source coordinates and input text. Formatting is deterministic and idempotent. Formatting does not execute a definition, annotation, package import, or effect.

Limits are deliberate refusal boundaries: two million source characters, 100,000 tokens, 128 nested expressions/postfix operations, and 4096 integer digits. Checking and graph decoding also have finite work limits. An over-budget input produces an error; no termination theorem for unrestricted programs is implied.

## Six-tag graph

`u.graph.KernelNode` is an immutable recursive value with immutable attributes and ordered regions. `elaborate` returns a detached JSON-friendly serialization plus dependencies and obligations. Only `wire`, `gen`, `seq`, `par`, `scope`, and `fix` are kernel node tags. The JSON dictionaries are serialization snapshots; mutating a snapshot does not mutate the underlying kernel value.

| Surface form | Elaboration rule |
| --- | --- |
| Literal | `gen(value.literal)` with tagged integer, exact binary64 bits, text, or Boolean payload |
| Bound name | `wire` naming an explicit lexical binder, with a one-port forwarding permutation |
| Definition and parameter | `scope` enclosing ordered annotation/body regions; module exports retain public names |
| Call | Named operator `gen`, or `gen(value.apply)` with callee followed by ordered arguments |
| Lambda | `gen(value.lambda)` enclosing fresh parameter scopes |
| Record, tuple, list | Named value generators retaining ordered fields/elements |
| Let and statement sequence | `seq` with an explicit result binder scope for each let |
| Partial recursion | `fix` qualified by `V.partial`, binding an explicit iteration identity |
| Derived operation | Named `gen` with an immutable reference-rule dependency |

The current surface has no standalone parallel keyword. The kernel admits a `par` representation, but graph execution refuses it without an independence and scheduling realization. No tuple/list/record constructor silently asserts independent execution: its regions retain strict source order.

Binder identities follow deterministic lexical traversal. Local names are diagnostic hints outside structural identity; alpha-renaming local parameters and lets leaves the graph identity unchanged. Exported definition names, field order, call order, literal bits, imported profiles, and operator contracts remain significant. The graph never embeds the original source or an opaque source-language payload.

Repeated `wire` references demand a `Copy` witness; unused local binders demand `Drop`. Structural classical-type rules discharge the elementary numeric, textual, tuple/list, and bit-vector cases. The first graph implementation retains obligations for other binders. QReg and owning-handle reuse is separately rejected by the static resource checker. Forwarding a port is not itself copying permission.

Named derived-rule descriptions are labeled `specified-unverified`. Their content participates in the operator dependency digest, so modifying a join rule changes dependent identities. They are inspectable specification references, not checked expansions, executable callbacks, or proofs. Native execution is supplied by the independently visible runtime operation rules. A metadata hash does not establish those rules' correctness.

## Profiles and identities

Every definition carries an explicit composition result from `u.admission`, including families, profile identity, adapters, assumptions, and remaining obligations. Local binders shadow operator namespaces, and global call dependencies propagate required families. Probability/search combinations require a named observation-order profile. Mixed-theory names alone do not manufacture an admitted model.

CDC graph profiles additionally bind `u.cdc.NUMERIC_PROFILE`, including the observed binary64 contraction behavior. This preserves the pre-existing CDC profile rather than silently imposing a different arithmetic law. Semantic identity includes graph structure, elaboration version, operator dependencies, composition contracts, and resource requirements. Runtime receipts are not part of that identity. A backend change can retain identity only while these semantic contracts remain fixed.

The structural digest establishes canonical structural identity, never general semantic equivalence. `semantic_digest` is explicitly the identity of a semantic candidate with its declared dependencies; `elaborate` always returns `checked: false`.

## Conservative type checking

`u.checker.check` reports `checked`, `rejected`, or `unsupported`, with per-definition results. Its `typechecked` and `ok` fields are true only for the checked fragment. `Unresolved` is an explicit failed-to-establish type with retained obligations; it never acts as a universal accepting type.

The current checker handles ordinary annotated function types, records/sums, homogeneous lists, pairs, basic numeric and Boolean operations, higher-order list combinators, structural Nat recursion, explicit partial recursion, contextual integer bounds, indexed carrier-kind checks, and a conservative lexical resource discipline. Function annotations accepting a linear argument do not themselves make the reusable function linear. Captured linear values require a separate one-shot-closure derivation and are rejected by this checker. Cached module-level linear resources are rejected. Recursive global call cycles cannot enter the checked total fragment merely by declaring a return type.

Sequential blocks preserve a declared effect boundary. A block that prints and then returns an integer cannot claim a pure `Int` result; it requires `Effect(IO, Int)`. Nested effectful containers and interactions without a complete sequencing rule retain obligations.

Quantum gates consume and return one joint register handle. Duplicate aliases, discarded handles, invalid fixed indices, equal control/target indices, and closure capture of a QReg are rejected. Mutable buffer regions must return their view to the region operator; leaked views and closure/record escapes are rejected. Broader borrowing, branching, polymorphism, dependent conversion, and resource proofs remain named obligations where the implemented rules do not suffice.

For the preserved source set, examples 1–5, 17, and 22 pass the current scoped static checker. The other examples parse and elaborate, and their available boundary checks run, but the result stays `unsupported` because complete domain derivations are absent. This says nothing by itself about their runtime capability: a supported simulator or finite checker can execute a specific domain realization without establishing the language's full static metatheory. Proof terms are routed to the independent total proof kernel; a generic function return annotation cannot create a proof.

## Executable graph path and checks

`u.graph_execution.reconstruct` recovers the shared AST from graph constructors and lexical references. It generates fresh local names, reconstructs types from their graph regions, and re-elaborates the result. The graph, rule digests, profile contracts, stages, port order, resource requirements, structural identity, and semantic identity must match exactly. Dangling/repeated binder identities, forged contracts, unknown metadata, and unsupported kernel execution forms are refused. No original source payload is consulted.

`run_graph` then rejects failed static checks or unadmitted compositions and executes the reconstructed operations through the direct U evaluator. Domain obligations remain visible in the artifact; execution does not upgrade them into proofs. This creates an actual source → kernel → operations execution path, with the direct source evaluator retained as a differential reference.

`tests/test_frontend.py` covers the 22 original parses, lossless scanning, comment preservation, idempotent formatting, malformed and seeded adversarial inputs, bounded refusal, exact lexical scopes, alpha identity, rule invalidation, Copy obligations, quantum/lifetime failures, uncertainty propagation, profile ordering, full graph reconstruction, tamper rejection, and actual arithmetic/Fibonacci/list execution from graph data.
