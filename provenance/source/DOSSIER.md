# U — Law-Bearing Operations
## Foundational language and semantic-substrate design dossier

**Design candidate:** 0.1, September 9, 2026. **Status:** architecture and specification proposal, with executable syntax and mathematical audit probes. This is not a released language, a verified compiler, or a claim of complete foreign-language subsumption.

**Canonical BiDi reference:** `ETEllis/BiDi`, latest `main` observed at `1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157`, merge commit August 8, 2026; CDC release 0.3.0, grammar 1, C ABI 1.5. Exact-head CI run 31252447769 was successful. The source and test definitions were inspected through the authenticated GitHub connection. A container download attempt failed at networking; the repository's verification suite was not independently rerun here.

**Historical statement:** CDC was discovered and built first. This proposal abstracts a more general substrate beneath it afterward. CDC was not secretly implemented on U. A future U–CDC compatibility bridge would be a new, explicitly versioned implementation relationship.

**One sentence:** U makes the laws under which an operation can be composed part of the operation itself.

**Core result:** a small calculus of typed open operations, with explicit mathematical theories, law-bearing transformations, observation boundaries, and residual-preserving imports. A single homogeneous calculus pretending all domain distinctions are ordinary values is rejected.

Source keys `[B...]` refer to the pinned BiDi audit, `[S...]` to public primary sources in `sources.json`. The accompanying files include a concrete grammar, 22 source examples, a conformance matrix, local probe results, and a self-contained Codex build directive.

---

# A. Executive result

## A.1 What U is

U is a language of **law-bearing operations**. An operation has an interface, a semantic theory, an account of the resources and observations it permits, and rules governing its composition. Its implementation is a separate, evidence-bearing realization of that meaning.

The distinction is the design's center:

1. **Meaning:** what behavior, relation, proof object, channel, or trajectory the operation denotes.
2. **Realization:** how a particular evaluator, solver, compiler, device, or observer implements or examines it.
3. **Evidence:** what has actually been established about that realization and its output.

A quantum channel may have a classical simulator realization without becoming a classical physical operation. A differential equation may have a finite-step numerical realization without becoming a finite sequence of samples in its denotation. A program may have an executed trace without that trace replacing its set of possible behaviors. A proof search may produce a candidate without that candidate becoming a checked theorem.

U is feasible as a programming language, executable semantic representation, and typed inter-language substrate. It is **not** feasible as a promise that every admitted mathematical description has an exact terminating evaluator, or that arbitrary imported languages have an automatic complete equivalence checker. Those promises are excluded from the language contract, not deferred to a faster compiler.

## A.2 What is actually small

The proposed structural calculus has six constructors: wiring, an admitted generator, sequential composition, independent composition, binding/scope, and theory-qualified iteration. Its mathematical content is NOT six magical keywords. Seven explicit theory clusters supply the distinct laws that the structural calculus must not erase:

- constructive values and proof terms;
- resources and machine memory;
- relations, constraints, and search;
- measures and probabilistic conditioning;
- causal behaviors and clocks;
- continuous paths and differential constraints;
- quantum channels and instruments.

These are seven **obligation clusters**, not a proved globally minimal axiomatization. Their subdivisions, generators, laws, and trusted implementation costs must be counted. Encoding all of them in one `operator` callback does not reduce the semantic basis.

The central unification is therefore not “everything is a function,” “everything is a relation,” or “everything is a rewrite.” It is:

> Every computation is an operation at an interface; what that interface permits us to compose, observe, duplicate, identify, or forget is part of its meaning.

This gives the source languages family resemblance without equating a probability measure with a search tree or a clocked register with a heap variable.

## A.3 What CDC contributes

CDC's unusually valuable contribution is not that ternary arithmetic somehow contains every other computation. It is the integration of transformation, admissibility, source identity, state, receipts, and earned analysis boundaries. U generalizes those mechanisms without making CDC's particular prefix barrier, cover geometry, or phase dynamics universal axioms.

“Transformation with earned consequence” is a useful compression of CDC's **guarded effect and analysis boundary**. It is not a literal characterization of every native step: `flow` and `nest` transform state without independently evaluating the prefix barrier, and a held commit does not globally reverse past effects. `[B03,B20,B21]`

U's deeper identity is **composition with explicit semantic obligations**. CDC specializes which transformations and consequences are admissible. U2 becomes an especially strong example of a law-bearing transformation: differentiation of an executed operation, followed by a separate recurrence obligation before a return operator is exposed.

## A.4 Honest outcome

The dossier provides an implementable architectural candidate, an explicit preservation test, concrete examples, and negative cases that defeat weaker designs. It does not establish that all eighteen source languages are already natively subsumed, that the theory basis is mathematically minimal, or that every cross-theory combination has a compositional model. Those become individually named conformance and proof obligations.

The strongest achieved result is a coherent **minimum-plural architecture**: common composition and evidence, with irreducible domain law distinctions retained. The remaining novelty claim is a research hypothesis to validate against existing semantic frameworks—not a declaration that typed graphs, effects, dependent types, or categorical composition have just been invented.

# B. Naming and prior-art audit

## B.1 Naming verdict

**U and `.u` are not clean names.** The following are direct, material conflicts, not search-engine noise:

| Collision | Evidence | Consequence |
|---|---|---|
| Unison source files | Official quickstart creates and edits `scratch.u`. `[S01]` | Existing contemporary source-extension collision. |
| Ü programming language | Project source examples include `hello_world.u`, `fibonacci.u`, async examples and more. `[S02]` | Same ASCII-adjacent spoken/typed name and exact extension. |
| A separately published U language reference | Public reference calls the language U and shows `hello.u` and other source examples. `[S03]` | Exact language-name and source-extension collision. Its implementation/performance claims were not validated in this audit. |
| Esolang U entries | U disambiguation names multiple independently proposed U languages. `[S04]` | Exact historical/esoteric naming collision. |
| UnrealScript packages | Official UDK material refers to classes packaged in `.u` files, distinct from `.uc` source. `[S05]` | Existing binary/package format collision; automatic text association is unsafe. |
| MIPS/Digital compiler ucode | Archived original Digital compiler documentation explicitly describes `.u` intermediate object files. `[S06]` | Historical IR/object-format collision directly relevant to this project's role. |
| Icon toolchain shorthand | University of Arizona's original documentation permits `.u` shorthand for ucode inputs. `[S07]` | Additional historical ecosystem collision; not an assertion that all Icon versions use one identical format. |
| PyPI `u` | Public registry entry is occupied. `[S08]` | Unqualified package naming must not be assumed available. |

Exact-phrase searches for “Universal Operation Language” and “Universal Operator Language” did not produce a conclusive authoritative clearance. That is **not evidence of availability**. npm and crates registry checks were inconclusive through the available access path; no availability conclusion is made. HDL, theorem-system, IR and format searches did not establish an exclusive namespace or a registry guaranteeing this extension. An exhaustive historical negative cannot be proved by these searches.

USPTO and WIPO search portals were inspected as clearance entry points; no complete live trademark search, jurisdiction/class comparison, common-law analysis, or professional legal clearance was completed. `[S09,S10]` A public name release must check software goods/services classes, similar spellings and sounds, relevant jurisdictions, package/CLI conflicts, editor language registrations, and corporate/project uses. This audit establishes practical naming conflicts, not a legal infringement conclusion.

**Decision:** retain **U as a working title**, qualified as `ETEllis/U`, with language identifier `etellis.u`. Preserve `.u` inside explicitly opted-in workspaces using the header `u "etellis.u/0.1";`. Do not claim it is available; do not globally override Unison or other `.u` associations. Use a qualified provisional executable name such as `etellis-u`, with optional workspace-local `u` alias. The qualified names also require a pre-release check; they are proposals, not registrations. Choose a distinctive public brand before 1.0 unless a deliberate collision-tolerant distribution policy is accepted.

## B.2 Closest conceptual prior art

| Prior art | What it already supplies | What U must add or test rather than relabel |
|---|---|---|
| MLIR | Extensible typed operations, regions, dialects, common textual/serialized structure, progressive lowering. `[S11]` | Semantic laws and preservation receipts as user-visible language contracts; residual reconstruction; mandatory native-possession tests. A dialect tag alone fails. |
| K | Executable language semantics and reusable formal tooling. `[S12]` | A usable object language and verified composition of admitted semantic interfaces, not merely defining another language in K. |
| PLanCompS/CBS funcons | Reusable fundamental constructs and modular operational semantics; explicit distinctions in how stores, contexts and control flow. `[S13]` | A genuinely wider domain scope and evidence/realization/residual architecture, with comparisons rather than claims of firstness. |
| Bigraphs | Nesting/locality independent of linking, reactive rules, common treatment of process calculi. `[S14]` | Rich numeric, resource, proof, probabilistic and quantum distinctions; CDC's hierarchy is not an invention of nested graphs. |
| Structured/decorated cospans | Composition of open systems across interfaces; circuits, Markov systems, Petri nets and dynamics. `[S15]` | Exact operational interfaces, executable checking, source ingestion, and explicit restrictions where a categorical composition does not exist. |
| Algebraic effects/handlers | Delimited interpretation of effects and explicit control behavior. `[S16]` | Physical-resource and semantic-law boundaries that a handler cannot silently rewrite. |
| Dependent/linear type systems | Proof terms, indexed objects, no implicit duplication of linear resources. `[S17,S18]` | A particular integrated checker, interoperability discipline and trusted-extension policy—not a new discovery of these concepts. |
| Modelica and synchronous modeling | Acausal differential/algebraic equations, clocked and event semantics. `[S19]` | Preservation of those laws through a common representation and explicit realization receipts. |
| OpenQASM | Quantum operations, measurement, timing and classical control. `[S20]` | Quantum channel semantics must remain quantum; neither a generic function type nor a simulator establishes physical execution. |

This audit rules out a credible claim that “a common typed graph with multiple dialects” is itself U's original discovery. The potentially valuable contribution is a **law-bearing and residual-bearing composition protocol** implemented as an approachable language, including CDC's stronger earned-artifact discipline. Novelty remains to be evaluated on that precise contribution.

# C. First-principles derivation

## C.1 Start with distinctions, not syntax

To describe an operation, we must identify what can interact with it and what differences its environment can observe. This requires an interface and some account of admissible behavior. We then need to connect operations, compose independent operations, bind local names without accidental capture, and represent repeated or recursive interaction.

That derivation does not initially require classes, loops, variables, files, threads, or even a total function. A join is an operation at relational interfaces. A register is an operation at clocked signal interfaces. A proof is an operation/object admitted by a logical judgment. A quantum instrument maps a joint quantum system to outcome-indexed quantum states and classical records. Their shared structural role is real; their mathematical laws remain different.

## C.2 Three counterexamples determine the architecture

**Copying.** A reusable integer supports a diagonal map `copy(x)=(x,x)`. A general unknown quantum state cannot simply inherit that operation. Therefore structural wiring must not imply universal contraction. Resource usage belongs in interfaces, not in a late lint pass.

**Choice.** Choose a bit and then compare it with an independent fair coin: maximal success probability is 1/2. Observe the coin first and then choose a matching bit: maximal success is 1. A model that freely interchanges nondeterministic choice and probability has changed meaning. Therefore effect/theory composition needs an interaction law and an observation policy, not a union of names.

**Time.** CDC's finite map with fixed source phase π/2 and unit coupling gives `F_d(x)=x+d cos(x)`. Then `F_1(F_1(0))=1+cos(1)≈1.5403023`, whereas `F_2(0)=2`. Consequently a generic flow law `flow(a);flow(b)=flow(a+b)` would be unsound for current CDC, even when a suitable autonomous exact ODE flow admits that law. Therefore realization and law profile must survive lowering. This is a property of the actual implemented map, not a defect to “repair” during import. `[B21]`

These probes are reproduced in `audit/check_design.py`.

## C.3 Structural minimality versus semantic minimality

A language can syntactically encode all six structural constructors as graph substitution, or encode all syntax in one lambda term. Such reductions say little about native compression. U therefore separates:

- **structural description length**: grammar and graph combinators;
- **semantic specification cost**: generators, models, laws and rule premises;
- **representation cost**: nodes, types, residuals and required annotations;
- **implementation cost**: trusted evaluator/solver/backend surface;
- **human cost**: learning and reading the representation.

The optimization target is a Pareto frontier over these costs, not a magical scalar primitive count. The proposal's six constructors are an economical readable basis. Independence arguments show why deleting certain *roles* loses useful native distinctions. They do not prove that no other five-constructor presentation can encode the same structure.

## C.4 Required theory distinctions

The minimum useful plural architecture must retain at least these distinctions: reusable versus linear data; relation versus evaluation strategy; measure versus nondeterministic possibility; clocked/causal behaviors versus a single sequential trace; exact continuous trajectory specification versus a numerical step map; checked proof versus executed computation; coherent quantum channel versus classical mixture.

They can share structure, and some can be encoded in more general mathematics. But moving a distinction into a type refinement, law set, or interpreter does not eliminate the distinction or its cost. U counts it wherever it lives.

# D. Semantic kernel

## D.1 Abstract syntax

For an admitted theory environment Θ:

```text
P ::= wire[permutation]
    | gen[theory_id, operator_id](static_parameters; regions)
    | seq(P, P)
    | par(P, P)
    | scope(name : NameKind, P)
    | fix[iteration_mode](bound_operation, P)
```

An operation has an ordered input interface A and output interface B. Regions carry explicit parameter binders, result interfaces, stage, effect bounds and theory constraints. Generator names resolve by immutable signature digest; a textual name is not an executable callback slot.

The default structural model is a free, typed symmetric-monoidal composition fragment with binding. It does **not** assert that every semantic theory has every feedback, closure, discard or copying operation. A theory supplies the models for the constructors it admits and records restrictions on their use. Acausal connection uses a distinct admitted connection/constraint operation when it is not representable by directed serial wiring alone.

## D.2 The six constructors

| Constructor | Meaning | What it must not silently mean |
|---|---|---|
| `wire(π)` | Permute or forward existing typed ports. | Allocate, copy, discard, cast, observe, or reorder effects. |
| `gen(T,o)` | Apply one explicitly specified generator of theory T. | “Call any foreign interpreter and call it native.” Every generator's full semantic cost is counted. |
| `seq(p,q)` | Connect compatible outputs of p to inputs of q. | Assume incompatible domains or time bases are interchangeable. |
| `par(p,q)` | Place operations alongside one another with a declared independence/resource-splitting judgment. | Assert physical simultaneity, causal independence, or commutation merely because syntax is parallel. |
| `scope(n,p)` | Bind a fresh nominal name/interface boundary with capture avoidance and escape restrictions. | Physically allocate memory/qubits or reset global state. Allocation is a theory operation. |
| `fix[m](x,p)` | Iterate under a named admissible fixed-point/feedback discipline. | Offer unrestricted recursive proof terms, instantaneous cyclic circuits, or arbitrary quantum trace feedback. |

Wiring, `seq`, and `par` can be subsumed by a general wiring-graph constructor, reducing syntax but not meaning. Scope is not reducible to substitution without freshness/binding machinery. Iteration is not available in a finite acyclic graph without adding an equivalent recursion/feedback mechanism. Generator semantics cannot be obtained from connectivity alone.

## D.3 Judgment and structural rules

Use the judgment:

`Θ; Γ; Δ ⊢ p : A ⟶[T, ε, u, stage] B`

Γ contains reusable assumptions and values; Δ contains affine/linear resources. T identifies an admitted semantic profile; ε is the effect interface; u is a resource-usage map. The module manifest resolves imported definitions, numeric modes, clocks, trust roots and observables.

Sequential composition requires the same boundary type or an explicit adapter:

```text
p : A ->[T, e1] B     q : B ->[T, e2] C
------------------------------------------------
seq(p,q) : A ->[T, compose_effects(e1,e2)] C
```

Parallel composition requires a proved or checked split, not merely two well-typed branches:

```text
Δ = Δ1 * Δ2    Independent_T(p,q, shared_readonly)
p : A -> B under Δ1     q : C -> D under Δ2
------------------------------------------------
par(p,q) : A tensor C -> B tensor D
```

Shared read-only resources require explicit sharing witnesses. Disjoint physical address ranges, causal channels, logical variable identity, and device lanes cannot be inferred from textually different names alone.

For partial computational fixed points, the operational equation is `fix x.p -> p[fix x.p/x]`; its denotation needs the admitted partial-computation model. For monotone relational least fixed points, monotonicity is checked or assumed explicitly. For synchronous feedback, every cycle must satisfy the declared causality/delay condition. Total proof recursion uses structural or well-founded elimination, never unrestricted `fix_partial`.

## D.4 Laws

Within an admitted profile: identity wiring and sequential associativity; associativity/unit/coherence of independent composition; alpha-renaming of bound names; capture-avoiding substitution; qualified unfolding of fixed points. Interchange,

`(p tensor q);(r tensor s) = (p;r) tensor (q;s)`,

requires the independence judgments that make both sides legal. No blanket reassociation of IEEE arithmetic, reordering of quantum gates, exchange of probability and search, or conversion of an effectful computation to a pure relation follows from these structural laws.

## D.5 Explicit semantic theory ledger

| Cluster | Minimum owned semantic obligations |
|---|---|
| V — values, types, proofs | Universes; dependent products/sums; inductive data; equality and eliminators; closures/application; strict/non-strict profiles; partial-computation boundary; typed syntax objects and staged substitution. |
| M — resources/memory | Allocation identity; byte layout; ownership/borrowing; accesses and deallocation; address/provenance distinction; atomics/order; machine-state/ABI refinements; explicit unsafe/undefined regions. |
| R — relations/constraints/search | Logical variables and substitution; unification with term-universe mode; conjunction/disjunction/existentials; multiplicity; ordered clauses and cut/search delimiters where admitted; monotone least fixed points; solver result conditions. |
| P — measure/probability | Measurable carriers; kernels and unnormalized measures; measure bind; sample identity; density/likelihood weighting; normalization preconditions; conditioning; explicit inference realization and approximation evidence. |
| C — causality/behaviors/clocks | Events and partial order; messages/mailboxes; enabled transitions; behavior sets; failures and supervision; fairness; clocked signal/register state; device execution geometry; explicit schedule refinement. |
| D — trajectories/differential constraints | Continuous path spaces; ODE/DAE equations, domains and initial conditions; event/reset constraints; derivative eligibility; topology/frame data; numerical realization and error contracts. |
| Q — quantum processes | Joint quantum interfaces; unitary/isometric channels; completely positive maps/instruments; tensor composition; measurement and reset; timing/classical feedforward bridges; physical realization distinction. |

The concrete initial signature/rule seed is in `spec/OPERATOR_CONTRACTS.md`. This ledger is deliberately not a primitive count of seven. Implementation must publish each generator, typing rule, law, model, algorithm and assumption beneath these headings. For example, claiming all of Lean because a `Proof` type exists is prohibited.

## D.6 Semantic models

For each profile T, supply `Model(T)`, interface interpretation, operation interpretation and observables. Pure total V operations may denote functions; partial V computations may denote partial maps. R may denote relations, weighted relations or ordered answer-stream semantics depending on profile. P denotes measures/kernels with integrability conditions. C denotes event structures or sets of behaviors. D denotes admissible trajectories/equations. Q denotes channels/instruments on joint systems.

`[[seq(p,q)]]_T = [[q]]_T composed_with [[p]]_T` and an analogous parallel law must hold. Cross-theory operations require an admitted composite profile or explicit adapter. There is no automatic “sum all effects” theorem.

# E. Types, effects, resources and identity

## E.1 Kinds and logical trust

Core kinds include `Type_i`, `Prop_i`, `Interface`, `Theory`, `Region`, `Clock`, `Shape`, `Stage`, and `Capability`. Universes are stratified: no `Type : Type`. Type normalization is total; unbounded search, IO, general recursion, and unchecked foreign evaluation cannot participate in definitional equality.

Proof terms and programs share syntax and selected constructors, not unrestricted evaluation rights. Tactics return candidate proof terms; the U proof kernel checks them. Lean/Rocq may mechanize U's metatheory or provide imported candidates, but a delegated success string is not U proof support. Foreign axioms are explicit assumptions in the theorem's dependency set.

## E.2 Usage and resources

Usage is tracked using an explicit algebra with linear, affine and unrestricted capabilities; the first implementation may use grades 1, at-most-1 and ω, plus region/lifetime constraints. Copy and Drop are verified interface capabilities, not universal defaults. Linear quantum resources do not acquire Copy through user-written trait instances. Borrowing temporarily transfers a restricted access right while suspending incompatible owner access; ending the lexical scope does not itself prove that an escaped reference is safe.

A borrow and an authority lease remain different types. A borrow constrains aliasing, access and lifetime to memory or another resource. A BiDi-style lease authorizes an action under issuer, frame, horizon, expiry, epoch and causal conditions. Both use scoped capabilities, but neither can substitute for the other.

Dynamic values are existential packages with runtime type witnesses, not permission to inspect every carrier. Reflection over a classical function description does not expose a physical qubit's unknown state or unseal an opaque proof constructor.

## E.3 Effects

Effects are typed operations on explicit capability interfaces, not strings in a global set. Handlers can interpret an abstract effect only under the declared semantic contract. A simulation handler cannot satisfy a physical-device execution capability. A pure optimizer may erase an effect boundary only with a witness that the erased boundary is unobservable under the selected profile.

Atomicity is scoped and graded: memory-atomic, local durable transaction, device command, stream write, distributed protocol action. These are not one universal transaction primitive. External IO can fail after partial progress; a correct result type must include indeterminate completion when the protocol cannot establish whether an effect occurred.

```text
Outcome(A) = Done(A, receipt)
           | Held(obligations, earned_artifacts)
           | Rejected(diagnostic)
           | Fault(error, known_progress)
           | Indeterminate(recovery_obligation, known_progress)
```

Divergence belongs to computation semantics, not a value a runtime can always detect. A finite budget interruption may return Held at a safe boundary without identifying a mathematical divergence.

Held preserves the protected application state for that refused operation. It may append an audit record, consume an attempt identity, or advance control-plane causal history. It never implies retroactive rollback of earlier effects. Parse/type errors remain errors; unsupported realizations are explicit refusals, not fabricated execution receipts.

## E.4 State model

A reference configuration is `K=(graph, bindings, stores, resource_ledger, event_structure, continuation, evidence_context)`, interpreted under T. Stores are indexed carriers, not a promise to serialize all domains into a classical heap. A quantum simulator may maintain a joint density operator; hardware execution maintains an opaque device resource and receipts instead. A DAE specification maintains equations/path constraints before a solver provides numeric approximants.

Reduction has the shape `K --label--> K'`, where labels carry observables and relevant effect data. A generator's rule comes from its checked theory descriptor. Context rules step sequential regions left-to-right in the ordinary strict profile; declarative, quantum, reactive and probabilistic regions obey their explicit interpretation. Race freedom or deterministic execution is not inferred merely from graph shape.

## E.5 Identity and provenance

Keep at least six identities separate:

| Identity | Binds |
|---|---|
| Original-source identity | Exact imported bytes and source environment. |
| Canonical-syntax identity | Parsed normalized syntax, with resolved syntax-version rules. |
| Semantic-artifact identity | Typed graph plus profile, dependency hashes, assumptions and numeric/observation contracts. |
| Execution identity | Artifact, inputs, scheduler/randomness/external observations and realization. |
| Replay identity | Declared projection of a history; possibly independent of storage layout. |
| Resource identity | Nominal allocated entity, epoch or device resource—not its present contents. |

No hash decides general semantic equivalence. “Semantic-artifact hash” means identity of a canonical representation and contract, not identity of every extensionally equivalent program. A digest is not an authentication signature. A signed receipt is not automatically truthful. A commitment to discarded history is not reconstructible history. CDC's store makes this last distinction explicit. `[B44]`

## E.6 Evidence

Use the requested shape `(scope, maturity, verdict, receipt-or-obligation)`, extended with assumptions, checker/runtime identity and dependencies. Maturity is not a single ascending scalar. Specified, executed, adversarially checked, mechanized-finite and exploratory/open can coexist for different claims about one component. A proof of a finite mirror does not subsume its C implementation; a hardware job receipt does not prove quantum advantage.

Only trusted constructors create `CheckedProof`, `VerifiedRecurrence`, `AdmittedEffect`, or `AuthenticatedEnvelope`. Arbitrary JSON can describe a claim; it cannot instantiate the corresponding authority-bearing type without validation.

# F. Execution model

## F.1 Discrete and machine execution

The reference V evaluator performs explicit beta reduction/application, inductive elimination and named numeric operations. Partial recursion has a visible effect and unfolding rule. A bytecode or native compiler must implement the same contract, including integer overflow, IEEE order, exceptions, laziness and observable allocation where those are part of the profile.

M adds a typed store and resource ledger. Allocation yields a fresh nominal allocation identity; an address is a representation within an ABI, not the allocation's whole identity. Loads/stores specify width, alignment, provenance, volatility and ordering. Atomics contribute events constrained by the selected memory model. An ISA profile additionally binds registers, flags, address space, traps/interrupt interfaces and allowed external transitions. RISC-V provides a useful first exact ISA target, not a universal assumption about all machines. `[S21]`

Imported undefined behavior is not silently converted into safe U behavior and called equivalent. A safe trapping translation is a separately labeled refinement. Equivalence claims apply to a stated defined-behavior fragment or a profile that explicitly represents the source's undefined/unspecified behavior.

## F.2 Concurrency and distribution

A concurrent program denotes possible event structures/behaviors. Each executed run selects a schedule and records enough non-deterministic input to replay the selected behavior. Deterministic replay does not establish equivalence of all schedules.

Message send introduces an event and causal edges. Receive obeys a profile's mailbox discipline, selective-receive policy, ordering guarantees and failure semantics. Supervisor restart is a derived protocol over spawn, link/monitor, failure and state-initialization operations; it is not a magical scheduler keyword. Distributed failure detectors and leases expose clock/freshness assumptions. A network partition does not inherit availability, consensus or exactly-once effects merely because local execution is deterministic.

## F.3 Time

The time context contains typed axes rather than one universal scalar:

`TimeContext = (causal partial order, event indices, clock domains, physical/simulation time maps, observation windows, trace serialization order)`.

A conversion between clock domains needs a map, synchronization assumption or error interval. Causal order, wall-clock time and printed trace order need not coincide. A CDC phase coordinate is also not wall-clock time. CDC's scheduler deliberately excludes local admission `now` from replay identity while retaining authenticated deterministic observation times. `[B43]`

## F.4 Continuous and hybrid semantics

For an ODE specification, the meaning is a set of admissible paths:

`Sol(f,x0,D) = {x:[0,T]→X | x(0)=x0, x(t)∈D, dx/dt=f(t,x(t)) under the declared regularity notion}`.

For a DAE, `F(t,x,dx/dt)=0` adds algebraic constraints; initialization, consistency, index and uniqueness assumptions remain explicit. A solver realization returns a trajectory approximation with method, tolerances, accepted steps, event-localization contract and error evidence. A local tolerance setting by itself is not a theorem bounding global error.

Hybrid systems combine these path segments with mode-indexed guards/resets and a declared simultaneous-event policy. A scheduled reset has derivative `D R`. A localized transverse guard event may need a saltation term:

`S = DR + ((f_plus - DR f_minus - partial_t R) n^T) / (n^T f_minus + partial_t g)`.

Missing event localization, grazing, changed itinerary or unsupported delay history must hold/refuse the requested derivative. Current CDC source execution is a finite-step hybrid specialization, not an implementation of every path operation above. `[B34,B35,B37]`

Native continuous **semantics** means retaining the equation/path operator explicitly and analyzing it as such. It does not mean a digital machine exactly executes arbitrary real-valued dynamics. Both specification-only and numerically executable profiles are first-class and visibly different.

## F.5 Relations, constraints and queries

For a pure relation, conjunction composes constraints and existential quantification hides a logical variable. Native unification operates on typed terms/substitutions, with explicit finite-term or rational-tree semantics. Repeated use of one logical variable preserves its identity; it does not allocate independent unknowns.

A relational denotation and a search procedure are separate. Prolog clause order, leftmost selection, depth-first search, cut and rational-tree behavior may affect observable answers and termination; an import that retains only the ideal relation loses those semantics. Datalog least-fixed-point evaluation is another explicit policy, not Prolog with a faster loop. SQL bags use multiplicities; NULL truth, collation, grouping, order and transaction isolation are part of the source profile. `[S22,S23]`

## F.6 Probability and uncertainty

For a kernel K and continuation kernel L, measure composition is

`(K bind L)(x,A) = integral L(y,A) K(x,dy)`.

Conditioning/likelihood weighting produces an unnormalized measure `mu'(dy)=likelihood(y) mu(dy)`; normalization requires `0 < mu'(whole) < infinity`. Undefined normalization produces a typed obligation, not an arbitrary posterior. Logical unknowns, stochastic variables and nondeterministic alternatives remain distinct carriers.

An inference algorithm is a realization of a probabilistic query. HMC, importance sampling, variational inference and exact finite enumeration have different guarantees and residual error accounts. A seed controls a pseudorandom realization; it does not prove that the algorithm samples the target exactly. Stan-style constrained parameters also require the transformation/Jacobian and target-density conventions in the semantic profile. `[S24]`

## F.7 Hardware and accelerators

A register denotes a clock-indexed state relation: outputs at tick k expose the pre-update value, and a next-state function determines k+1 under the declared clock and event-region semantics. A full SystemVerilog profile must distinguish two-state/four-state values, X/Z propagation, scheduling regions, nonblocking assignment, initialization, assertions and synthesis-admitted subsets. The included counter chooses a narrower two-state synchronous profile. `[S25]`

A GPU operation carries grid/block/lane geometry, address-space and synchronization scopes, memory visibility, divergence and resource constraints. A partitioned output lease can justify disjoint lane writes, but generic parallel syntax does not prove that a barrier is reached uniformly or that a shared-memory race is absent. The first reference GPU realization may be a semantic simulator; actual device execution requires its own gate and receipt. `[S26]`

## F.8 Quantum semantics

A finite quantum interface denotes a joint Hilbert-space/density-operator carrier. A unitary operation is the channel `rho -> U rho U†`. A measurement instrument is a family of completely positive maps `{M_i}`; the outcome probability is `Tr(M_i(rho))`, and the normalized conditioned state is defined only when that probability is positive. Reset, measurement and discard are not reversible unitary functions.

Quantum tensor composition acts on joint systems and preserves entanglement. Classical descriptions of circuits may be copied; unknown quantum states may not. Classical feedforward requires an explicit measurement/control bridge. QASM timing, calibration and hardware layout are additional contracts, not comments erased before a hardware claim. `[S20]`

The finite Bell probe in this package shows why preserving only computational-basis probabilities fails: a Bell state and its dephased classical mixture agree in ZZ probabilities and disagree after an XX-basis observation. The probe is a classical matrix calculation, not a physical experiment.

## F.9 Cross-theory composition

An adapter is a first-class contract:

`Adapter(T,S) = (interface_map, semantic_relation, observation_map, assumptions, loss_account, checker, realization, residual_policy)`.

Examples: measurement from Q to classical C/P; sampling from P to an executed C trace; numerical solution from D to finite M/V arrays; CDC phase quantization from D/M state to a trit V carrier; database snapshot from M/C to R; syntax quotation from an operation graph to classical V code.

An adapter can be exact, refining, approximate, residual-preserving, or unsupported. U rejects composition when no admitted adapter exists. The absence of a canonical distributive law between two theories is a design boundary, not a missing convenience cast.

# G. Source syntax and representations

The canonical human source is ordinary UTF-8 text with the small grammar in `spec/surface.lark`. It uses definitions, typed regions, calls, lists, tuples, records and sequencing. Domain families contribute resolved operators, not custom parsers. The examples deliberately avoid separate Prolog punctuation, SQL strings, shader strings or embedded quantum assembly.

```u
u "etellis.u/0.1";
use "u.standard/0.1";

def square(x: Int) -> Int = int.mul(x, x);
```

This is a projection of a typed operation graph, not a claim that graph internals must be exposed in every beginner program. The ordinary explanation is: **name an operation, state what it accepts, connect it to another operation**. Effects, advanced profiles and proof obligations become explicit as they become relevant.

Optional mathematical/infix aliases must be reversible, name-resolved projections. For example, a displayed `p ; q` maps to `seq(p,q)`, `p tensor q` or `p ⊗ q` to `par(p,q)`, and an arrow has an ASCII spelling `->`. A plus sign cannot resolve to exact integer addition, IEEE addition, bag union and quantum superposition without the type/profile determining which operation it denotes.

Graphical projections may expose ports, topology, clock domains, causal edges and scope nesting. They must retain stable node identity and round-trip to the canonical serialization. Layout information can be a residual; geometry cannot silently replace semantic connectivity.

**Ergonomic risk:** fully qualified calls can become verbose, especially in relation/proof construction. This version has a coherent grammar, not a demonstrated learnability victory. Before 1.0, test reading and editing tasks with beginners and specialists; compare task completion, semantic error rate, token density and ability to explain an operator's domain. Introduce reversible infix/pipeline projections only after those tests, not another eighteen grammar islands.

# H. Native subsumption test and language matrix

## H.1 A formal, non-Turing-completeness criterion

For a source language L with version/environment profile kappa, define:

`E_(L,kappa): Programs(L,kappa) -> UEnvelope`

`E(P) = (G, R, Pi)` where G is a typed U graph, R a residual, and Pi provenance plus preservation obligations.

For the claimed fragment F and admitted contexts C:

`Obs_L(P,C) approx_(domain,epsilon) project_L(Obs_U(G,E(C)); R)`.

The equivalence is domain-specific: contextual equivalence, bisimulation, trace-set equivalence, state/ABI correspondence, measure equivalence, proof/type preservation, cycle equivalence, query multiset equivalence or quantum-channel equivalence. Numerical closeness uses an explicitly named norm and error budget; “within epsilon” is generally not a transitive equivalence relation.

**Lifting** should preserve source meaning, including sets of allowed behaviors. **Backend lowering** may instead refine nondeterminism by selecting allowed schedules or implementations. These are different theorems. An executed example is evidence for neither theorem in full generality.

The projection is a fixed observation map, not a second source-language evaluator. It may read retained observable metadata, but cannot execute P hidden in the residual. Otherwise an empty graph plus the original source would vacuously satisfy the equation and defeat the experiment. Full abstraction against all unrestricted U contexts is a stronger, separate obligation; the initial theorem quantifies only over the explicitly admitted source-corresponding contexts.

A source operator earns **derived but native** status only when all of the following are satisfied:

1. Its operator and domain distinctions remain explicit in the typed canonical graph.
2. Its translation is locally compositional, not a source-program interpreter invocation.
3. It has a declared denotation/rule or checked U-native expansion and an inspectable law set.
4. The U evaluator/analysis machinery operates directly on these operators, not opaque source strings/ASTs hidden inside an L VM.
5. Observations, resources, failure behavior and relevant intensional distinctions can be recovered or are declared residuals.
6. It has operator-specific positive, negative and differential conformance tests.
7. Its representation and semantic-engine cost are recorded; hidden interpreter debt is not excluded from the calculation.

A bounded-size image for each source AST node is useful but insufficient: a constant call to a huge foreign interpreter also has small syntactic size. Count source-shaped runtime machinery and required semantic axioms separately. Shared algorithms such as unification, a query planner or a Schur solver are not automatically forbidden; they qualify only when they implement explicit U operators with inspectable contracts rather than burying the source semantics.

## H.2 Proposed support matrix

**Legend:** P = theory primitive (not a new structural keyword); D = derived but native; L = library abstraction; B = specialized backend; F = foreign interoperability; R = residual-preserving import; U = unsupported. These are proposed architectural placements. No row below is already implemented as a complete U frontend. Whole-language imports must start with an honest fragment manifest.

| Source | Distinctive operators and U placement | Required preservation / residual boundary |
|---|---|---|
| ISA/Assembly | M-P exact registers, byte accesses, atomics, traps; C-P external events; B target instruction realization. | State/trace bisimulation under exact ISA, privilege, memory and interrupt environment. Instruction bytes, labels, debug info and platform conventions retained. Unknown opcodes U/R, never guessed. `[S21]` |
| C | M-P allocation/layout/pointer access and alias model; D structured control; B ABI; F foreign calls. | Defined-behavior observational equivalence with compiler/dialect/ABI/profile fixed. Preprocessor/source layout R; volatile/MMIO is semantic, not cosmetic. Undefined behavior cannot be silently “fixed.” `[S27]` |
| Rust | M-P resource/borrow judgments; V-D traits/generics/patterns; B drop/unwind realization. | Preserve ownership/lifetime acceptance and observable drops/panics, then memory behavior for admitted unsafe model. Current Rust documentation does not claim a complete settled aliasing/unsafe semantics; safe subsets and pinned models are mandatory. `[S18]` |
| Python | V-P existential dynamic values; V-D attribute/metaclass/descriptor dispatch and code introspection; C-D orchestration; F native extensions. | Versioned data-model and exception behavior; source/code-object/frame observations retained as semantic metadata where observable. Full native C-extension compatibility remains F/B, not native Python subsumption. `[S28]` |
| JavaScript + TypeScript | C-P jobs/continuations; V-D prototypes/dynamic object behavior; V-D structural/type-level transformations; B host loop. | Preserve JS job order, coercion, property descriptors and host contract. TS type erasure, assertions and checker acceptance are separate from logical soundness; types/source R. DOM/browser environment is not supplied by ECMAScript alone. `[S29,S30]` |
| Lisp / Racket | V-P typed syntax/scopes and staged substitution; D quote/splice and hygienic macros; D language elaborators. | Preserve binding/phase behavior, evaluation strategy and observable syntax. Racket syntax-object properties and source positions may be R or active semantic metadata. Unrestricted legacy eval is not admitted in proof conversion. `[S31]` |
| Haskell | V-P functions, inductive sums/products; D non-strict thunks/call-by-need; D effect composition; L abstractions. | Preserve bottom, strictness, `seq`, sharing where observable, and IO order. Replacing lazy evaluation with eager evaluation fails. Typeclass/coercion/newtype details need a pinned frontend profile. `[S32]` |
| Prolog | R-P variables, unification, goals and answer streams; D resolution; R/C-P scoped cut and search strategy; L constraint algorithms. | Preserve answer/termination/order behavior for chosen SLD/rational-tree/occurs-check mode, not just the mathematical relation. Foreign predicates F; residual exact clauses retained. `[S22]` |
| SQL | R-P bags, projection, join, NULL truth and constraints; D grouping/aggregation; M/C-D transactions; L optimizer. | Multiset/query equivalence, collation/numeric/null behavior, declared order and isolation. Engine extensions/UDFs F/R until modeled. Query planning must retain only justified algebraic rewrites. `[S23]` |
| APL | V-D indexed arrays, shape/rank/cell operators, scan/reduce/contraction; B vectorized execution. | Shape/rank/prototype/empty-array rules, scalar extension and evaluation/numeric mode. A NumPy-style broadcast approximation is not automatically APL equivalence. `[S33]` |
| Stan | P-P target measures/conditioning; V/D-D constrained transforms and AD; L inference; B numeric solver. | Density and transformation-Jacobian correspondence; realized inference carries approximation and diagnostics, not exact-posterior claims. RNG/inference settings and ordering survive. `[S24]` |
| Erlang | C-P isolated processes, message delivery/receive and failure; D links/monitors/supervision; B distribution. | Mailbox ordering/selective receive, process identity, exit propagation and restart behavior. Real network partitions/failure detection need actual distributed gates. `[S34]` |
| TLA+ | C-P behavior sets, actions and observations; D temporal modalities/fairness; L finite model checking; V-D proofs. | Stuttering and temporal satisfaction preservation over behavior sets. One run or bounded exploration is not an unbounded liveness theorem. `[S35]` |
| Lean | V-P universes, dependent types, equality/inductives and proof checking; D elaboration; L tactics. | Kernel-level proof/type preservation in an explicitly supported logic. Lean-specific quotient/axiom/universe/definitional-equality features need an exact bridge or R/F; success logs are not proof objects. `[S17]` |
| SystemVerilog | C-P signals/clocks/event regions, M/V-P bit carriers, D elaboration; B simulation/synthesis. | Four-state event/cycle semantics for simulation; narrower synthesized two-state contracts require explicit refinement. Assertions/classes/DPI/extensions require individual coverage. `[S25]` |
| CUDA | C/M-P execution geometry, memory spaces and synchronization; D ownership partitions; B device code generation. | Race/order/address-space and kernel trace correspondence under exact architecture/profile. CPU simulation does not earn GPU execution. Undefined/racy cases stay outside safe equivalence claims. `[S26]` |
| OpenQASM | Q-P channels/instruments/reset; C-D timed classical control; B QPU; L simulator. | Channel/instrument equivalence or declared channel error, plus timing/calibration constraints. Simulator/native-representation/hardware evidence remain three different claims. `[S20]` |
| BiDi/CDC | D/M/V-D finite flow, ternary quantization/prefix barrier and nest; C/M-D guarded effects/persistence; D/V-D topology, U1/U2 and evidence. | Exact pinned executed semantics, attribute-consumer views, source/receipt contracts, ordered state manifests and earned-artifact stages. Historical/theoretical dynamics are separate profiles. `[B03,B21,B34]` |

## H.3 Is the comparison basis complete?

Not as originally stated. The basis should add **Modelica-style acausal DAE composition**, because directed imperative execution does not preserve equations' lack of preassigned causality; and **synchronous-reactive constructive causality**, because logical ticks and instantaneous dependency rejection are not supplied by general actors. Modelica's specification supplies concrete versions of both concerns. `[S19]`

Datalog/constraint logic deserves a separate conformance seat for least-fixed-point semantics, stratified negation and order independence, rather than another mandatory new theory nucleus. Algebraic effect handlers deserve a seat for user-defined delimited interpretation, not because Haskell's mere ability to implement them would already count as native possession. `[S13,S16]`

The other candidates are evaluated as follows:

| Candidate group | Decision |
|---|---|
| Wolfram | Rewriting strategy, symbolic expressions and evaluation attributes deserve V/R conformance cases. They do not yet justify an eighth nucleus. Full proprietary evaluator equivalence is not claimed. |
| Smalltalk, Julia | Dispatch, object identity, reflection and image/world-age-like versioned method visibility belong in V/M/C profiles; residuals and staged code versions matter. No new nucleus just for dynamic dispatch. |
| Forth | Concatenative stack effects provide a valuable direct view of wiring/composition and resource effects; a projection rather than a separate semantic center. |
| ML/OCaml, Elixir | Pattern/type/closure and actor variants add important compatibility cases, not a new foundational operator family beyond the existing seats. |
| Kahn dataflow, Petri nets, interaction nets, CHR | Demand, causality, conflict, token resources and rewrite strategies stress C/R/M. Treat them as adversarial conformance witnesses; don't flatten true concurrency into one trace. |
| Differentiable languages | AD is a law-bearing transformation with derivative eligibility and discontinuity rules; higher derivatives and probabilistic/control-flow gradients add obligations, not a universal `differentiate anything` primitive. |
| Nix | Reproducible build expressions and dependency/effect isolation merit V/C/M capability tests. The Nix evaluator is not implicitly supplied by a pure function syntax. `[S36]` |
| Shell/process composition | Streams, file descriptors, process exit, signal and pipeline semantics are C/M interfaces. Byte streams and typed message protocols are not the same channel profile. |
| Regex, BNF, PEG | Regular-language recognition, context-free grammar description and prioritized parsing are distinct R/V strategies. PEG ordered choice must not be rewritten as unordered grammar alternation. |
| HTML/CSS/layout | Document structure and constraint/layout semantics can be native V/R/D graphs. Full browser layout, rendering, accessibility tree and host event behavior require extensive B/F profiles; not presently subsumed. |
| WASM, LLVM, MLIR, shaders | Important interchange/backends and exact trap/poison/memory profiles, not proof that semantics can be erased on import. MLIR remains a possible lower compiler substrate, not the definition of U. |
| Reversible, session-typed, categorical languages | Supply important no-discard/inverse/protocol/composition constraints on existing interfaces. A categorical presentation is useful only when its typing and equations reject real counterexamples. |

These decisions are design classifications, not claims to have proved embeddings for all candidate languages. A candidate gains a new nucleus only when its distinguishing operator cannot be represented natively in the existing ledger without significant hidden machinery or semantic loss, as measured by the conformance criterion.

# I. Deriving CDC without rewriting its history

## I.1 Audit reconciliation

All of the requested named documents were inspected: README, CDC_LANGUAGE, FORMAL_SEMANTIC_SPINE, UNIVERSAL_OPERATOR_SYSTEM, the root verification matrix, FRAMEWORKS, NATIVE_SELF_HOSTING_MANDATE, CDC_TOOLCHAIN_PLAN, historical BIDI_CALCULUS_CORE, U2_SEMANTICS, RFTC_FULL_BUILD_SPEC and the RFTC verification matrix. Relevant AST/parser, native reducer, variational implementation/tests, ABI, RFTC reducer, store and scheduler contracts/implementation sections and formal mirrors were also inspected. This is a source-and-evidence audit, not a claim to have read every line of every file or rerun every test.

Authority follows the repository's executable/source and evidence hierarchy. Historical documents remain historical. Important reconciliations:

| Surface | Audited result | Maturity boundary |
|---|---|---|
| Primitive reduction | Native `flow`, `commit`, `nest` execute; richer forms are derived. | Executed implementation inspected; relevant exact-head CI observed. |
| Continuous dynamics | Current flow is a synchronous explicit finite map, not an exact ODE solver. | Executed map; broader dynamics remain specifications/obligations. |
| U1 | Guarded source-bound closure, cover/pair/decision constraints. | Executed/tested closure, not full-state periodicity. |
| U2 | Executed-path Jacobians; earned recurrence and spectral stages; eight permanent mutant families. | Executed and adversarial gates inspected; no full runtime-correctness theorem. |
| Lean/Rocq mirrors | Finite ternary laws/counts, finite map composition/order and involutions. | Mechanized-finite; not C correctness, physical laws or nonlinear stability proofs. |
| C ABI 1.5 | Parsing/canonicalization/registry/verification and other explicit interfaces exist. `cdc_runtime_execute` still returns an unavailable-state error. | Native CLI execution and ABI execution must not be conflated. `[B24,B27]` |
| Toolchain plan | Later amendment says build/install/x are live; versioned manifest/dependency resolution did not land; directory packages remain. | Original phase sketches are not implementation evidence. `[B32]` |
| Self-hosting | Native C implementation exists; historical mandate names unfinished self-hosting gates. | Native does not mean compiler written in CDC. |
| RFTC | Classical reducer, topology/cells, authenticated local control/scheduler/journal. | C1/C2 and local C3-capable machinery, not completed multi-host C3 or quantum/physics evidence. `[B14,B33,B45]` |
| Exact-head CI | Run 31252447769 succeeded for the pinned main SHA, including native/formal/paper and macOS jobs. | Observed remote CI; no fresh local rerun. Supersedes stale pending-PR prose. `[B28]` |

## I.2 What is fundamental and what is specialized?

Fundamental *design mechanisms* worth lifting are typed interaction, contextual observation, explicit resource/authority boundaries, separation of state from transitions, causal/trace/clock distinctions, source identity, evidence-bearing derivations, and refusal to manufacture downstream artifacts. This is not a historical-priority claim that CDC first invented those mechanisms.

Specializations include the exact ternary carrier, phase/cosine quantizer, oriented prefix barrier, particular phase-coupled finite map, receptive/radiant channel roles, double-cover convention, parent-belief/child-prior update, and the U1/U2 family. These remain valuable and stable, but they are not forced on every U operation.

Aperture data, a held result, absence of a value, and logical false are four different meanings. U must not merge them just because each sometimes has a zero-like representation.

## I.3 Pin the compatibility profile

The initial bridge profile is:

`etellis.cdc/native-0.3.0@1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157`.

It binds grammar, AST attribute lookup by consumer, canonicalization, floating-point operations, operation order, resource bounds, state manifests, result strings/schemas and available execution surfaces. Future mathematical CDC semantics receive a different profile; importing today's programs does not upgrade them to future dynamics.

The parser retains an ordered token/statement stream. Its dictionary view is last-wins, while the legacy native scanner has a first-wins view. The bridge must preserve both consumer semantics where applicable. Collapsing duplicate attributes into one “clean” map can change behavior. `[B22,B23]`

## I.4 Exact primitive lowering

Let a selected state contain cell phases theta, module beliefs b and priors p, and discrete latch flags/values. Frozen parameters and source order belong to the manifest.

### Flow

For the selected field:

`theta_i' = theta_i + omega_i*d + G*d*sum_(e:t(e)=i) w_e sin(theta_s(e)+alpha_e-theta_i)`.

Every right-hand side uses the pre-step snapshot; only eligible same-field couplings participate. Beliefs and priors do not evolve under this current map. `[B21]`

Lower it to a shape-indexed snapshot transformation with explicit binary64/trigonometric realization and declared update order. Retain a derived native node `cdc.flow` with its checked expansion, so analysis can recover the operator rather than reverse-engineer loops. The displayed equation is real-arithmetic notation: bit-faithful lowering must retain the actual source sequence (initial phase plus omega times duration, then channel-order additions of gain times weight times sine times duration), not factor and reassociate the sum.

For fixed parameters, its Jacobian is:

`J_i,l = delta_i,l + G*d*sum_e w_e cos(theta_s+alpha_e-theta_i)*(delta_s,l-delta_i,l)`.

The derivative contract must identify the **real-valued extension of the executed finite-map graph**. It is not the literal classical derivative of binary64 rounding as a discontinuous map on encoded machine states. Analytic AD values and finite-difference checks are numerical evidence about that extension at the stated scope. This distinction retains the actual executed formula without claiming that floating-point rounding itself has been differentiated away.

For a self-loop the derivative terms cancel. Nonzero delays without a declared history state do not earn this derivative. No semantic rule replaces this map with a theoretical amplitude/plasticity/ODE dynamics absent from the executor.

### Commit

For each selected cell in source order, compute

```text
q(theta) = +1, if cos(theta) > deadband
           -1, if cos(theta) < -deadband
            0, otherwise.
```

Admit exactly when all running prefix sums are nonnegative. On acceptance, publish latch values and flags; on hold, leave the protected latch state unchanged. No phase snap or belief update is invented. `[B20]`

Lowering uses native quantization, an ordered scan and a guarded memory update. The barrier is a derived CDC policy, not a universal U primitive. Scheduled commit has identity derivative on continuous coordinates within a fixed mode; a quantization boundary produces a derivative hold.

The plain native reducer's sequencing must not acquire a new global abort-on-HOLD behavior. A held commit can be followed by later source steps according to the existing entry path; an accepted U1/U2 path has stricter admission requirements. Compatibility preserves the relevant entry semantics.

### Nest

For compatible parent and child modules, let u be the mean child latch trit, or current quantized value where the native path uses an unlatch fallback. Then:

`b_parent' = b_parent + G_parent*u`

`p_child' = b_parent'`.

Within a fixed local trit itinerary, the derivative copies the parent-belief tangent into the child-prior row. It is not the derivative of a theoretical `child_belief-child_prior` correction that the executor does not evaluate. `[B20,B37,B38]`

The U expansion is a typed aggregation followed by a scoped pair of state updates. Parent/child association and direction are explicit; global U scope is not constrained to CDC's hierarchy.

## I.5 Derived family map

| CDC family | U derivation |
|---|---|
| guard | Predicate/decision operation plus a capability-admission boundary; reasons and scope retained. |
| trace | Observation projection over an event/state path; observation-window and order contracts retained. |
| measure | Native aggregate/reduction over the selected carrier, not automatically a probability measurement. |
| policy | Source-selected decision function/constraint over evidence with explicit effect authority. |
| bridge | Typed coding/isomorphism or lossy projection with declared inverse/residual; short ternary codebook tags are not cryptographic hashes. |
| persist | Local durable transaction plus CDC's existing admissibility policy, through the same protected commit boundary. |
| authority | Nonforgeable lease protocol over issuer, action, frame, horizon, expiry, epoch and nonce constraints. |
| transport | Envelope/protocol operations with authentication and causal admission; actual network realization separately gated. |
| frame | Indexed observation/context interface and explicit frame translation. |
| topology | Oriented adjacency, path lifting, cover/winding invariants and witnessed transitions; no physical spin claim. |
| universal | A source-bound derived execution/closure family; the name does not itself prove Turing universality. |
| orbit | Full or declared-relative recurrence request and validation over an exact state/coordinate manifest. |
| variational | Law-bearing transformation of an executed operation graph to its admitted tangent map. |
| spectrum | Validated linear-algebra realization over an earned square return map, never over an arbitrary cover-return label. |

This yields `U structure -> admitted theory operations -> derived operator algebra -> CDC.native specialization`. No CDC keywords need enter the six-constructor core.

## I.6 U1 is not U2 recurrence

U1's lifted-cover closure can verify a double-cover return and source-bound admissibility while continuous coordinates or latch modes still fail full recurrence. A projected return cannot silently discard accumulating belief/prior coordinates. Canonical `loop-u720` has a meaningful tangent but does not thereby earn a full monodromy or Floquet classification. The isolated accepted relative fixture is a different, explicitly constructed realization. `[B09,B34,B35]`

A first-class type sequence captures this:

```text
ExecutedPath
  -> Analysis(U1Closure)
  -> Analysis(PathTangent)
  -> Analysis(VerifiedRecurrence)
  -> Analysis(ReturnMap)
  -> Analysis(Spectrum)
```

Each certificate binds the same selected path, source, runtime profile, coordinate manifest and relevant numerical contract. A caller cannot mix a recurrence certificate from one run with a tangent from another. A late spectral hold retains an already earned return map; it does not erase evidence or fabricate eigenvalues.

## I.7 U2 lowering

For ordered operations with column perturbations:

`A_0 = I; A_(k+1) = J_(k+1) A_k`.

Absolute recurrence requires the full declared continuous state, discrete mode and coordinate identity to return within the explicit tolerance contract. Relative recurrence additionally requires an executable restoration `rho`, its derivative, and admitted symmetry/equivariance obligations:

`norm(rho(x_T)-x_0)_W <= epsilon`.

Then and only then:

`M_absolute = A_final`

`M_relative = D rho(x_T) A_final`.

The inspected RFTC reducer also hashes wrapped phases after quantizing at a 10^12 scale. That is a precision-qualified fingerprint, not the raw identity of arbitrary binary64 phase arrays. U must preserve and name that quotient, not advertise it as lossless reconstruction. `[B45]`

Current v1 uses unit weights and declares topology through the coordinate manifest; a projected mask is not independently reconstructible from some current receipt shapes. Those consumer limitations are retained in the CDC bridge, not papered over by U. Current receipt decimal canonicalization uses fifteen significant digits even though decisions use unrounded binary64. U's own exact-bit numeric artifact sidecar should expose the full decision state, while preserving legacy CDC receipt bytes under their original contract. `[B34,B35]`

Permanent negative tests include reversed Jacobian order, reset-only substitution for required saltation, missing restoration derivative, theoretical rather than executed nest derivative, false/projected recurrence, ignored aperture, skipped tangent conjugacy and automatic polarity acceptance. `[B36]`

## I.8 Why this is a natural specialization

CDC requires transformations, typed observations, scoped state, explicit causal order, interface-indexed topology, capability admission and evidence-bearing transformations of transformations. Those are already required by the broader comparison basis. Its `flow`, `commit` and `nest` consequently lower without an extra universal primitive or an opaque CDC interpreter.

That is a concrete low-friction derivation—not a claim that CDC's particular prefix barrier or double-cover choice is the only calculus mathematically possible in U. “Inevitable” is justified at the level of available structural mechanisms, not uniqueness of the CDC specialization.

# J. Worked U examples

The following sources are design examples under one grammar. All parse in the included syntax audit. Their complete types/effects, runtime execution and backend realizations are not implemented by that audit. Names are proposed standard operators, not published packages. The independent arithmetic/proof probes establish only the scopes recorded in their receipts.

The first twenty satisfy the requested ergonomic set. Two additional examples expose the missing DAE family and a fully explicit native universal-computation construction. The latter is an expressiveness witness only; it is not the argument for subsuming any comparison language.

## J.01 hello

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// ConsoleCap is an explicit, nonforgeable permission; this profile borrows it.
def main(out: ConsoleCap) -> Effect(IO, Unit) =
  io.println(out, "Hello, world!");
```

## J.02 arithmetic

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// Int is unbounded. Changing this to I64 requires an overflow policy.
def square(x: Int) -> Int = int.mul(x, x);
```

## J.03 fibonacci

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// nat.rec is structural recursion. The pair is (F(k), F(k+1)).
def fib(n: Nat) -> Nat =
  tuple.first(nat.rec(n, (0, 1),
    fn(k: Nat, pair: Pair(Nat, Nat)) =>
      (pair.second, nat.add(pair.first, pair.second))));
```

## J.04 map filter reduce

```u
u "etellis.u/0.1";
use "u.standard/0.1";

def sum_positive_squares(xs: List(Int)) -> Int =
  list.fold_left(
    list.map(list.filter(xs, fn(x: Int) => int.gt(x, 0)),
      fn(x: Int) => int.mul(x, x)),
    0, fn(acc: Int, x: Int) => int.add(acc, x));
```

## J.05 ownership

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// with_mut suspends ownership, opens a fresh lifetime, and requires its
// view back. The view cannot escape, alias a writer, or cross an await.
def change_first(buf: Owned(Buffer(U8, 1))) -> Owned(Buffer(U8, 1)) =
  mem.with_mut(buf, fn(view: MutView(Buffer(U8, 1))) =>
    mem.write(view, 0, bits.u8(7)));
```

## J.06 async

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// Task semantics include completion, failure and cancellation; this is
// not a blocking call renamed async. HTTP authority is explicitly borrowed.
def fetch_text(net: SharedCap(Http), url: Text) -> Task(Text) =
  async.then(http.get(net, url),
    fn(reply: Response) => http.body_text(reply));
```

## J.07 macro

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// bind_once generates a hygienic let: an effectful argument is not duplicated.
// quote captures its region body at stage+1; splice is typed stage crossing.
def twice(argument: Code(Int)) -> Code(Int) =
  syntax.bind_once(argument, fn(x: Code(Int)) =>
    syntax.quote(fn() => int.add(syntax.splice(x), syntax.splice(x))));
```

## J.08 logic

```u
u "etellis.u/0.1";
use "u.standard/0.1";

def parent: Rel(Text, Text) =
  rel.rows([("Ada", "Ben"), ("Ben", "Cara"), ("Ada", "Dana")]);

def grandparent: Rel(Text, Text) =
  logic.relation(fn(x: Logic(Text), z: Logic(Text)) =>
    logic.exists(fn(y: Logic(Text)) =>
      logic.and(logic.apply(parent, x, y), logic.apply(parent, y, z))));

def query: Search(Substitution) =
  logic.query("sld.leftmost.depth_first.finite_terms",
    fn(who: Logic(Text)) => logic.apply(grandparent, "Ada", who));
```

## J.09 sql join

```u
u "etellis.u/0.1";
use "u.standard/0.1";

def User: Type = types.record(#{id: Int, name: Text});
def Order: Type = types.record(#{user_id: Int, total: Decimal});
def Row: Type = types.record(#{name: Text, total: Decimal});

// Inputs are bags from one explicit database snapshot. sql.eq is three-valued
// where nullable types require it; output order is not invented.
def join_orders(users: Bag(User), orders: Bag(Order)) -> Bag(Row) =
  sql.project(
    sql.inner_join(users, orders,
      fn(u: User, o: Order) => sql.eq(u.id, o.user_id)),
    fn(row: Pair(User, Order)) =>
      #{name: row.first.name, total: row.second.total});
```

## J.10 arrays

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// Contract one axis of each array. Rank and shape survive in the graph.
// The profile fixes reduction order and forbids IEEE reassociation.
def multiply(a: Array(F64, [2, 3]), b: Array(F64, [3, 4]))
  -> Array(F64, [2, 4]) =
  array.contract(a, b, [1], [0], "f64.strict_left_fold");
```

## J.11 bayes

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// sample inside model is a measure-binding operator, not a single draw.
// observe_all multiplies likelihood densities; normalization is separate.
def model(data: List(Real)) -> Measure(Real) =
  prob.model(fn() => {
    let mu = prob.sample(dist.normal(0.0, 1.0));
    prob.observe_all(dist.normal(mu, 1.0), data);
    yield mu;
  });

def infer(data: List(Real), seed: U64) -> ApproxPosterior(Real) =
  prob.infer(model(data), "hmc",
    #{seed: seed, chains: 4, draws: 1000, warmup: 500});
```

## J.12 actors

```u
u "etellis.u/0.1";
use "u.standard/0.1";

def Message: Type = types.sum(#{inc: Unit, crash: Unit});

def worker(state: Nat, message: Message) -> ActorStep(Nat) =
  sum.case(message, #{
    inc: fn(unit: Unit) => actor.continue(nat.succ(state)),
    crash: fn(unit: Unit) => actor.fail("requested_failure")
  });

def start(system: ActorSystemCap) -> Effect(Actors, SupervisorRef) =
  actor.supervise(system, "one_for_one",
    [actor.child("counter", 0, worker)],
    #{max_restarts: 3, window: duration.seconds(5)});
```

## J.13 temporal

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// spec includes stuttering and weak fairness of advance.
def advance(n: Nat) -> Option(Nat) =
  option.when(nat.lt(n, 3), fn() => nat.succ(n));

def system: BehaviorSet(Nat) =
  temporal.spec(0, advance, temporal.stutter(), temporal.weak_fair(advance));

def safe: Temporal(Nat) =
  temporal.always(fn(n: Nat) => nat.le(n, 3));
def live: Temporal(Nat) =
  temporal.eventually(fn(n: Nat) => nat.eq(n, 3));

def verification: FiniteModelReport =
  temporal.check(system, temporal.and(safe, live),
    model.closed_reachable_states([0, 1, 2, 3]));
```

## J.14 proof

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// nat.add recurses on its SECOND argument. This theorem needs induction.
// Proof checking is total and cannot call core.fix_partial or trust an IO result.
def zero_add: Pi(Nat, fn(n: Nat) => Eq(Nat, nat.add(0, n), n)) =
  fn(n: Nat) => nat.induction(n,
    fn(k: Nat) => Eq(Nat, nat.add(0, k), k),
    eq.refl(Nat, 0),
    fn(k: Nat, ih: Eq(Nat, nat.add(0, k), k)) =>
      eq.congr(Nat, Nat, nat.succ, ih));

def checked: ProofReceipt = proof.check(zero_add);
```

## J.15 hardware

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// A two-state, positive-edge synchronous counter. reg's region receives
// pre-edge q; all registers publish new q together. Initialization is explicit.
def counter(clk: Clock, reset: Signal(Bit, clk)) -> Signal(Bits(8), clk) =
  hw.reg(clk, bits.u8(0), fn(q: Bits(8)) =>
    value.select(hw.at_tick(reset), bits.u8(0),
      bits.add_wrap(q, bits.u8(1))));
```

## J.16 gpu

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// partition_launch derives a disjoint per-index lease from y. Padded lanes
// outside n never enter the region. x's read lease lives until task completion.
def saxpy(n: Nat, a: F32, x: GpuRead(F32, n), y: GpuOwn(F32, n))
  -> GpuTask(GpuOwn(F32, n)) =
  gpu.partition_launch(
    #{grid: [nat.ceil_div(n, 256)], block: [256]}, y,
    fn(thread: GpuThread, yi: GpuOwnElement(F32)) =>
      gpu.write(yi,
        f32.add(f32.mul(a, gpu.load(x, thread.global_x)), gpu.read(yi))));
```

## J.17 quantum

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// q is ONE joint register. Each gate consumes and returns its linear handle.
// No classical Copy witness exists for QReg. measure_all is an instrument.
def bell(q: QReg(2)) -> QReg(2) =
  quantum.cx(quantum.h(q, 0), 0, 1);

def experiment: QuantumProgram(Bits(2)) =
  quantum.program(fn() => quantum.measure_all(bell(quantum.zero(2))));

// Choosing quantum.simulator or a QPU occurs outside this program, through an
// explicit realization. A simulator receipt is never a hardware receipt.
```

## J.18 cdc

```u
u "etellis.u/0.1";
use "u.standard/0.1";

use "etellis.cdc/native-0.3.0-1307f2a7";

// make_state includes default immutable parameters and explicit source order.
def initial: CdcState = cdc.make_state(#{
  fields: [#{name: "f", gain: 1.0, deadband: 0.1}],
  modules: [#{name: "parent", field: "f", belief: 0.0, prior: 0.0},
            #{name: "child", field: "f", belief: 0.0, prior: 0.0}],
  cells: [#{name: "p", module: "parent", theta: 0.0, omega: 0.0},
          #{name: "c", module: "child", theta: 0.0, omega: 1.0}],
  channels: []
});

def run(state: CdcState) -> CdcStepResult = {
  let flowed = cdc.flow(state, "f", 0.1);
  let committed = cdc.commit(flowed.state, "child");
  // Both accepted and held commit results retain state. Native reducer
  // sequencing does not gain a new abort-on-HOLD policy in this lowering.
  yield cdc.nest(committed.state, "parent", "child");
};
```

## J.19 u1

```u
u "etellis.u/0.1";
use "u.standard/0.1";

use "etellis.cdc/native-0.3.0-1307f2a7";

// Every check binds the SAME immutable ExecutedPath identity. close cannot
// accept checks from a different run, selected path, frame, or source.
def guarded_closure(run: ExecutedPath(CdcNative), pair: ReciprocalBinding,
                    target: DecisionCoordinate) -> Analysis(U1Closure) =
  cdc.u1.close(run, #{
    reciprocal: cdc.check_reciprocal(run, pair),
    cover: topology.check_oriented_double_cover(run, pair),
    commits: cdc.check_accepted_commits(run),
    decision: cdc.check_generated_coordinate(run, target),
    effects: cdc.check_effect_binding(run, target)
  });
```

## J.20 u2

```u
u "etellis.u/0.1";
use "u.standard/0.1";

use "etellis.cdc/native-0.3.0-1307f2a7";

// Analysis.then retains earlier earned artifacts on a downstream hold.
// No test for a cover return is allowed to stand in for full recurrence.
def variation(closed: U1Closure, request: RecurrenceRequest)
  -> Analysis(Spectrum) =
  analysis.then(ad.executed_path(closed), fn(tangent: PathTangent) =>
    analysis.then(recurrence.check(tangent, request),
      fn(recurrence: VerifiedRecurrence) =>
        spectral.real_schur(
          linear.return_map(tangent, recurrence),
          #{residual_tolerance: 0.0000000001})));

// return_map uses D(rho) * tangent for earned relative recurrence, not tangent
// alone. The canonical loop-u720 holds before this constructor is available.
```

## J.21 acausal dae

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// Acausal RC equations. The solver chooses causality after analyzing the
// equations; these are not preordered assignments or discrete samples.
def rc(resistance: PositiveReal, capacitance: PositiveReal,
       source: RealTrajectory) -> DAE =
  dynamics.dae(fn(v: RealTrajectory, i: RealTrajectory) =>
    #{equations: [
        dynamics.equal(source,
          dynamics.add(v, dynamics.scale(resistance, i))),
        dynamics.equal(i,
          dynamics.scale(capacitance, dynamics.derivative(v)))
      ],
      initial: [dynamics.at(v, 0.0, 0.0)]});
```

## J.22 native universal construction

```u
u "etellis.u/0.1";
use "u.standard/0.1";

// Explicit native Turing-machine construction, ONLY a computational-
// universality witness, NOT evidence of native foreign-language subsumption.
// A finite transition function and a finite input tape are parameters.
def Tape: Type = types.record(#{left: List(Nat), head: Nat, right: List(Nat)});
def Config: Type = types.record(#{state: Nat, tape: Tape});
def Move: Type = types.sum(#{halt: Unit,
  left: Pair(Nat, Nat), right: Pair(Nat, Nat)});

def execute(delta: Fn(Nat, Nat, Move), initial: Config) -> Partial(Config) =
  core.fix_partial(
    fn(again: Fn(Config, Partial(Config)), config: Config) =>
      sum.case(delta(config.state, config.tape.head), #{
        halt: fn(unit: Unit) => partial.done(config),
        left: fn(next: Pair(Nat, Nat)) => again(#{
          state: next.first,
          tape: #{left: list.tail_or_empty(config.tape.left),
                  head: list.head_or(config.tape.left, 0),
                  right: list.cons(next.second, config.tape.right)}
        }),
        right: fn(next: Pair(Nat, Nat)) => again(#{
          state: next.first,
          tape: #{left: list.cons(next.second, config.tape.left),
                  head: list.head_or(config.tape.right, 0),
                  right: list.tail_or_empty(config.tape.right)}
        })
      }), initial);
```

### J.23 What the examples establish—and do not

The grammar is shared across code, logic, measure models, signals, circuits, proof terms and CDC. Domain-specific region signatures determine their interpretation, and editors must show those signatures rather than let visually similar syntax hide different evaluation rules. The source does not contain SQL/Prolog/QASM strings passed to a foreign evaluator.

The proof example's induction is also checked by the small independently readable Nat/Eq checker in `audit/check_design.py`: zero base, inductive hypothesis, successor congruence and the defining equations of addition. This checks that mathematical proof term structure, not the U elaborator or its total soundness. A deliberately invalid induction step is rejected.

The Turing-machine construction stores an unbounded-in-principle tape as two finite lists plus a head, performs native inductive case analysis, and unfolds partial recursion. Each source machine transition has a direct U computation. A formal simulation/adequacy theorem and resource-bounded implementation tests are required before publishing a mechanized universality claim. This construction does not import a hidden foreign interpreter; its complete machine mechanism is visible. It must never be reused as the native-subsumption argument for SQL, Prolog, hardware or quantum operations.

The examples still expose real ergonomic debt: relational binders, effect/resource signatures and staged proof terms are more verbose than mature specialist syntax. Family resemblance is demonstrated syntactically; comparative usability remains open.

# K. Compiler, runtime and tooling architecture

## K.1 Frontend and typed graph

The frontend retains both a lossless concrete syntax tree and an elaborated operation graph. The CST owns comments, original spelling, source spans and import residual anchors. The graph owns resolved operations, interface types, binders, stages, effect/resource contracts and immutable semantic profile identifiers.

Bidirectional type checking is preferred over promising full inference for arbitrary dependent types. Recursive groups, exported polymorphism, effects and non-obvious resource splits require annotations. A small total proof/type-conversion checker is kept separate from the ordinary evaluator and optimizer. A plugin may introduce syntax conveniences or derived definitions, but it may not extend the trusted logic or resource rules without an explicit trust-manifest change.

## K.2 Required typed interfaces

```text
TheoryDescriptor {
  id, version, digest;
  kind_signature, interface_constructors;
  operator_schemas;
  resource_and_stage_rules;
  semantic_model_reference;
  executable_reference_rules;
  laws_with_assumptions;
  observation_contracts;
  cross_theory_adapters;
  conformance_cases;
  trust_dependencies;
}

OperatorSchema {
  qualified_name;
  type_scheme;
  static_parameter_schema;
  region_signatures;          // binders, theory, stage, effects, resources
  admissibility_premises;
  denotation_reference;
  reference_rule_or_expansion;
  invalid_and_held_outcomes;
  permitted_realizations;
}

TransformResult {
  input_artifact, output_artifact;
  relation_kind;              // exact, refinement, approximation, unsupported
  observation_map;
  assumptions;
  residual_change;
  error_budget;
  proof_or_obligations;
  receipt;
}
```

An operator that has only a name and a backend callback is F/B or unsupported, not certified native. A derived-native operation carries an immutable expansion and can remain as a recognizable high-level node while its body is checked against that expansion. Analysis and optimization may inspect both.

## K.3 Evaluation and compilation

The reference evaluator is the semantic oracle for admitted executable fragments. It interprets U operators directly; it does not dispatch to a collection of full source-language runtimes and relabel the result. Numeric/constraint/spectral solvers can be specialized implementations with explicit validation contracts.

The compiler has three abstraction-preserving graph levels:

1. **Meaning graph:** high-level operators, domain distinctions, laws, proof and residual obligations.
2. **Realization graph:** selected algorithms, data representations, scheduling, numeric modes and backend requirements.
3. **Executable graph:** bytecode/native/WASM/device operations and evidence hooks.

Every lowering edge carries a relation and a source map. A late low-level optimizer cannot silently make a new high-level equivalence claim. Caches key on profile, dependencies, assumptions, numeric modes and observation contracts—not source text alone.

## K.4 Optimization

Rewrites require a law whose assumptions are discharged. Examples: join reassociation under compatible bag/null semantics; pure map fusion preserving laziness/effects; array contraction lowering preserving required reduction order; quantum circuit cancellation under exact gate/channel identities; dead code elimination only where observing allocation, failure, timing or reflection cannot distinguish it.

An equality-saturation engine may search candidate rewrites, but an e-graph is not a proof of their soundness. Extraction produces a checked derivation or an explicitly weaker tested transformation. Compile-time checking costs are separate from runtime costs.

## K.5 Backend strategy

Start with an independent reference evaluator, then portable bytecode/WASM and native code generation for explicit fragments. Reuse LLVM/MLIR as lower implementation infrastructure when useful without confusing their representation with U's semantic kernel. Preserve unbounded Int via a declared native implementation rather than pretending all integers fit machine words.

GPU, HDL synthesis and quantum provider interfaces are architecture targets from day one. Until device/cycle/channel execution gates pass, their honest statuses are semantic specification, simulator execution, interchange generation, or unsupported physical realization. “Emits QASM” is not “ran on a quantum computer”; “emits RTL” is not “synthesis-equivalent for all SystemVerilog.”

## K.6 Tooling and packages

Provisional command surface:

```text
etellis-u check   etellis-u run     etellis-u eval
etellis-u test    etellis-u prove   etellis-u fmt
etellis-u build   etellis-u lift    etellis-u export
etellis-u explain etellis-u audit   etellis-u lsp
etellis-u package lock/install/verify
```

An explicit workspace manifest and lockfile exist from the start. Exact dependency/profile hashes, package permissions, build inputs and generated artifacts are recorded. Package scripts do not obtain ambient IO/network/subprocess access. A local package path is not a trusted package automatically. Host permissions are capabilities enforced by the runtime/process boundary.

The LSP exposes resolved semantic theory, effects, resource lifetime, profile, law obligations, import loss and earned evidence. “Why is this held?” and “What changed meaning in this lowering?” are first-class diagnostics. Formatter/canonicalizer/schema migration all share the parser and AST rather than implementing unrelated syntactic guesses.

# L. Legacy ingestion and semantic reconstruction

## L.1 Pipeline

```text
source bytes + source version + build/host environment
 -> lossless CST and source symbol identity
 -> source elaboration with explicit semantics profile
 -> U meaning graph + residual + provenance + obligations
 -> analysis / proof / law-qualified transformations
 -> realization selection
 -> execution / native / WASM / hardware / accelerator / quantum interchange
```

The source environment includes include paths, macro expansion, module resolution, schemas, ABI, language flags, dynamic-loading policy and host semantics where observable. Missing environment is an unresolved import obligation, not permission to choose an attractive default.

## L.2 Residual types

Residuals are not one opaque blob:

- **Lexical residual:** comments, spelling, whitespace and source layout.
- **Intensional residual:** source/frame/code-object information observable by reflection.
- **Semantic residual:** constructs not yet represented natively; blocks native-subsumption claims for that region.
- **Environment residual:** host, schema, ABI, device calibration, external module and build assumptions.
- **Approximation residual:** lost precision, solver assumptions, discretization error or unresolved reconstruction data.
- **Provenance residual:** signed/digested links and mapping information, not a substitute for retained bytes.

A semantic residual can be preserved exactly while remaining unexecutable without FFI. That is a useful import, but it has not passed native subsumption. An intensional residual may legitimately be read as data by a native reflection operator; that differs from interpreting a whole opaque source AST at runtime.

## L.3 Reconstruction laws

For a lossless import, require:

`reconstruct(G,R,Pi) = original_bytes`.

This alone is trivial if original bytes are stored, so it is **not** the semantic preservation theorem. The native criteria and observational relation must pass separately.

After transformation T, require either a checked residual update/lens or an explicit loss report:

`reconstruct(T(G), update_R(T,R)) = transformed_source`.

Original bytes remain historical provenance; they cannot falsely stand in for transformed semantics. If source L lacks an equivalent construct, export produces a typed residual/unsupported result, not a misleading approximation advertised as exact round-trip.

For a finite collision fiber `{P : E_native(P)=G}`, a lossless residual must distinguish every retained source possibility. Its information content is at least `log2(fiber size)` bits under a fixed coding convention. There is no universal bounded residual that losslessly reconstructs arbitrarily many distinct artifacts.

## L.4 Domain-specific preservation relations

For deterministic defined programs, use contextual/trace equivalence with the declared observation boundary. For nondeterministic specifications, distinguish trace-set equality from implementation refinement. For probability, use exact measure equivalence where justified or a named statistical distance/expectation error bound. For finite quantum programs, use channel equality or a specified channel norm; an output histogram on one basis is insufficient.

For numerical map composition, an error budget may propagate as `epsilon_total <= epsilon_2 + L_2*epsilon_1` only when the relevant Lipschitz/stability premise is justified. Tolerances cannot be casually added across unstable dynamics or discontinuous decisions.

For types/proofs, preserve the source judgment in a compatible target logic with all assumptions visible. A TypeScript acceptance result, a Rust borrow-check result, a Lean proof, and a CDC adversarial test are different evidence types.

## L.5 What cannot be simultaneously promised

U can serve as language, semantic IR, formalism, bridge, analysis substrate and backend description layer. It cannot promise all of these additional properties at once: arbitrary source semantics, exact total executable evaluation, complete automatic proof, minimal residual, universal optimal performance and maximal beginner simplicity.

The contradiction is resolved by explicit profiles and statuses, not hidden modes. Some artifacts are analyzable specifications without an executable realization; some are executable but only approximately related to a mathematical model; some can be round-tripped lexically but remain foreign; some need an expert-facing view. The common kernel and interface contracts keep those distinctions compositional rather than pretending they disappear.

# M. Formal proof program

Proof obligations must be entered before implementation claims. A practical sequence:

| Obligation | Statement / scope | Evidence needed |
|---|---|---|
| Structural well-formedness | Capture avoidance, alpha-renaming, graph substitution and interface preservation. | Mechanized binding/typing proofs and parser/serializer property tests. |
| Type preservation | A well-typed step preserves the operation/interface judgment and required resources. | Theory-parametric theorem with explicit per-generator premises; extension admission discharges them. |
| Progress, qualified | A closed admitted executable operation steps, returns, is legitimately held/refused, or is in a declared waiting state. | No false theorem excluding environmental blocking, general divergence or missing backends. |
| Resource safety | Linear resources are not duplicated; invalid borrow/alias/escape cannot be constructed through safe interfaces. | Mechanized ledger/usage invariants plus adversarial runtime enforcement tests. |
| Proof-fragment soundness | Checked closed proof terms cannot derive falsehood relative to listed axioms/model. | Independent small kernel, normalization/consistency program; no unrestricted partial recursion in proof conversion. |
| Operator expansion | A derived-native node and its declared U expansion agree under its contract. | Per-operator equivalence proof or explicitly weaker differential evidence. |
| Theory composition | Adapter and composite profile preserve the relevant laws/observables. | Per-pair interaction laws; no universal monad-composition assumption. |
| Semantic lifting | Each admitted source fragment preserves observations, types/resources, intensional behavior and required residuals. | Frontend-specific simulation/contextual equivalence theorems and differential suites. |
| Backend lowering | Realization/executable graph implements or refines the meaning graph. | Backend simulation/refinement proofs and machine/device conformance. |
| CDC bridge | Pinned flow/commit/nest, parser views, entry modes and receipts match original execution. | Differential tests first; source-bound semantic preservation theorem later. |
| U1/U2 non-fabrication | No return map without bound recurrence; no spectrum without a return map; holds retain exactly earned artifacts. | Typed-constructor invariants, receipt consumer checks and permanent mutants. |
| Continuous realization | Numerical artifacts satisfy the stated approximation contract under explicit hypotheses. | Interval/residual validation or convergence/error proof at the claimed scope; not just a tolerance flag. |
| Probability | Measure/inference transformations preserve the claimed distribution or error relation. | Integrability/normalization premises, inference-specific theory and independent tests. |
| Quantum | Channel/instrument composition and lowering preserve physical admissibility and stated error. | Algebraic proofs plus independent simulator/device receipts at separate levels. |
| Replay/persistence | Recovery preserves committed state and distinguishes torn tails from corruption; replay identity respects declared projection. | Crash-point injection, concurrency tests, authenticated history/anchor model and eventual mechanization. |
| Serialization/residual | Canonical structural identity is stable; exact source reconstruction and transformation lenses obey their laws. | Property tests, fuzzing and mechanized encoding/injectivity results on the specified data model. |
| Universality | The native partial-recursive/inductive-data fragment simulates a chosen universal machine. | Explicit state encoding and step correspondence; not used as foreign-language subsumption proof. |
| Self-hosting | Stage builds preserve compiler behavior and bootstrap dependencies are visible. | Reproducible stage comparison plus independent reference/translation validation; equal binaries alone do not establish trust. |

Formalization may begin in Lean or Rocq, but U's own proof support requires an independent implemented checker for its admitted logical fragment. Claims must state whether a theorem concerns syntax, a finite model, an abstract evaluator, generated machine code, or the deployed runtime.

# N. Adversarial review and two redesign passes

## N.1 Architecture zero: everything is a contextual rewrite

The first plausible architecture was a universal contextual rewrite over a shared graph/state store, with source languages mapped to operations and CDC's guard/commit ideas used throughout.

It fails. A copyable universal value cannot preserve quantum state semantics. Generic graph steps do not distinguish probability from search, time trajectories from finite samples, or proof terms from arbitrary nonterminating programs. A mandatory universal “commit” privileges CDC without first-principles justification. Giving every operation a custom callback hides a kitchen sink behind one keyword.

**Redesign pass one:** replace universal values with typed ports and explicit resource usage; separate semantic theories; move CDC's barrier out of the core; separate declared mathematical meaning from execution profile. General recursion is removed from total proof conversion, and `par` requires an independence judgment.

## N.2 Architecture one: typed graphs with dialects

This fixes the obvious category errors but is still in danger of being MLIR with a different source syntax. Merely labeling a node `prolog.unify`, `quantum.gate` or `continuous.flow` does not give it native meaning. It also allows profile explosion: every source language could hide its own VM behind an otherwise identical surface.

It fails the stronger native test unless operators have reusable laws, direct rules/expansions, observation contracts, and genuinely interoperating resources. It also fails recovery if a “residual” is only a digest or if transformed code keeps stale source annotations.

**Redesign pass two:** require law-bearing operator descriptors, explicit adapters and theory-composition obligations, native/operator conformance gates, residual reconstruction laws, an immutable semantic-profile ledger, and separate meaning/realization/evidence graphs. High-level operators remain inspectable through certified expansions rather than disappearing irreversibly during lowering. Admission constructors prevent tests or metadata from impersonating proofs and recurrence.

## N.3 Final attack table

| Attack | Result after redesign |
|---|---|
| “Just Lisp?” | Shared syntax and metaprogramming are not novelty. U must additionally enforce resource/theory/realization laws and earn native conformance; otherwise this criticism wins. |
| “MLIR with prettier syntax?” | A serious risk. Mandatory semantic descriptors, user-visible obligations, residual transformation and independent evaluation are differentiators to implement and test, not merely promises. |
| “Wolfram rewritten?” | Symbolic expressions alone do not establish resource, clock, proof or channel semantics. Full symbolic evaluator coverage is not claimed. |
| “Process calculus plus types?” | Causal composition is one component, not a sufficient account of measures, proof conversion, DAE solution spaces or channels. Encoding them still has to satisfy the native-cost test. |
| “Category theory cosplay?” | The categorical fragment is useful only insofar as its typed laws reject copying qubits, invalid wiring, unsafe interchange and unqualified feedback. No physics or uniqueness claim follows. |
| “CDC privileged?” | Its ternary barrier, phase realization and double cover are derived profiles. Core programs need not have CDC-style commit/hold policies. |
| “Fake quantum?” | The design retains joint channels/instruments and forbids implicit cloning. A physical backend remains unearned until actual provider/device execution evidence exists. |
| “Fake continuous?” | Path/equation denotation is native; numerical realizations remain finite and explicit. Arbitrary exact real computation is not promised. |
| “Proof delegated to Lean?” | External metatheory is allowed; object-language proof acceptance must be performed by U's own declared trusted checker. No theorem from an external success message. |
| “Hardware opaque?” | A reference cycle/event semantics is required before declaring native HDL support. Synthesis is a separate refinement with its own gate. |
| “Kernel minimal?” | Structurally economical, not globally minimal. Six constructors can be re-encoded; the seven theory clusters are an honest semantic ledger, not an irreducibility theorem. |
| “Several languages in one file?” | It becomes that if theories are opaque VMs. One graph, shared typed interfaces, explicit adapters and cross-family conformance must demonstrate otherwise. Some combinations will remain unsupported. |
| “Foreign semantics recoverable?” | Only within declared fragments and residual contracts. Retaining original bytes proves lexical recovery, not native translation or valid transformed export. |
| “Performance tax everywhere?” | Effects/profiles/laws can often be discharged or erased at compile time. Dynamic checking and receipts are scoped, and their overhead must be measured. No universal speed claim. |
| “Easy to learn?” | The central concept is short; advanced programs remain advanced. Twenty-two syntactically coherent examples are not a user study. |

## N.4 Remaining hard problems

The largest unresolved items are sound and useful cross-theory composition; extensible yet small trusted proof/type checking; full source-intensional compatibility; efficient resource analysis across mixed concurrency/device models; total/certified derivative transformation through hybrid events; and predictable ergonomics when profile obligations become rich.

These are concrete research/engineering obligations, not reasons to replace the full architecture with a toy. They also prevent a responsible claim that the universal language-design problem is now completely solved.

# O. Normative specification candidate 0.1

The following requirements are binding for implementation work derived from this dossier.

**O1 — Three artifacts.** Meaning graph, realization graph and evidence record are distinct, linked, immutable versioned artifacts. A successful backend run cannot silently revise a meaning contract.

**O2 — Six structural constructors.** The kernel AST implements the constructors in D.1. Derived high-level nodes retain a checked expansion or admitted reference rule. Syntactic compression does not exempt semantic generators from the public ledger.

**O3 — Total type conversion.** Kinds/types use an explicit stratified dependent core: variables/de Bruijn indices, universes, Pi with usage, Sigma, inductive sums/data, equality/eliminators, indexed opaque interfaces and staged Code types. Conversion normalizes only total admitted definitions. Bidirectional checking handles exported annotations; no promise of complete dependent inference.

**O4 — Extension admission.** New syntax macros may not mutate core semantics. A new primitive/theory/checker extension names its model, operator schemas, rules, laws, negative tests and trust impact. Unchecked laws remain assumptions, visible to downstream claims.

**O5 — Resource safety.** The unrestricted/affine/linear contexts are explicit. Copy/drop/aliasing require admitted witnesses. Foreign values, unsafe memory and physical handles cannot mint proof or authority certificates by coercion.

**O6 — Profile preservation.** Numeric realization, evaluation strategy, memory model, clock discipline, observation boundary, source version and backend constraints participate in semantic artifact identity. An optimizer cannot ignore them.

**O7 — Typed refusal.** Parse/configuration/type errors, unsupported realization, semantic HOLD, operational fault and indeterminate effect completion have distinct codes. Previously earned artifacts survive later-stage holds; unavailable artifacts are absent, not dummy hashes or zero-dimensional placeholders.

**O8 — Canonical format.** Canonical text normalizes a syntax tree under a versioned grammar. The semantic graph serialization is a tagged, deterministic encoding of ordered ports, binders, profiles, attributes and edges. Initial exchange encoding: UTF-8 canonical JSON with sorted object keys by Unicode code point, ordered arrays, no duplicate keys, and no untyped JSON numeric values in semantic numeric fields. Integers/rationals use canonical tagged strings; IEEE values use width plus exact hexadecimal bit patterns. Source strings retain exact code points. AST node IDs are deterministically assigned by specified source/region traversal; arbitrary graph isomorphism and extensional equivalence are not canonicalized.

**O9 — Hashes.** Use domain-separated cryptographic hashes over exact canonical bytes. Algorithm identifier and format version are mandatory. The probe package uses SHA-256 for its own file/audit identities; compatibility preserves CDC's existing BLAKE3 contracts. A digest is not an authenticity claim. Signatures require a separate identity/threat model.

**O10 — Residual contract.** Each import records whether lexical, intensional and semantic reconstruction is available. Transformations update residuals or explicitly invalidate the relevant export claim. A hash without payload is a commitment only.

**O11 — Evidence.** Receipts carry scope, maturity dimensions, verdict, obligations, assumptions, profile/runtime/checker identity, source/artifact dependencies and earned outputs. Evidence aggregation may not upgrade tests to proofs or simulation to physical execution.

**O12 — Versioning.** Grammar version, semantic-profile version, ABI version and package version are separate. A behavior-changing fix receives a new profile identity, even when marketed as a patch. Old profiles remain executable or explicitly unsupported; migrations produce preservation/change receipts. Pre-1.0 does not permit silent semantic drift.

**O13 — Realization gates.** No backend capability is advertised merely because a file can be emitted. Native/WASM requires execution equivalence gates; HDL requires cycle/synthesis contracts; GPU requires device/memory/concurrency gates; QPU requires actual job/provider/calibration/result provenance. Simulation gates remain valuable but differently labeled.

**O14 — CDC stability.** `.cdc` remains a first-class frontend and useful language. The initial bridge is pinned to the audited SHA and keeps BiDi unchanged. Any later shared-runtime refactor is a separate, authorized project with parity gates and rollback.

**O15 — Semantic uncertainty.** Solver unknown, incomplete proof search, unavailable source context, invalid differentiability and unresolved cross-theory composition remain information. They never become arbitrary Boolean false, guessed values, or success-shaped receipts.

# P. Repository, interfaces, tests and milestones

## P.1 Proposed sibling layout

```text
workspace/
  BiDi/                         # initially untouched
  U/
    README.md
    U.toml                      # workspace, theory/profile, package policy
    U.lock                      # exact dependency and semantic identities
    AGENTS.md                   # architecture and evidence rules for Codex
    Cargo.toml                  # bootstrap workspace; host compiler is explicit
    docs/
      SPEC.md AUTHORITY.md CLAIMS.md DESIGN_HISTORY.md
      semantics/ types/ effects/ resources/ identity/ time/
      profiles/ adapters/ ingestion/ backends/ self_hosting/
      decisions/ risks/ migration/
    spec/
      grammar/ ast/ graph/ schemas/ canonical/ receipts/
      theory-ledger.json
      conformance-manifest.json
    crates/
      u-source/                 # lossless CST, source bytes, spans, residual anchors
      u-parser/ u-ast/ u-format/
      u-kinds/ u-types/ u-resources/ u-proof-kernel/
      u-core/                   # structural AST, substitution, graph wellformedness
      u-theory/                 # checked descriptors, profiles, adapter admission
      u-eval/                   # independent executable reference semantics
      u-values/ u-memory/ u-relations/ u-probability/
      u-causality/ u-dynamics/ u-quantum/
      u-evidence/ u-canonical/ u-store/
      u-opt/ u-realize/ u-bytecode/
      u-backend-native/ u-backend-wasm/ u-interchange/
      u-backend-gpu/ u-backend-hdl/ u-backend-qpu/
      u-import-core/ u-import-cdc/ u-import-c/ u-import-rust/
      u-import-python/ u-import-js/ u-import-lisp/ u-import-haskell/
      u-import-prolog/ u-import-sql/ u-import-apl/ u-import-stan/
      u-import-erlang/ u-import-tla/ u-import-lean/
      u-import-systemverilog/ u-import-cuda/ u-import-qasm/
      u-cli/ u-lsp/
    theories/
      value/ memory/ relation/ measure/ causal/ trajectory/ quantum/
      derived/ adapters/
    profiles/
      cdc/native-0.3.0-1307f2a7/
      numeric/ isa/ c/ rust/ python/ ecmascript/ typescript/
      lisp/ haskell/ prolog/ sql/ apl/ stan/ erlang/
      temporal/ dependent-proof/ systemverilog/ cuda/ openqasm/
    compatibility/cdc/
      SOURCE_PIN.json oracle/ fixtures/ differential/ receipt-consumers/
    tests/
      syntax/ typing/ proof/ resource/ semantics/ conformance/
      cross_theory/ differential/ metamorphic/ adversarial/
      fuzz/ crash/ replay/ security/ performance/ golden/
    examples/
      basics/ logic/ data/ probabilistic/ reactive/ proof/
      hardware/ accelerator/ quantum/ cdc/ hybrid/ universal/
    formal/
      lean/                     # or Rocq; pin toolchain and axioms
      correspondence/
    std/                        # derived libraries written in U as they become viable
    bootstrap/
      stage0/ stage1/ stage2/ trust-manifest.json
    tooling/editor/ tree-sitter/ integration/
    scripts/ ci/ benchmarks/
    artifacts/                  # generated receipts, no fake evidence committed as facts
```

A module may initially have an explicit `Unsupported` implementation and declared obligations. It may not return canned successful results to make the tree look finished. The tree is an architecture contract, not a demand for empty crates before useful vertical implementation.

## P.2 Runtime interfaces

At minimum separate `parse`, `elaborate`, `check`, `normalize_total`, `evaluate`, `realize`, `transform`, `lift`, `reconstruct`, `verify_receipt` and `compare_observations`. Each takes immutable profile/context inputs and returns typed diagnostics/receipts. Executing is not a hidden side effect of parsing, formatting or type checking. A proof-checking path never calls an unrestricted evaluator.

The CDC oracle runner is an explicit test utility, isolated from the native U CDC evaluator. Native compatibility cannot be claimed by invoking the original executable. Because the inspected C ABI execution function is unavailable, do not build the bridge around a nonexistent in-process execution feature; use actual available surfaces and a transparent CLI oracle for differential tests. `[B27]`

## P.3 Milestones and release gates

| Milestone | Deliverable | Must not claim |
|---|---|---|
| 0 — Specification/identity lock | Source audit, contracts, theory ledger, grammar/CST/AST, error taxonomy, exact versioning, residual/evidence schemas and threat model. | “Language complete” because docs and parsers exist. |
| 1 — Structural/value foundation | Reference evaluator, type/resource checker, total proof fragment, native recursion/inductive data, formatter, CLI, independent tests. | Universal native subsumption from universal computation. |
| 2 — Native CDC compatibility | Actual U expansion/evaluation of pinned primitives, source identity, holds/receipts, differential oracle, U1/U2 staging cases. | ABI execution availability, exact continuous ODEs, or proof of all CDC behavior from examples. |
| 3 — Theory operator conformance | Representative native cases for all eighteen families plus DAE/synchronous/handler/least-fixed-point additions; explicitly unsupported whole-language regions. | Complete frontends or physical device execution. |
| 4 — Interoperability and analysis | Lossless CST ingestion, residual reconstruction, fragment manifests, checked transforms, law-aware optimizer, source maps and LSP. | Exact transformed round-trip where the source cannot express the result. |
| 5 — Production classical runtime | Crash-safe store, sandboxed capabilities, deterministic replay policies, native/WASM execution, fuzz/security/performance gates. | Global exactly-once distributed effects or universally fastest implementation. |
| 6 — Specialized realizations | Validated numeric solvers, GPU device runs, HDL cycle/synthesis checks, quantum simulator and separately QPU receipts. | Evidence above the specific gate/experiment. |
| 7 — Formal correspondence and self-hosting | Strengthened source/backend theorems; stage1 compiler in U; reproducible stage2 and independent oracle comparisons. | Trust-free bootstrap or complete proof of deployment merely from self-compilation. |

Milestones can proceed partly in parallel, but later gates never waive earlier semantic obligations. Preserve the end-state interfaces from day one; implement coherent vertical paths rather than a disposable demo with hard-coded examples.

## P.4 Test policy

Every operator family needs positive, negative, differential and metamorphic cases. Add permanent mutation gates that remove a relevant law and must fail: clone a QReg; conflate probability with nondeterminism; drop a SQL duplicate; eager-evaluate a lazy bottom; turn a held result into false; drop a lifetime escape check; reorder a GPU barrier; equate a finite map with an ODE semigroup; omit D-rho; accept an unsigned/incorrect-frame lease; mistake a source digest for source reconstruction; and bypass a proof kernel through general recursion.

Conformance reports must show the admitted fragment, backend, numeric/environment profile, observations compared and unresolved obligations. A test skipped for a missing backend remains skipped/unexecuted; it never contributes to a passed hardware claim.

## P.5 Performance and quality scorecard

Track representational loss, operator visibility, annotation/code density, type guarantees, analysis coverage, proof scope, runtime time/memory/energy, compile time, ecosystem maturity, ergonomics and portability separately. Use matched workloads and modes; compare the same numerical semantics, hardware, warmup and optimization assumptions. A metadata-erasure theorem can justify low runtime overhead, but does not establish a benchmark result.

A frontend that preserves an operator more faithfully may be valuable before it outperforms a mature implementation. A fast implementation that discards source observables has not won the semantic comparison.

# Local audit artifact

`receipts/design-audit.json` records **40/40 local probes passed**: 22 common-grammar parses with comment invariance, three malformed-syntax rejections, and fifteen scoped mathematical/proof/representation probes. These are not forty proofs of U, not execution of the examples in U, and not a rerun of BiDi's suite. The audit includes a small checked Nat induction proof and an invalid-step rejection under its explicitly listed trusted rules. All artifacts are reproducible with `python audit/check_design.py` after installing the listed local Python dependencies.

The entire U language remains a specification/architecture candidate. The successful probes strengthen particular design decisions and expose counterexamples; they do not silently upgrade the language's implementation maturity.

# Q. Codex execution directive

The following is the complete self-contained build instruction. It is also provided verbatim in `CODEX_EXECUTION_DIRECTIVE.md`.

```text
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
```
