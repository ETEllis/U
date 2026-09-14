# U 0.1.0 reference release: demonstrated scope

Fresh local verification on September 14, 2026: **121/121 tests passed, zero skipped; 96/96 randomized CDC primitive comparisons matched exact binary64 and latch state; 23 Lean theorems compiled without axiom dependencies.** The formal manuscript is 18 pages and was rendered and visually inspected. These results describe the tested reference release, not completion of the entire original language directive.

| Surface | Demonstrated result | Evidence and boundary |
|---|---|---|
| Preservation | Five supplied sources remain byte-exact; all 22 original examples match historical source hashes | `artifacts/verification.json`; original 40-check historical audit was not rerun |
| Frontend | All 22 parse, format idempotently, elaborate deterministically and reconstruct from six kernel tags | `tests/test_frontend.py`; seven pass the bounded static checker, advanced domains retain explicit obligations |
| Reference runtime | 123 registered operators with inspectable implementations across V/M/R/P/C/D/Q | `spec/capabilities.json`, `docs/RUNTIME.md`; registration is not a native-subsumption proof or complete descriptor/expansion certificate |
| Values/resources | Arbitrary-precision arithmetic, lexical closures, structural recursion, explicit partial iteration, borrowing and scoped capabilities | Actual source execution and negative tests; full dependent/resource calculus remains open |
| Scientific domains | Typed finite logic/search, SQL bags/NULL, strict contraction, explicit measures and seeded HMC, affine DAE integration | Tested supported fragments; no claim of general Prolog/SQL/Stan/Modelica source frontends |
| Processes and simulators | Real asynchronous HTTP against a loopback server; cancellation/failure; bounded supervision; fairness-sensitive finite temporal checking; clocked and GPU reference simulation; joint quantum instrument simulation | `tests/test_runtime.py`, `tests/test_integration.py`; simulators earn no device execution |
| Proof | Original `zero_add` source yields a checked closed Nat/Pi/Eq induction judgment | Independent `u/proof.py`; malformed steps, shadowing exploits and partial/foreign operations are rejected. Wider dependent static checking remains incomplete |
| CDC primitive bridge | Flow/commit/nest reproduce the pinned C runtime on 96 seeded multi-step programs, including HOLD and source order | `artifacts/cdc-parity.json`; exact tested Apple arm64/libm/FMA profile, finite valid primitive source fragment |
| CDC analysis | Actual finite-map local derivatives, ordered tangents, full-manifest numerical absolute recurrence, certificate binding and retained return artifacts on spectral HOLD | `tests/test_foundations.py`; full U1/U2 source parity, general relative symmetry and validated Schur remain open |
| Native and WASM | Direct compiled exact-integer expression functions agree with the U reference evaluator over 35 test inputs; out-of-domain inputs are rejected/trapped | `tests/test_integration.py`; interval certification bounds every intermediate; not a general compiler backend |
| Identity/persistence | Injective typed encoding for admitted values, domain-separated identities, transitive dependency checks, local locked/fsynced journal, corruption/torn-tail detection | Regression and runtime tests; hashes are not authentication, process-local leases are not distributed cryptographic authority |
| Tooling | CLI, stdio LSP diagnostics/hover/definitions/formatting, exact source lift/export, locked local package installation, installable wheel | Integration tests; no package scripts executed; no network dependency resolver or hostile-host sandbox |
| Formal artifact | 23 machine-checked theorems with no axiom dependencies, plus 1,092 finite trit sequences in empirical bridge tests | `formal/U.lean`, `formal/README.md`; proofs concern defined abstract/finite models, not whole Python runtime correctness |
| Publication | Substantial README, responsive landing page, 18-page PDF and arXiv source ZIP | Browser interactions checked at desktop/mobile; paper rendered. No arXiv submission or public-site deployment |

## Full completion remains open

This build does **not** complete the full original directive. The material remaining work includes complete theory/adapter machine contracts and checked expansions; the full dependent type/resource core; native U1/U2 CDC source compatibility and all original mutants; the eighteen foreign frontends and preservation theorems; a general optimizing native/WASM runtime; production crash/security/distribution hardening; physical GPU/HDL/QPU gates; and actual stage1/stage2 self-hosting. These are tracked individually in `docs/FULL_COMPLETION_BACKLOG.md`.

The repository preserves the original end state. A successful build, clean commit, GitHub workflow, rendered paper or a passed narrow test is not evidence that those unimplemented gates have closed. The private repository and local landing page keep that distinction visible.
