# U architecture and lineage

U gives operations one explicit structural form while preserving the theories that determine their meaning. The architecture connects meaning, realization and evidence. The compiler checks the structural graph; domain admission and preservation requirements remain explicit at its boundaries. A realization's evidence is scoped to the operation, inputs and contracts it binds.

## Source to operation

The language identifier is `etellis.u`; package, profile, graph-schema, elaboration and compiler versions are distinct. The preserved design uses the header `u "etellis.u/0.1";`. A source header is syntax selection, not approval of arbitrary imported generators.

Readable syntax contains definitions, calls, typed regions, binders, records, lists, tuples, staging and sequencing. Elaboration resolves lexical bindings and immutable dependencies deterministically into `wire`, `gen`, `seq`, `par`, `scope` and `fix`. Qualified operation names, declared interfaces and rule obligations remain available throughout the graph. Unsupported obligations are reported on the graph rather than concealed by a successful parse.

The rich syntax tree remains useful for editor features, diagnostics and exact-source residuals. Its existence does not add a seventh semantic kernel tag. Conversely, assigning a surface construct one of six tags does not itself prove that its binding, typing or evaluation rules are correct.

## Semantic interfaces

An operation's interface closes over carrier types, usage, effects, stages, clocks, observables and semantic-profile identity. A profile chooses the relevant model and laws. Values may be reusable; an owned allocation or joint quantum register may be linear. A relation's answer multiplicity and a behavioral model's scheduling assumptions remain visible.

Sequential composition requires compatible boundaries. Mixed theories require a checked composite profile or adapter. A graph may contain inferred adapter evidence even when no extra source annotation was necessary. The inference must be deterministic under the same manifest, and the emitted evidence must pass the same checks as an explicit annotation.

Parallel composition requires disjoint owned identities or an explicit shared-read allowance. A shared writer, duplicated linear token or unresolved alias prevents that admission. The current graph emits `par` for literal-only groups with a checked empty-resource footprint; broader resource independence still requires its domain checker. Runtime borrowing contracts apply separately. Causal independence, commutation, device synchronization and physical simultaneity have further premises.

## Native realization

The production compiler and domain algorithms are written in U. A checked generated-C seed starts a fresh build; the U compiler then reproduces its own output. Native functions execute closures and library operations directly. The generic C bridge supplies values, allocation, arithmetic and operating-system access, not U syntax interpretation. The independent Python seed is optional development verification.

The implementation interfaces are in [INTEGRATION.md](../INTEGRATION.md). Source, graph, proof, evidence, domain, compiler-target and tool contracts have separate verification boundaries. The historical reference implementation remains in Git history for differential comparison; it is not the default engine.

The first total proof calculus is deliberately smaller than the full V theory proposal. Type annotations or a `Proof` name do not establish a dependent type system. The authoritative support inventory must state the terms checked, normalization rules admitted, assumptions retained and constructor authority. Unbounded program recursion never becomes total conversion.

## Artifact identity

U distinguishes original bytes, normalized syntax, meaning, realization, execution, replay and nominal resource identity. A meaning digest binds a canonical graph and every meaning-relevant profile field, including numeric and observation contracts, assumptions and expansion dependencies. Recursive programs use explicit binding/reference syntax within an acyclic content-addressed object graph.

A backend change can retain meaning identity if the whole semantic contract remains unchanged. Realization identity changes when the selected implementation changes; execution identity also binds input and relevant external decisions. Equal hashes are identity evidence under the digest assumptions, not a decision that arbitrary programs are extensionally equivalent. Hashes neither authenticate a sender nor certify the truth of a receipt.

Evidence is indexed by subject and scope. Artifacts earned under one subject cannot be attached to another graph after a semantic edit. A hold can retain already earned artifacts without producing a stronger artifact. A new invalidation records the dependency mismatch; it does not retroactively change what the historical receipt said about its historical subject.

## The seven theory families

The initial design's V/M/R/P/C/D/Q grouping is an accounting discipline. Each family must publish concrete operators, rules, laws, model assumptions, observables, failure cases and supported realizations. A family's existence does not earn every source language associated with it.

For R, bags preserve multiplicity and NULL requires its own truth contract. For P, normalization requires positive finite mass; sampling is a realization rather than the measure itself. For C, one replay does not cover all schedules or prove fairness. For D, equation objects and numerical approximations retain separate identities and error obligations. For Q, simulator state remains distinct from a physical opaque resource, and coherent joint states remain distinct from classical mixtures.

Native support is assessed for a fragment: explicit operators, compositional semantics, direct rules or checked expansion, analyzability, recoverability and adversarial evidence. Hidden foreign interpreters fail that test even if they return correct answers.

## BiDi crosswalk

The reference checkout is [ETEllis/BiDi](https://github.com/ETEllis/BiDi/tree/1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157). CDC predates U. Its source language and native runtime are independent compatibility authority, not files U may silently rewrite to satisfy a test.

| BiDi source at the pin | What it establishes for this integration | U obligation |
|---|---|---|
| [Universal Operator System](https://github.com/ETEllis/BiDi/blob/1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157/UNIVERSAL_OPERATOR_SYSTEM.md) | One source/state/variation/evidence path; CDC, U1, U2 and instruments have distinct roles | Preserve the whole path and its authority order rather than extracting ternary arithmetic alone |
| [Formal semantic spine](https://github.com/ETEllis/BiDi/blob/1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157/FORMAL_SEMANTIC_SPINE.md) | Rich calculus and narrower native realization remain distinguishable | Lower consumed native fields exactly; classify richer declarations as unsupported or separate profiles |
| [Verification matrix](https://github.com/ETEllis/BiDi/blob/1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157/VERIFICATION_OBLIGATION_MATRIX.md) | Each capability is attached to its witness and open obligation | Compare ordered results, identity and effects; no transfer of historical BiDi counts into fresh U claims |
| [Frameworks](https://github.com/ETEllis/BiDi/blob/1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157/FRAMEWORKS.md) | Task-shaped bindings, exemplars and role contracts are derived over existing machinery | Retain derived framework identity and check bindings rather than inventing universal primitives |
| [Native self-hosting mandate](https://github.com/ETEllis/BiDi/blob/1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157/NATIVE_SELF_HOSTING_MANDATE.md) | Historical host-dependency and removal program; current authority is explicitly elsewhere | Verify U's own compiler stages and platform boundary; do not inherit BiDi's historical milestones |
| [Toolchain plan](https://github.com/ETEllis/BiDi/blob/1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157/CDC_TOOLCHAIN_PLAN.md) | Binding amendments separate ABI, durability, keyed authority, per-check parity and trusted-local packages | Treat similarly named U commands as their own implemented contracts, not parity by naming |
| [U2 semantics](https://github.com/ETEllis/BiDi/blob/1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157/docs/u2/U2_SEMANTICS.md) | Actual finite-map Jacobians, exact itinerary, complete/relative return and gated spectrum | Differentiate the executed maps and preserve canonical recurrence hold |
| [RFTC matrix](https://github.com/ETEllis/BiDi/blob/1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157/docs/rftc/VERIFICATION_OBLIGATION_MATRIX.md) | Bounded classical local evidence; deployment and physical claims have additional gates | No multi-host, quantum, physical-law or advantage claim from local simulation or receipt machinery |

### Finite-map compatibility

CDC's native `flow(d)` reads a pre-step snapshot and applies one finite synchronous update. In the scalar counterexample `F_d(x) = x + d cos(x)`, `F_1(F_1(0)) = 1 + cos(1)`, while `F_2(0) = 2`. An optimizer cannot merge these steps through the exact-ODE semigroup law.

The exact-bit compatibility profile also binds the compiler and floating-point contraction choices. The inspected arm64 Apple Clang 21 `-O2` oracle emits fused multiply-add instructions; replacing those with separate multiply and add can differ by one bit. Explicit fusion belongs in the selected realization contract and its parity receipt. Analytic Jacobians differentiate the real extension of the executed finite-step expression under its fixed itinerary, not literal discontinuous IEEE rounding; exact-bit primal and numerical-derivative agreement are separate verification claims.

The actual `nest` update copies the newly updated parent belief into the child's prior; its derivative must describe that assignment and the fixed discrete mode. The scheduled `commit` leaves the continuous coordinates fixed on an accepted itinerary, so its reset Jacobian is identity there. Quantization boundaries or changed itineraries require a hold. Source-localized event saltation belongs to a different event contract.

U1 closure and U2 recurrence are separate. Absolute recurrence includes the complete declared continuous state and discrete restoration. Relative recurrence additionally requires an executable restoration map, its derivative and the required symmetry contract. A projected phase match is insufficient. For column tangents, the ordered product is `A = J_n ... J_1`; relative monodromy is `D rho · A` only after recurrence is verified.

## Preserved design and accepted refinements

The supplied design documents remain byte-preserved under [provenance/source](../provenance/source/). The [review reconciliation](../provenance/review/RECONCILIATION.md) resolves earlier qualifications into five implementation requirements: deterministic whole-surface elaboration, checked theory composition, multidimensional admission, contract-sensitive artifact identity and conservative independence witnesses.

All 22 original examples were recovered from supplied text. The original grammar, executable audit, source ledger and further structured package assets remain absent. A new grammar or verifier in this repository is labeled new implementation. Preservation of supplied sources is not a claim to have recovered every historical workspace artifact or rerun the historical 40-check audit.

## Full-completion boundary

The [original execution directive](../provenance/source/CODEX_EXECUTION_DIRECTIVE.md) remains the end-state target. The [current claim ledger](../CLAIMS.md) controls what this checkout has earned. The native compiler, graph, libraries and developer tools now execute in U, and self-compilation is reproducible under the recorded toolchain. The wider target includes admitted source fragments with preservation evidence, complete domain contracts, production hardening, specialized execution and stronger formal correspondence. Each part retains its own acceptance criteria.
