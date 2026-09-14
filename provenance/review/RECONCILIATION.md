# U pre-build review — reconciled result

2026-09-10. Status: proposed refinements after one preliminary Codex review and one Grok review. Original source documents remain unchanged; no U build was executed.

The two reviews support the architectural direction: operations share a composition discipline while retaining their own resource, observation, time, probability, proof, and physical laws. Neither identified a demonstrated contradiction in that center. This supports beginning foundational implementation after its first contracts are made explicit; it is an engineering assessment, not mathematical validation or proof of universal subsumption.

## What the second perspective added

Grok widened Codex's derived-node concern into the more useful requirement: define deterministic elaboration for the whole readable surface into the six-constructor kernel. It also called for visible operator coverage for the 22 examples and direct kernel-level independence cases. These are worthwhile additions. Most other findings refine open work that the dossier already acknowledges in N.4.

## Proposed final refinement round

| Decision to record | Minimum useful requirement | Discriminating check |
|---|---|---|
| Surface → kernel | Version the elaboration rules for definitions, calls, bindings, regions, records and derived operators. Use the six kernel tags; represent a derived operator through `gen` and an immutable expansion/reference-rule dependency. Keep the richer CST/AST where useful. | Repeated elaboration under the same compiler/profile yields identical canonical bytes. Derived join retains its inspectable identity and expansion dependency. A changed expansion invalidates dependent evidence. |
| Theory composition | Every checked mixed-theory graph must carry an admitted composite profile or adapter witness, including its observation, loss and assumption contracts. Unsupported composition is explicit. | The probability/choice ordering counterexample must retain distinct behavior and identity. No unchecked union of effect names establishes a valid composition. |
| Admission and total conversion | Specify the initial total calculus and its trusted checker. Track operator origin, native-support status, proof/assumption status and trust dependencies as separate fields. Freeze interfaces for the first supported slice; inventory the other example operators as specified/unsupported. | A prose descriptor or backend callback cannot manufacture a checked proof. Partial recursion and solver/runtime responses cannot enter total conversion. Conditional results retain their assumptions through composition. |
| Artifact dependencies | Define which canonical objects and profile fields participate in each hash. Keep an acyclic content-addressed object graph, expressing recursive syntax through explicit binders/references rather than circular content hashes. Meaning must not depend on a particular execution receipt. | An implementation change that preserves the entire meaning contract can retain meaning identity while changing realization identity. Changed numeric/observation behavior changes the appropriate semantic profile and invalidates incompatible evidence. |
| Independence | Provide a conservative first witness system for `par`, beginning with disjoint owned resources and explicitly permitted shared reads. Preserve wider theory-specific obligations. | Reuse of one linear token is rejected; independent owned branches pass; a shared read with an overlapping writer fails. Causal dependence never becomes an independence certificate merely because it has been named. |

These decisions can be finalized in the first specification work. They do not require implementing or proving all 22 programs before foundational work starts. Preserve the intended architecture and its later interfaces while making the supported fragment and remaining obligations explicit.

## Qualifications to the reviews

1. **The mixed examples do not establish a contradiction.** Dossier D.3 already gives module manifests responsibility for profile resolution, and J.18 imports a named CDC profile before sequencing its operations. That profile could lawfully encompass the required theories. The missing part is a precise elaboration/admission rule and inspectable witnesses. Surface annotations need not be mandatory everywhere if deterministic inference emits and checks the same explicit graph evidence.
2. **A backend swap is not always meaning-preserving.** Dossier E.5 and O6 bind numeric and observation contracts to semantic identity. The reviews' “backend swap leaves meaning stable” test applies only when those contracts stay fixed. Precision, ordering, timing or observations can make a nominal backend change semantically consequential.
3. **Admission is not one exclusive status enum.** An operator can be a trusted primitive, have some checked laws and some assumptions, and lack a particular backend simultaneously. Keep these dimensions separate. O4 already allows explicit assumptions; prevent their promotion into unconditional claims rather than forbidding all conditional preservation evidence.
4. **An independence witness needs semantics.** Named causal edges can establish dependence; they do not by themselves prove independence. The first witness system must state its read/write, ordering and observation premises.
5. **Recovered examples are now present.** The preliminary report's statement that individual examples are absent was superseded during preservation. All 22 now exist under `handoff/derived/examples/` and match the source hashes in the supplied audit receipt.

## Preservation and remaining source gap

All four Desktop downloads plus the pasted GPT-6 Pro response are preserved byte-for-byte. The 22 derived example files have also been checked against the historical hashes. See `handoff/PRESERVATION.json`.

The supplied folder does not include the referenced grammar (`spec/surface.lark`), runnable audit (`audit/check_design.py`), source-reference ledger (`sources.json`), or the additional structured theory/conformance records. The original generated bundle/ZIP is the fastest way to complete that portion of the handoff. Do not regenerate these and describe them as original files. The original conversation API also clips the long prompt and response; its JSON retains those flags, and the full pasted response is stored separately as supplied. Therefore preservation of supplied material is verified; a lossless export of the whole remote research workspace is not claimed.

Those missing assets do not prevent this preliminary design review. They prevent reproducing the original 40-check audit and claiming the original executable package is fully recovered. No U implementation, CDC parity run, deployment or physical-backend execution occurred in this task.

## Grok receipt

Used the existing grok.com login through the installed native Grok client. Requested alias: `grok-4.6`; reported model: `grok-4.6-build`. Medium reasoning, one model call, no model tool calls requested, normal `end_turn` completion. The client reported 86,819 total tokens and a cost of USD 0.03317074; this is the client's usage record, not an independently reconciled account bill.

The review packet contains the full dossier, full operator contracts, preliminary Codex analysis and preservation/evidence context. Repeated exposition and the duplicate build directive were omitted from that one prompt to limit repetition; they remain intact in the local source archive. Raw final answer and usage fields are retained in `GROK_RESULT.json`; the readable answer is `GROK_REVIEW.md`.

Grok session: `01a089fc-e1cd-7f51-9332-355c8bc69377`. Request: `30bb1a57-e039-4e90-91d6-09597f79c37e`.

The next build should begin from the preserved design plus explicitly accepted refinements, with the package gap visible. This review has prepared that decision and has not executed the historical build directive.
