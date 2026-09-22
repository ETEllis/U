# Paper source and scope notes

The manuscript develops the U design and operator contracts through explicit refinements to elaboration, adapters, admission, artifact identity and independence. Its bibliography links primary papers and author manuscripts supporting the related-work discussion.

## Primary prior art consulted

| Source | Verification location | Use in paper |
|---|---|---|
| Lattner et al., MLIR (2020) | [arXiv:2002.11054](https://arxiv.org/abs/2002.11054) | Existing extensible operations, regions and lowering; no novelty claim for a common graph alone |
| Plotkin and Pretnar, Handlers of Algebraic Effects (2009) | [Author manuscript](https://homepages.inf.ed.ac.uk/gdp/publications/Effect_Handlers.pdf) | Operations, equations and lawful interpretation into models |
| Baez and Courser, Structured Cospans (2020) | [arXiv:1911.04630](https://arxiv.org/abs/1911.04630) | Existing open-system/interface composition; no automatic executable semantics inferred |
| Foster et al., Combinators for Bidirectional Tree Transformations (2007) | [Author manuscript](https://www.cis.upenn.edu/~bcpierce/papers/lenses-full.pdf), [author publication list](https://natefoster.org/publications/) | Lens background and limited source/view update argument |
| Leroy, Formal Verification of a Realistic Compiler (2009) | [Author manuscript](https://xavierleroy.org/publi/compcert-CACM.pdf) | Standard for concrete compiler preservation and honest correspondence gap |
| Cross et al., OpenQASM 3 (2022) | [arXiv:2104.14722](https://arxiv.org/abs/2104.14722) | Quantum/classical/timing comparison boundary |

Prior-art summaries are deliberately short. The elementary proofs in this paper are derived within its own stated models. They are not presented as newly invented general results in type theory, separation reasoning, category theory, cryptography, lenses or automatic differentiation.

## BiDi compatibility reference

Pinned revision: `1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157` in [ETEllis/BiDi](https://github.com/ETEllis/BiDi). The reference defines the compatibility profile. The paper's flow, commit, nest, U1/U2 and history statements are grounded in:

- `README.md` and `UNIVERSAL_OPERATOR_SYSTEM.md`;
- `FORMAL_SEMANTIC_SPINE.md` and `VERIFICATION_OBLIGATION_MATRIX.md`;
- `FRAMEWORKS.md`, `NATIVE_SELF_HOSTING_MANDATE.md` and `CDC_TOOLCHAIN_PLAN.md`;
- `docs/u2/U2_SEMANTICS.md`, `docs/rftc/VERIFICATION_OBLIGATION_MATRIX.md`;
- `paper/arxiv/main.tex`.

Historical self-hosting plans remain distinct from the current implementation. RFTC results concern local classical models; distributed and physical realizations require separate evidence. Source-specific pinned links are provided in [the architecture crosswalk](../docs/ARCHITECTURE.md#bidi-crosswalk).

## Mathematical audit notes

- The structural preservation theorem assumes locally sound admitted rules. It does not infer domain soundness from six tags.
- Resource nonduplication assumes actual nominal identities, exclusive constructor authority, fresh allocation, consuming transfer and correct footprints. Arbitrary alias analysis and adversarial process isolation require separate arguments.
- Adapter composition requires one compatible intermediate execution/observation and compatible quantifier directions. Assumptions accumulate; they are never silently discharged.
- The error bound requires a metric, domain containment, a uniform realization bound and a Lipschitz constant.
- Hash invalidation requires canonical encoding, complete retained dependencies and absence of collisions on encountered objects.
- Evidence monotonicity is restricted to a fixed subject. Expiry and subject changes cause new validity decisions and cannot reuse old authority.
- The lens argument's semantic conclusion requires interpretation to factor through the retained view; exact bytes alone do not earn native possession.
- The natural-number induction example follows the preserved example's addition recursion on its second argument.
- CDC derivatives describe the pinned finite map and fixed discrete itinerary; scheduled reset and state-localized event semantics remain distinct.
- Exact-bit CDC parity binds the arm64 Apple Clang 21 `-O2` reference contraction profile. U's native path explicitly requests FMA at the corresponding sites; the historical Python reference uses its own explicit FMA path. Analytic Jacobians describe the real extension of the finite-step formula, not a derivative of literal IEEE rounding.

## Native implementation evidence

The compiler, structural graph, standard libraries, total proof kernel and CDC analysis are implemented in U. The self-compilation record binds the tested U source, runtime and toolchain to the repeated generated C and native binaries. Graph checks compare reconstructed U structures to both the native parser and the independent historical reference, with byte-preserved source examples.

The normal execution boundary contains native compiled functions and the disclosed generic platform bridge. Independent Python tools remain test or bootstrap comparison tools. The native analysis tests distinguish path tangents, complete-state recurrence, explicit phase restoration and return operators; a general dense spectrum and complete U1 bindings remain held. Source labels and hashes identify declared data; they are not authenticated provenance or a universal semantic-equivalence proof.

## Asset and experiment boundary

The design documents and original examples retain their preservation record. The repository's design history identifies historical package gaps. Implementation results and benchmarks require revision-specific receipts; their presence is not inferred from a mathematical statement.
