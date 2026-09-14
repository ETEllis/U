You are Codex acting as principal language designer, compiler/runtime engineer and verification lead. Build the production-intent U language and semantic substrate described below. This is not a toy language, transpiler collection, demo, or MVP. Preserve the full target architecture from the first commit, implement coherent vertical paths in phases, and keep every unimplemented capability explicitly gated. Do not replace execution with scaffolding or canned outputs.

PROJECT AND SOURCE OF TRUTH

Create a new local Git repository named U as a sibling of the existing BiDi repository, unless inspection establishes and records a compelling architectural reason otherwise. Do not modify BiDi initially. Record its branch, SHA, dirty status and version, and verify that your work leaves it unchanged. Do not publish private source, credentials, or a remote repository without explicit authorization.

The design audit inspected ETEllis/BiDi main at 1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157, CDC release 0.3.0, grammar 1, C ABI 1.5; GitHub Actions run 31252447769 was successful for that exact head. Fetch current main rather than trusting this pin as forever current. If it moved, inspect the delta, preserve the audited profile, and add a new compatibility profile rather than silently redefining old semantics.

Inspect and reconcile README.md, CDC_LANGUAGE.md, FORMAL_SEMANTIC_SPINE.md, UNIVERSAL_OPERATOR_SYSTEM.md, VERIFICATION_OBLIGATION_MATRIX.md, FRAMEWORKS.md, NATIVE_SELF_HOSTING_MANDATE.md, CDC_TOOLCHAIN_PLAN.md, historical BIDI_CALCULUS_CORE.md, docs/u2/U2_SEMANTICS.md, docs/rftc/RFTC_FULL_BUILD_SPEC.md, docs/rftc/VERIFICATION_OBLIGATION_MATRIX.md, and relevant runtime/parser/AST/compiler/store/scheduler/RFTC/U1/U2/ABI/proof/tests. Follow the repository's authority order; late binding amendments outrank old phase sketches. Record what was specified, executed, adversarially checked, mechanized-finite and open. Do not turn CI success into a proof of implementation correctness.

Historical truth is mandatory: CDC was discovered and implemented first. U is a later abstraction beneath CDC. A new U–CDC bridge does not mean CDC was originally built on U. Keep .cdc stable and useful. U is not a user-facing replacement imposed on CDC.

NAMING

U and .u are collision-prone working identifiers, not cleared branding. Unison and Ü use .u; another public U language specification does too; .u also has UnrealScript-package and historical ucode uses. Use qualified language identifier etellis.u and a required header u "etellis.u/0.1";. Keep .u only in opted-in workspaces; do not globally seize its editor association. Use a provisional qualified executable such as etellis-u and an optional workspace-local u alias. Record package/CLI/trademark clearance as a release gate; do not claim availability.

FOUNDATIONAL DESIGN

U's center is: an operation exposes the laws under which it can be composed. Separate its meaning, its selected realization, and its earned evidence. These must be separately versioned linked artifacts.

The structural kernel AST has exactly these six constructors in the initial design:
1. wire(permutation): forward/reorder existing typed ports, never implicit copy/drop/allocation.
2. gen(theory_id, operator_id, static_parameters, typed_regions): a declared semantic generator, not an arbitrary foreign callback.
3. seq(p,q): compatible boundary composition.
4. par(p,q): independent composition requiring an explicit resource/causality split.
5. scope(name_kind, body): fresh nominal binding with capture avoidance and escape checks, not physical allocation.
6. fix(iteration_mode, binder, body): theory-qualified iteration/feedback, never universally available recursion.

Do not pretend gen is one semantic primitive that makes everything else free. Maintain an explicit generator/law/model/trust ledger. The seven initial semantic obligation clusters are V constructive values/types/proofs, M resources/byte memory/machine models, R relations/constraints/search, P measures/probability, C causal behaviors/clocks/processes, D trajectories/differential constraints, and Q quantum channels/instruments. They are not a proved minimal set of axioms. Add a primitive only with a recorded native-compression argument and counterexample to the existing basis.

Each theory descriptor must contain: immutable identity and digest; kind/interface constructors; operator type schemes; static parameter schemas; region binders/stages/effect/resource limits; denotation/model reference; executable reference rules or a checked U-native expansion; laws and assumptions; observables; held/error outcomes; adapters; conformance cases; and trust dependencies. A name plus a backend stub is not native semantic possession.

Implement typing as an explicit unrestricted context plus affine/linear resource context. Use stratified universes, dependent Pi/Sigma, inductive data, equality/eliminators, indexed opaque interfaces and staged Code types. Type conversion is total. General partial recursion, IO, arbitrary solver search and unchecked foreign evaluation cannot enter proof conversion or construct checked proof/authority/recurrence values. Use bidirectional checking and require annotations where necessary; do not promise complete dependent-type inference.

Copy and Drop are admitted interface witnesses, not universal operations. A physical QReg cannot gain Copy through ordinary trait instances. Borrowing and authority leases are distinct types: borrows constrain alias/access/lifetime; leases constrain issuer/action/frame/horizon/expiry/epoch/nonce. Typed code descriptions can be copied when safe; physical resources cannot be reflected into ordinary values.

Cross-theory composition requires an admitted composite model or an explicit adapter with interface map, observation map, assumptions, preservation/refinement/error relation, residual policy and checker. Probability and nondeterministic choice cannot commute by default. A simulator cannot satisfy a physical-device capability. Continuous paths cannot be silently replaced by finite samples. One trace cannot replace a behavior set. A passing test cannot create a proof.

CANONICAL SOURCE AND IR

Implement one shared surface grammar: version header; exact-profile imports; def name(params)->type = expression; def name:type = expression; fn(params)=>expression; qualified calls; tuples; lists; records spelled #{field:value}; and blocks { let x=expression; expression; yield expression; }. Types are expressions checked in the total fragment. Domain operators use this grammar, not SQL/Prolog/QASM strings sent to hidden interpreters. Region signatures expose whether their body is evaluated, staged, declarative, clocked, probabilistic or quantum. Do not let visually similar syntax hide that information from the typechecker/editor.

When available, use the supplied surface.lark and 22 examples as the design-candidate syntax, not as proof that all examples already typecheck. Resolve any genuine type/interface defects with a documented spec change, not a silent special case. Create a lossless CST, stable AST, typed operation graph, formatter and canonical serializer from a common parser. Reject malformed input predictably and fuzz all layers.

Use a deterministic tagged graph encoding. Preserve ordered ports, binder identity, ordered regions, theory/profile hashes, exact attributes, provenance and residual references. Canonical JSON is acceptable initially with sorted keys, no duplicates, deterministic traversal IDs and tagged numeric payloads. Unbounded integers/rationals use canonical strings; IEEE values carry width and exact hexadecimal bits. Never call canonical structural identity general semantic equivalence. Human Unicode/infix/graphical forms are reversible projections with ASCII equivalents, not alternative semantics.

STATE, EFFECTS AND EVIDENCE

Keep original byte identity, canonical syntax identity, semantic-artifact identity, execution identity, replay projection identity and nominal resource identity distinct. A digest authenticates neither the signer nor the truth of a claim; signatures and external history anchors are separate mechanisms. A history commitment without payload is not reconstructible history.

Outcomes include Done, Held with obligations/earned artifacts, Rejected, Fault with known progress, and Indeterminate when an external effect may have occurred. Divergence is not generally detectable. HOLD prevents the current protected application mutation but may append an audit/control receipt; it does not reverse earlier effects. Parse/configuration errors and missing realizations are not success-shaped held computations. Enforce effect admission before execution; post-hoc receipt validation cannot retroactively prevent an unauthorized effect.

Receipts bind scope, maturity dimensions, verdict, obligations, assumptions, checker/runtime identity, profile/source/dependency/input identity, scheduler/randomness/external observations when relevant, numeric/clock contract, and only earned output artifacts. Evidence levels are not a scalar ladder. Unknown, missing and skipped fields stay explicit. Optimizations must invalidate or explicitly transport certificates; never reuse an admission/proof/recurrence certificate across a changed scope, artifact, or source hash without its preservation obligation. Only checked constructors create Proof, AdmittedEffect, VerifiedRecurrence, ReturnMap and physical-device execution receipts.

EXACT CDC BRIDGE

Create an independent U-native lowering/evaluator for the pinned CDC profile, plus an isolated original-CDC differential oracle. Invoking cdc run and relabeling its output is not native lowering. At the audited pin, cdc_runtime_execute in the C ABI still returns an unavailable-state error although native CLI execution exists. Do not build against an imagined completed ABI. Use actual supported parsing/canonical/registry surfaces or implement the specified frontend; use the CLI transparently for tests.

Preserve source expectations/assertions, tolerance failures, budget/cancellation boundaries, result schemas and entry-specific sequencing; do not invent transactional rollback after a native post-step assertion fails. Preserve ordered statements/tokens and consumer-specific attribute semantics: the registry dictionary view is last-wins while the legacy native scanner view is first-wins. Retain original bytes as residual and canonical parser identity as a distinct artifact. Do not reread a mutable path later and call those bytes the executed source.

Implement current flow exactly as the synchronous finite map
 theta_i' = theta_i + omega_i*d + G*d*sum_e w_e*sin(theta_source+angle_e-theta_i),
using the old snapshot and eligible same-field channels. Do not invent exact ODE flow, amplitude relaxation, plasticity or delay history. Pin float operation order and library/numeric contracts: preserve the source-ordered additions and gain*weight*sin*duration sequence rather than algebraically factoring the displayed sum. Derivative contracts refer to the real-valued extension of the executed map, not an exact classical derivative of binary64 rounding. Numeric AD/finite-difference evidence must state that scope. Retain the derived cdc.flow operator and its checked expansion.

Commit quantizes cos(theta) with the exact deadband to -1/0/+1, scans trits in source order for nonnegative prefixes, and only then updates latch values/flags. Hold leaves the protected latch state unchanged. Zero is a trit aperture value, not false and not the same as Held. Preserve entry-specific sequencing: plain native steps do not acquire an automatic global abort-on-HOLD policy. U1/U2 accepted-path admission remains stricter.

Nest computes the child mean latched/current trit under native rules, updates parent belief by parent-field gain times that mean, then overwrites child prior with the new parent belief. Its fixed-itinerary Jacobian copies the parent-belief row into the child-prior row; do not differentiate a theoretical belief-minus-prior update absent from execution.

U1 verifies bound reciprocal/cover/admissibility/generated-coordinate/effect conditions. It does not establish full-state recurrence. U2 uses the exact selected state manifest, discrete modes, source order and analytic local rules. Propagate column tangents by A <- J_k*A. Scheduled commits use their scheduled reset derivative, not invented source-localized saltation. Guard-localized events require their actual event/transversality/itinerary contract before saltation is used.

Absolute recurrence checks complete declared state and discrete restoration. Relative recurrence requires executable rho, verified symmetry/equivariance conditions and bound D-rho, with M = D-rho*A. Projected/omitted coordinates do not authorize full monodromy. Canonical loop-u720 must retain its current held/non-recurrent distinction. A downstream spectral hold retains an already earned monodromy but emits no fabricated multipliers. Preserve legacy CDC receipt precision and schema; add U exact-bit sidecars without silently redefining legacy hashes.

Reproduce the eight U2 mutant families: reversed product, omitted saltation, omitted restoration derivative, theoretical nest derivative, false/projected recurrence acceptance, ignored polarity aperture, skipped tangent conjugacy and automatic false polarity acceptance. Add store/replay/authority/scheduler adversarial cases. RFTC classical local machinery does not earn multihost, quantum or physical-law claims.

NATIVE COVERAGE AND INGESTION

Maintain a conformance matrix for Assembly, C, Rust, Python, JS/TS, Lisp/Racket, Haskell, Prolog, SQL, APL, Stan, Erlang, TLA+, Lean, SystemVerilog, CUDA, OpenQASM and CDC. Add acausal DAE, synchronous causality, least-fixed-point logic and effect-handler probes. Classify every operator as theory primitive, derived-native, library, backend, foreign, residual-preserved or unsupported. Publish actual status, not desired status.

For each admitted fragment define E(P)=(typed U graph,residual,provenance) and an explicit source/target observational relation. Distinguish faithful semantic lifting from backend refinement of allowed nondeterminism. Use domain-appropriate relations: traces/bisimulation, memory/ABI, bags/NULL, laziness/bottom, distributions, type/proof judgments, hardware cycles or quantum channels. Numerical tolerances require norms and propagation assumptions.

Native status requires explicit inspectable operators, compositional meaning, direct U rules/checked expansion, analyzability, recoverability, and differential/adversarial evidence. A giant hidden source-language interpreter fails native subsumption. Turing completeness is never the substitute for this test.

Legacy import retains lexical, intensional, semantic, environment and approximation residuals with source maps. reconstruct(G,R)=original bytes is useful but not the semantic theorem. Transforms must update residuals through checked lenses or invalidate exact export honestly. Unmodeled source regions remain foreign/unsupported even if their original text is retained. Imported proof candidates are independently checked; external typechecker/test success is evidence only of its actual scope.

BUILD ARCHITECTURE

Use an explicit bootstrap implementation, preferably a memory-safe Rust workspace unless repository/tooling constraints justify another choice. This is allowed bootstrap engineering, not a claim of self-hosting. Keep independent crates/modules for source/CST/parser/formatter, core graph/substitution, kinds/types/resources/proof kernel, theory/adapters, reference evaluator, seven theory clusters, evidence/canonical/store, optimization/realization/bytecode, native/WASM/interchange, specialized backends, import-core/source frontends/CDC bridge, CLI and LSP. Do not force every path through a single giant runtime switch or monolithic interpreter.

Create docs/SPEC.md, AUTHORITY.md, CLAIMS.md, DESIGN_HISTORY.md, theory/adapter/profile documentation, decisions, threat model, migration rules, verification matrix, exact grammar/AST/receipt schemas, and a machine-readable capability ledger. Include U.toml and an exact U.lock from the start. Parsing/checking/formatting must not perform undeclared IO. Package builds run under explicit filesystem/network/subprocess capabilities; importing a package does not trust its arbitrary scripts.

Provide check/run/eval/test/prove/fmt/build/lift/export/explain/audit/lsp and package lock/install/verify command paths. A command whose owning capability is unfinished returns a typed explicit unsupported result, not dummy success. Implement a useful LSP path showing profiles, effects, ownership, residual loss, obligations and reasons for holds; an editor folder alone does not satisfy it.

Implement the 20 representative programs: hello effect; arithmetic; structural-recursive Fibonacci; map/filter/fold; ownership-sensitive mutation; async/event request; hygienic quote/splice rewrite; family relation query; SQL multitable bag join; ranked array contraction; Bayesian measure model/inference separation; supervised actors; temporal safety/liveness with fairness; an independently checked inductive theorem; synchronous 8-bit counter; partitioned GPU vector kernel; Bell circuit/instrument; CDC flow/commit/nest; U1 guarded closure; U2 recurrence-gated variation. Add acausal DAE and an explicit native universal machine construction. Never hard-code an expected output in lieu of the mechanism.

VERIFICATION AND PERFORMANCE

Use unit/property/metamorphic/differential/fuzz/mutation/security/crash/replay suites. Cross-theory negatives must reject implicit QReg copy, invalid lifetime escape, probability/search reordering, SQL bag flattening, eager evaluation of a lazy divergent argument, wrong event/clock order, racing device writes, incorrect CDC Jacobians, false recurrence, forged or stale capability receipts, and proof creation through partial recursion or unsafe coercion.

Create the reference evaluator before optimizing. Establish native and WASM execution parity for admitted fragments. Keep GPU/HDL/QPU interfaces designed and typed, but require real execution gates before advertising their physical backends. A simulator or emitted interchange file has its own lower claim level. Compare performance only under matched semantics, hardware, numeric mode, workload and resource accounting. Track compile time, runtime, memory, code density, learnability, analysis/proof coverage and ecosystem separately; never claim universally faster execution.

Formalize structural/type/resource preservation, qualified progress, proof-fragment boundaries, operator expansion, profile/adapter laws, source lifting, backend refinement, residual reconstruction, CDC/U2 non-fabrication and replay invariants. State the exact trust and theorem scope. Finite model checking is not an unbounded theorem; an abstract theorem is not automatically a proof about compiled code.

Self-hosting milestones: stage0 explicit host bootstrap; enough U implementation to write its parser/formatter/elaborator; stage1 compiler in U; stage2 reproducible self-compilation; independent reference/differential checks; bootstrap trust manifest and removal gates. Do not destroy the independent oracle merely because the compiler can compile itself.

EXECUTION POLICY AND HANDOFF

Work toward the full architecture, not an intentionally disposable MVP. Phase implementation into coherent end-to-end paths: contracts/CST/core; executable values/resources/proof fragment; native CDC bridge; theory conformance; ingestion and law-aware transforms; production classical runtime; specialized execution gates; strengthened formal correspondence and self-hosting. Parallelize independent work behind stable interfaces. Empty module trees and superficial examples do not count as completed phases.

Run the actual tests you report. Preserve exact command/environment/artifact receipts. Keep supported, held, failed, skipped and unimplemented cases visible. When a real environment or dependency blocker prevents a gate, record the blocker and continue independent reachable work without faking success. Do not silently shrink the final scope. Leave the repository with runnable completed paths, explicit unimplemented interfaces, specifications, tests, formatter/LSP/CLI progress, verified BiDi non-modification, a precise claim ledger, and an ordered full-completion backlog.

Conclude each execution handoff with exact files changed, commands run and outcomes, demonstrated native operators, evidence scope, open obligations, failed/skipped gates, and the next architecture-preserving work. Do not claim full U completion until its documented release gates actually pass.
