**Independent review of U 0.1 (architecture and specification candidate)**

U’s strongest idea is not a sixth constructor or a seventh “nucleus.” It is a **composition protocol that refuses to flatten domain law**. An operation is an open interface plus the obligations that govern wiring, duplication, observation, iteration, and evidence. Meaning, realization, and earned evidence stay distinct artifacts (A.1, O1). The six graph roles are a readable structural basis; the seven clusters are obligation ledgers, not a minimality theorem (A.2, C.3, D.5). Native possession is given a real failing test: a residual that still runs the source interpreter does not count (H.1). CDC is a pinned specialization (`etellis.cdc/native-0.3.0@1307f2a7…`), not a rewritten origin (historical statement; I.3–I.4; O14).

That architecture is coherent. It already answers the usual recasts (universal rewrite, MLIR-with-labels, “just Lisp,” CDC-as-kernel) in N.1–N.3. I see **no contradiction in the center** that would justify shrinking the end-state.

**Readiness.** Candidate 0.1 is ready to start **Milestone 0–1 foundational implementation** (P.3): identity lock, lossless CST, six-tag kernel, total V fragment, reference evaluator, evidence records. It is **not** ready to claim language completion, eighteen-language subsumption, general cross-theory soundness, CDC parity, production runtime, or physical backends. The 40/40 receipt is a historic scoped grammar/math probe set; `U_language_implemented=false`; BiDi was not rerun here; `surface.lark`, `check_design.py`, and `sources.json` are still absent. The 22 example bodies are hash-verified extracts of the dossier, not typed or executed U programs (J intro; local audit artifact). Historic probes must not be treated as a fresh package rerun.

---

### Material findings (≤5)

**1. Surface elaboration is unspecified (new representational gap, not a semantic contradiction).**  
O2 and D.1 freeze six kernel constructors. The human language is `def`/`let`/`yield`, records, `fn`, qualified calls, and derived names (`sql.inner_join`, `cdc.flow`, `nat.rec`) (G, J.01–J.22). The dossier never says how that surface becomes exactly those six tags: whether `let` is `seq`+`scope`, whether a derived join is `gen` plus an expansion witness, or whether a larger elaborated AST merely *projects* to six constructors. K.2 and O2 require inspectable derived nodes; they do not pin the encoding.  
**Failure:** two implementations can parse the same `.u` text into different kernel graphs, different canonical hashes (O8–O9), and non-comparable evidence. Optimizers then disagree about what “the” meaning graph is.  
**Smallest refinement:** state that the canonical kernel has exactly six tags; every surface form has a deterministic elaboration; every derived operator is `gen[T,o]` whose descriptor binds an immutable expansion or reference rule. No seventh kernel tag.

**2. Same-`T` sequential composition vs mixed examples (acknowledged research, plus a concrete clash).**  
D.3 types `seq` only when both sides share profile `T`. D.6 and F.9 require an admitted composite profile or adapter; N.4 names cross-theory composition as unresolved. The 22 examples still *look* like ordinary sequential programs spanning V/M/R/P/C/D/Q without adapter nodes or composite-profile annotations (especially J.06, J.11, J.16–J.20).  
**Failure:** implementers will either (a) silently infer a “sum of theories” so that `seq` typechecks the examples, recreating the probability/choice collapse C.2 forbids, or (b) reject the examples as untyped, which the syntax audit never tested.  
**Smallest refinement:** mixed-theory `seq` is illegal until an explicit adapter node or named composite profile appears in the graph. Treat J.* as untyped illustrations until those annotations exist. The C.2 choose-vs-observe pair must produce two artifact identities, not one effect union. This does not solve general composition; it stops the wrong default.

**3. Operator-admission and conversion TCB are named, not judged (acknowledged; Codex 2+4).**  
K.2/`OPERATOR_CONTRACTS.md` list descriptor fields; O4 allows unchecked laws as visible assumptions; E.1/O3 forbid partial recursion, IO, search, and foreign evaluation in conversion, while allowing indexed opaque interfaces and theory extension. There is no machine verdict for TrustedPrimitive vs KernelCheckedExpansion vs AssumedLaw vs BackendOnly vs Unsupported, and no frozen 0.1 conversion fragment (universes, positivity, opacity, definitional equality). Contracts also omit most operators the examples actually call (`cdc.flow`, `sql.inner_join`, `gpu.partition_launch`, `prob.infer`, …). The file is a seed, not a closed 0.1 ledger.  
**Failure:** a name-plus-callback “descriptor” acquires native status; or opacity/extension leaks runtime/solver results into `CheckedProof`. Example signatures get invented crate-by-crate and later cannot round-trip.  
**Smallest refinement:** freeze (i) admission class + TCB delta on every operator, (ii) a small total core (Sort/Pi/Sigma/strictly-positive inductives/Eq/structural rec/Code), (iii) explicit schemas for every operator in J.01–J.22. Prose plus a backend stub may parse and must not enter proof conversion or preservation evidence.

**4. `par` / `Independent_T` has no witness language and almost no examples (acknowledged; coverage gap).**  
D.3 requires a resource split and `Independent_T`; E.2 distinguishes copy, borrow, and lease; interchange in D.4 is legal only when both sides are. None of the 22 programs is a kernel `par`. GPU disjoint writes are a specialized `gen` (J.16); CDC nest is sequential (J.18).  
**Failure:** textual separation or distinct names get treated as independence (explicitly forbidden), or ordinary `par` of disjoint owned values is rejected and the constructor atrophies. K.4 rewrites that assume interchange then become unsound.  
The design already rejects “syntax is parallel ⇒ commutation.” What is missing is the first conservative witness algebra.  
**Smallest refinement:** 0.1 witnesses = disjoint affine/linear tokens, explicit shared-read, named causal edges; aliasing/device overlap is Held/obligated, never inferred. Kernel `par` still exists; its first decision procedure may be narrow.

**5. Artifact dependency direction is unstated (new schema ambiguity; Codex 6, adjacent to O1/K.2).**  
O1 requires distinct meaning, realization, and evidence. `TheoryDescriptor`/`OperatorSchema` mix model references, executable rules, adapters, and permitted realizations (K.2). Adapter is defined as a contract (F.9) but not as an identity in the three-artifact DAG. O8 pins JSON hygiene, not whether binders are names or de Bruijn in the hash.  
**Failure:** a backend swap mutates “meaning”; a run receipt becomes part of a theory digest; cyclic descriptors hash-stable-loop; CDC BLAKE3 vs U SHA-256 (O9) gets collapsed.  
**Smallest refinement:** acyclic content addressing—meaning never depends on an execution receipt; realization depends on meaning; evidence depends on both; adapters are named meaning-level objects. Discriminate structural identity from extensional equivalence (already stated; must be schema-enforced).

These are seams, not a license to replace plurality with one callback or a demo subset. Sound general adapters, full intensional recovery, hybrid AD, and ergonomics remain N.4 research and must stay gated (H.2, P.3, O13).

---

### Codex preliminary findings

**Agree** with the recovered center, the falsifiable native test, CDC-as-derived-profile, the refusal to treat GPT-6 and the dossier as independent confirmation, and the readiness split (foundational slice vs subsumption). The six seams are real. The historic-audit boundary is right once updated: examples are hash-verified extracts; grammar/checker/sources ledger remain missing; 40/40 is not a fresh rerun and not execution of U.

**Disagree in emphasis, not thesis.**  
(1) Composite profiles are not only underspecified: D.3 plus the examples will force a wrong default unless mixed `seq` is illegal without an adapter.  
(2) Finding 3 (derived nodes) is too small; the live risk is **whole-surface elaboration**, not only join-as-seventh-tag.  
(3) Admission (Codex 2) and conversion freeze (Codex 4) should be one TCB decision, and must include freezing J.01–J.22 operator schemas—the contracts file does not.  
(4) I would not add a seventh “fatal” flaw. Naming collisions (B.1) and learnability (G, N.3) are already decided as working-title / pre-1.0 empirical work.

---

### Last refinement round, then start

Do this **before** crate sprawl, not instead of the architecture:

1. Elaboration + six-tag kernel projection; derived = `gen` + immutable expansion.  
2. Composite-profile/adapter object; mixed `seq` rejected without it.  
3. Operator admission class + frozen 0.1 total core + schemas for J.01–J.22.  
4. Meaning → realization → evidence DAG; adapters as meaning identities.  
5. Narrow `par` witnesses (can lag the first evaluator, not the AST).

**Discriminating tests (must fail if the seam is wrong):**  
- Derived join serializes with six tags only; expansion change changes meaning identity and invalidates stale evidence.  
- Choose-then-observe vs observe-then-choose: distinct profiles/identities; implicit commute rejected (C.2).  
- Descriptor with prose + callback: parseable, not native, not a proof.  
- `core.fix_partial` / solver bytes / runtime values cannot construct `CheckedProof` (J.14, E.1, J.22).  
- One linear token in both `par` branches rejected; disjoint owned tokens pass; shared read needs a witness.  
- Backend swap leaves meaning hash stable; a run changes only execution/evidence (O1, E.5).  
- CDC: finite-map `flow` ≠ ODE semigroup; HOLD ≠ false ≠ aperture 0; no abort-on-HOLD invented for the native path; U2 mutants remain failing (I.4, I.7, P.4).  
- J.22 never cited as SQL/Prolog/Q/hardware subsumption (H.1, J.23).

After those meta-contracts exist, authorize the thin vertical path in P.3 Milestone 1. Keep the full target interfaces from the first commit; keep unimplemented capabilities explicitly unsupported. That is permission to build the substrate—not a claim that the universal-language problem is solved, that the theory basis is minimal, or that foreign languages are already natively possessed.
