# U implementation specification 0.1

This specification records implemented stage-0 contracts. The preserved dossier and reconciled review retain the full target architecture. Python 3.13 is the explicit trusted reference bootstrap; no self-hosting or general compiled-runtime claim follows from it.

## Source, elaboration and identity

The shared versioned grammar is in `spec/grammar.ebnf`. The scanner preserves exact lexemes and original source bytes; parsed source is inert. Definitions, calls, lexical bindings, typed regions, records, lists and tuples elaborate deterministically into six structural tags. `u/graph_execution.py` reconstructs executable structure from the graph itself, re-elaborates it and validates its contracts before reference execution. No source string is hidden inside a generator to implement another language.

Canonical identity uses UTF-8 JSON with unambiguous typed envelopes: exact integers use `$int`, IEEE64 values exact big-endian bits, byte payloads hex, dictionaries an explicit `$map` list of exact-key/value pairs, and tuples `$tuple`. Map ordering is Unicode code-point ordering. A marker-shaped user dictionary therefore cannot impersonate a numeric payload. Lists and tuples remain distinct. Hash inputs include a domain separator and encoding version. Graph structural identity is not general semantic equivalence.

Meaning, realization and evidence have separate dependency directions. The semantic candidate identity includes graph, resolved profile contracts, operator rule dependencies and explicit resource requirements. Every backend change must preserve its complete numeric and observation contract to retain meaning identity. Registrations and prose rule references are not checked expansion proofs. The machine capability ledger binds implemented reference rules to their current source bytes and keeps support, assumptions and proof status distinct.

## Composition and resources

The first composite profile rule admits values with one domain cluster under declared reference contracts, and the pinned CDC composite. Probability/search composition requires a supported observation order; contradictory or unknown orders fail. Other theory interactions require new explicit admission rules. This finite admitted set is not a universal composition theorem.

Reference memory uses fresh allocation identity and epochs, initialized-byte checks, consuming owner handles and scoped exclusive borrows. Whole-owner access suspends during a borrow; returned views expire. Independence requires validated footprints, consistent epochs, disjoint ownership or explicitly shared reads, and no cross-branch causal dependence. A read/write overlap is rejected. The separately implemented runtime buffer interface tracks its linear lifecycle for `.u` examples; the lower byte-memory model has its own tests and is not claimed to be a complete C/Rust memory model.

Authority leases are process-local issuer objects, scoped by action, frame, epoch, finite expiry and single-use nonce. They are distinct from memory borrows. Distributed cryptographic authority and hostile-host isolation remain separate release gates. Host Python code is in the trusted computing base; U source has no host reflection, `eval`, arbitrary import, or access to certificate constructors.

## Proof

The independent total checker supports stratified sorts, Nat, dependent Pi/lambda/application, Eq, reflexivity, congruence and Nat induction. Addition recurses on its second argument. It validates candidate syntax without the ordinary evaluator. The named bootstrap kernel rejects shadowing of an existing dependent context binder; this prevents context retargeting and can later be relaxed through a proved hygienic internal representation. General partial recursion, IO and solvers have no reduction rules in this checker. Other inductive families, Sigma, general well-founded recursion, quotient types and full dependent elaboration remain extension obligations.

Proof receipts are minted only after checking the inferred judgment and are protected against copying as ordinary U dictionaries. `prove` requires a receipt issued by that checker; a source record with a convincing tag cannot substitute for it. Resource exhaustion refuses proof construction; it is not a mathematical divergence verdict.

## Execution and backends

The reference evaluator executes explicit operations with lexical environments and fuel. Total structural Nat recursion and explicitly tail-recursive partial fix have distinct rules. See `docs/RUNTIME.md` for the exact relational, probabilistic, process, clock, GPU, quantum and DAE contracts. Simulator results preserve their realization labels.

The native/WASM compiler currently admits annotated exact Int/Nat expression functions over explicitly guarded input intervals. Interval arithmetic verifies every intermediate fits signed i64; C and WASM emit runtime input checks. Unknown operations, shadowed arithmetic namespaces, wrong imports, or failed interval proofs stop compilation. This is a bounded-input realization of exact integers. Other graph fragments remain reference-evaluated.

## CDC and numerical analysis

The pinned compatibility source remains BiDi SHA `1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157`. Native first-wins attributes and registry last-wins views are retained separately. The implemented `.cdc` import covers finite well-formed field/module/cell/channel declarations and ordered flow/commit/nest. Other directives preserve source and return explicit unsupported obligations.

On the verified Apple arm64 Clang profile, native C contracts the final multiplication and addition in phase and belief updates. U uses explicit `math.fma` at those same points, with source-ordered channel accumulation and the platform libm. The numeric profile records this; generic separate multiply/add did not earn bit parity. Derived Jacobians concern the real extension of the executed finite map and fixed itinerary, not differentiation of IEEE rounding.

HOLD protects the current latch update; earlier flow is retained and later native primitive steps proceed. Post-step expectation faults retain known progress. Path tangents are source-bound computed artifacts. Absolute recurrence checks the full declared continuous and discrete manifest. Tolerance, norm, residual and numerical qualification survive into the recurrence certificate and return map. A changed tangent invalidates its certificate. General relative symmetry admission and validated real-Schur realization remain explicit holds. Full U1/U2 source ingestion and all original mutant families remain open compatibility gates.

## Persistence, packages and tooling

The local journal writes hash-linked records under advisory file locking and fsync, detects corruption and refuses to append over a torn tail. The default lock manager snapshots `.u` members and U.toml; installation copies only verified locked members and never runs package scripts. Network dependency resolution is not yet implemented. Exact source export validates both retained bytes and unchanged graph identity; transformed export needs a checked residual update.

CLI commands expose check/run/eval/prove/fmt/explain/audit/lift/export/build/test/lsp and package lock/install/verify. Unsupported static domain checks are distinct from explicitly requested reference evaluation. LSP parsing, diagnostics, hover, definitions and formatting operate on document text without executing programs. Receipt verification checks binding and identity, not truth or signer authenticity.
