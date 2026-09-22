# U 0.2: implementation and evidence

U's compiler, graph reconstruction, domain libraries, proof kernel and production tools are written in U and compile to native functions. The ordinary build starts from a checked generated-C seed. It does not require Python.

The [native verification report](artifacts/native-verification.json) records the tested checkout and results. The [normal bootstrap report](artifacts/native-bootstrap.json) covers the checked-C build without Python, while the [self-compilation record](tests/selfhost/verification.json) records compiler generations and toolchain identity. Earlier reports without the `native-` prefix describe the historical 0.1 reference implementation; their counts are not native-release results.

| Area | Implemented path | Evidence boundary |
|---|---|---|
| Source and compiler | U scanner, parser, core checking, lexical closures, lazy globals and direct C generation | General dependent conversion and complete static resource checking remain obligations |
| Self-compilation | Checked C seed, U-owned regeneration, three identical generated-C stages, same-basename native binary comparison | Reproducibility is not a proof against a compromised compiler or platform |
| Graph | Six-constructor executable bodies, deterministic lexical bindings, validated reconstruction and relowering | Structural identity is not general semantic equivalence; provenance text is checked separately before export |
| Values and staging | Arbitrary-precision integers, structural recursion, collections, compiled quotation and single-evaluation binding | General typed macro expansion and total conversion are not inferred from a Code label |
| Relations and probability | Finite unification/search, SQL bags and NULL, weighted measures, seeded HMC | HMC uses numerical gradients and bounded isolated batches; no convergence certificate |
| Processes | OS-isolated tasks, result sharing, cancellation, autonomous continuations, HTTP and actor supervision | Resource transfer across processes is restricted; no distributed correctness claim |
| Scientific models | Strict array contraction, affine index-1 backward Euler, finite temporal checks, clock/GPU/statevector simulation | Simulator output is not physical GPU, HDL or QPU execution |
| Proof | U-written total Nat/Pi/Eq checker; original induction example checked; opaque bound certificates | Not a complete dependent type theory or a whole-compiler correctness proof |
| CDC reductions | U flow, nonnegative-prefix commit and nest under the pinned numeric contract | Exact parity is scoped by the independent oracle receipt and host numeric profile |
| CDC analysis | Recorded paths, local Jacobians, ordered tangents, complete-state recurrence, uniform phase restoration and triangular spectra | General dense Schur and complete U1 source/effect binding remain explicit holds |
| Evidence | Typed length-framed identities, dependency validation and invalidation | Hashes establish identity, not authenticity, truth or authority |
| Tooling | Native CLI, formatter, graph lift/export, stdio language server, immutable local package snapshots and artifact audit | No network resolver, package hooks or hostile-host sandbox |
| Browser target | U interval analysis and direct WASM emission for exact integer expressions and Boolean predicates | Guarded signed-i64 intervals; exact BigInt input checks must precede host ABI coercion |
| Publication | Interactive U and BiDi sites, outward-facing READMEs, formal manuscripts and source bundles | Manuscripts are preprints; no arXiv submission or peer review is claimed |

## Remaining limits

The [remaining-gates inventory](docs/FULL_COMPLETION_BACKLOG.md) retains the wider language target. Major open areas are complete theory/adapter contracts, stronger dependent and static resource analysis, full CDC U1/U2 compatibility, the eighteen foreign-language frontends and their preservation arguments, general WASM lowering, long-running garbage collection, production persistence/distribution and physical realization gates.

The handwritten C bridge supplies generic values, arithmetic, storage, calling conventions and operating-system primitives. Generated C is recorded as build output. The [native ABI](native/ABI.md) identifies this platform boundary, its trust requirements and its allocation model.
