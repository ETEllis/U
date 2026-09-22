# Completion record and remaining work

The transition to native U ownership is complete for the supported 0.2 compiler, libraries and tools. The broader language program continues through the separate gates below. Reproducible self-compilation, domain conformance and formal correctness answer different questions.

## Completed native foundation

| Area | Established result | Scope |
|---|---|---|
| Native implementation | U owns the compiler, executable graph, domain libraries, proof kernel, CDC analysis and production tools | The supported source and domain fragments listed in [CLAIMS.md](../CLAIMS.md) |
| Self-hosting | A checked generated-C seed starts U; three self-generated compiler stages agree, with native binary reproduction under the recorded toolchain | Independent comparison remains part of verification; the platform bridge and C toolchain remain disclosed dependencies |
| Source and developer tools | Native parsing, formatting, graph reconstruction/export, CLI, language server, package snapshots and artifact auditing | Formatting and graph checks cover the preserved corpus; package installation is local and executes no hooks |
| Browser compilation | U performs interval analysis and direct WASM emission for integer expressions and Boolean predicates | Guarded signed-i64 ranges, with exact host input checks before ABI conversion |

## Remaining research and production gates

| Gate | Existing foundation | Work required |
|---|---|---|
| Theory and adapter contracts | Explicit graph, profile, identity and finite composition checks | Complete per-operator schemas, checked reusable expansions, broader adapters and their preservation arguments |
| Values, resources and proofs | Native values, consuming handles, runtime borrowing and the U Nat/Pi/Eq kernel | Full stratified dependent forms, stronger static ownership/effect analysis, extension admission and metatheoretic correspondence |
| CDC compatibility | Native finite source import, primitive comparisons, ordered Jacobians, numerical recurrence, uniform phase restoration and triangular spectra | Complete U1 source/cover/decision/effect binding; general return symmetries and validated dense Schur; remaining source, store, scheduler and authority parity |
| Theory conformance | Native relation, probability, process, clock, DAE and coherent-state operations | Richer models and per-generator conformance beyond the supported fragments; statistical and numerical guarantees where required |
| Foreign-language ingestion | Reconstructible U graph and finite CDC import | Frontends for the eighteen comparison languages, explicit fragment contracts, checked residual updates and translation validation |
| General backends and optimization | General supported native target and a bounded WASM target | Broader WASM lowering, law-aware optimization and preservation checks across expanded targets |
| Production runtime | Native local processes, capabilities, packages, editor tools and bounded execution | Long-running garbage collection, robust persistence and crash recovery, distributed protocols, stronger isolation, cross-platform resource contracts and performance work |
| Physical realizations | Classical clock, launch and coherent-state simulators; numerical DAE realization | Actual device/provider access, calibration and provenance, race/barrier or synthesis checks, error guarantees and discriminating physical tests |
| Formal correspondence | Scoped mathematical results, finite formal mirrors and executable proof checking | Proofs connecting source, types, resources, graph, native backend and runtime to their mathematical models |

The next major integration work is complete native CDC U1/U2 compatibility and stronger descriptor/resource contracts, followed by broader source ingestion and realization support. Physical evidence requires physical execution and measurement. The native and self-hosting milestones above remain completed foundations rather than recurring open items.
