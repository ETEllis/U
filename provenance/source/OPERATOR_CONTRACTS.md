# Initial operator contracts and semantic accounting

Status: proposed signatures/rules. This is the generator/derived-node accounting seed, not a claim that the operations below have been implemented. Resource/effect indices omitted from compact signatures must be explicit in the machine schema.

## Shared descriptor requirements

Every entry must name its input/output interfaces, static parameters, region binders and stages, capability/effect requirements, denotation, evaluation or expansion rule, observables, failure/HOLD conditions, applicable laws, conformance tests and trust dependencies. A derived node must point to a checked immutable expansion. Backends implement these contracts rather than define their meaning accidentally.

## V: constructive values and total proof/type terms

Formation/checker rules: stratified Sort(i), Pi(usage,A,B), Sigma(A,B), strictly positive inductive declarations and their eliminators, identity type Eq(A,a,b), reflexivity, equality elimination, and Code(stage,environment,type,effects). Strict positivity, universe constraints and totality are checked. Opaque domain interfaces are indexed types admitted by a theory descriptor, not arbitrary unsafe casts.

| Operation | Signature / meaning | Boundary |
|---|---|---|
| nat.zero | Unit -> Nat | Exact arbitrary-precision semantic natural. |
| nat.succ | Nat -> Nat | No overflow; bounded realizations require checks/other types. |
| nat.rec | Nat × A × (Nat × A -> A) -> A | Structural induction/recursion; total if body is total. |
| v.close | Captures Γ/Δ plus typed region -> indexed Closure | Affine captured resources make the closure affine. |
| v.apply | Closure(A,B,e) × A ->[e] B | Capture-avoiding substitution/environment evaluation. |
| v.copy/v.drop | A -> A×A / A -> Unit | Only with admitted Copy/Drop witnesses. |
| inductive.case | Sum(A,B) plus branch regions -> C | Only chosen branch executes in strict computational mode. |
| eq.refl / eq.elim | Standard identity introduction/elimination | Total proof kernel only; no truth from unchecked bytes. |
| core.fix_partial | ((A -> Partial(B)) × A -> Partial(B)) × A -> Partial(B) | Explicit partiality and unbounded-in-principle unfolding. |
| syntax.quote | Staged typed region -> Code(A) | Captures body syntax at stage+1; does not execute it. |
| syntax.splice | Code(A) into an admitted quotation region | Stage/binding/type checks; not general ambient eval. |

Integer arithmetic, records, lists, ordinary map/filter/fold, macros, structural type transforms and most array operators are derived definitions or specialized realizations over these foundations. Their high-level names remain explicit, inspectable nodes with expansion witnesses. Fixed-width arithmetic and strict IEEE arithmetic have different contracts.

## M: resources and memory

A reference memory state maps fresh allocation identities to byte ranges, initialization bits, layout and live epoch, plus access/ownership facts. Virtual addresses belong to an ABI interpretation of those identities. No theorem about raw-address equality automatically proves allocation identity.

| Operation | Contract |
|---|---|
| mem.alloc | Capability and layout/length -> fresh Owned region; allocation failure explicit. |
| mem.read | Admitted read access plus typed offset/layout -> value; alignment, initialization, provenance and range checked by profile. |
| mem.write | Exclusive or otherwise lawfully synchronized access plus value -> updated access/resource. |
| mem.free | Consume owning handle; invalidate epoch; outstanding incompatible borrows make it inadmissible. |
| mem.borrow / reborrow / return | Suspend/restrict access and discharge lifetime/escape constraints; never fabricate a lease or physical resource. |
| mem.atomic_load/store/rmw/fence | Add events under a named order/scope/model; no sequential-consistency default for foreign code. |
| unsafe.raw_access | Explicit unsupported/unsafe source profile with conditional preservation; not part of the safe-resource theorem. |

Durable logs/transactions, file handles, process descriptors, ABI calls and cryptographic capabilities add derived protocols and backend obligations. Commit-on-disk and externally anchored authenticity are distinct.

## R: relations, constraints, multiplicity and search

Finite-term unification uses standard typed substitution rules: identical terms discharge; an unbound variable can be bound only when sort/occurs constraints pass; equal constructors recurse on corresponding arguments; unequal constructors fail. Rational-tree mode deliberately changes the occurs/representation contract and must be separately named.

| Operation | Contract |
|---|---|
| logic.fresh / exists | Fresh scoped logical identity; repeated references name the same unknown. |
| logic.unify | Terms × substitution -> constraints/substitution or failure under explicit term universe. |
| logic.and / or | Constraint composition; operational search order recorded when observable. |
| logic.resolve | Ordered clauses/goals -> answer stream under selected strategy; not arbitrary source-text execution. |
| logic.cut | Delimited commitment to a search branch under an explicit source profile; cannot masquerade as pure relational conjunction. |
| relation.lfp | Least fixed point of a monotone relation transformer; monotonicity/finite convergence not assumed without evidence. |

SQL join/project/group and APL shape/rank operators are retained derived-native nodes, not source strings. A finite bag can be modeled as a finite-support map row->Nat; join multiplies multiplicities and union adds them. NULL predicates/collations/numeric conversions retain the selected SQL profile. Constraint solvers return Proven/Refuted/Unknown or model/countermodel evidence; a timeout is not logical false.

## P: measure theory and inference

| Operation | Contract |
|---|---|
| prob.dirac | A -> Measure(A) with concentration at A. |
| prob.bind | Measure(A) × (A -> Measure(B)) -> Measure(B), subject to admitted measurable composition. |
| prob.weight | Nonnegative measurable likelihood × Measure(A) -> unnormalized Measure(A). Densities may exceed 1. |
| prob.normalize | Requires strictly positive finite total mass; otherwise an explicit undefined/held obligation. |
| prob.sample | In a model region: a binding in the model's measure algebra. In an execution realization: a recorded random operation. Those are different contexts. |

Distributions have mathematical definitions and separately chosen numeric samplers. HMC/VI/importance sampling are library/algorithm realizations with error/diagnostic assumptions, not new definitions of probability. Conditioning on zero-probability observations needs the declared density/disintegration convention; it is not division by zero with a fallback.

## C: behaviors, clocks and execution structure

| Operation | Contract |
|---|---|
| event.emit / channel.send | Introduce events with explicit causal edges and channel contract. |
| channel.receive / wait | Consume/select an enabled message under mailbox/order/timeout semantics; can legitimately block. |
| behavior.choose | Preserve a set of possible transitions or the selected execution decision; no probability implied. |
| behavior.spec | Initial predicate, transition relation and fairness/observation contract -> behavior set; stuttering explicit. |
| clock.reg | Clocked input/next-state region with previous-tick state; only publish under declared event regions. |
| process.spawn / fail / monitor | Resource-scoped process creation/failure/observation; supervision is a derived protocol. |
| geometry.launch | Grid/block/lane and resource partition -> accelerator process family; memory/synchronization scope retained. |

Temporal always/eventually and fairness are predicates on behavior sets; checking a finite graph and proving an infinite-state theorem are different realizations. Physical clocks, simulator time, event order and serialized trace order are different indexed interfaces.

## D: paths, differential equations, frames and topology

| Operation | Contract |
|---|---|
| dynamics.path / equation / dae | Typed path/equation object with domains and initial/boundary conditions; no solver result invented. |
| dynamics.derivative | Derivative expression or checked differentiable transformation under declared regularity. |
| dynamics.solve | Numerical or symbolic realization with explicit method/error/domain assumptions; not exact arbitrary-real execution. |
| hybrid.guard_reset | Hybrid transition with localization, priority, transversality and reset conditions. |
| frame.transport | Declared coordinate/interface map, domain and derivative where needed; partial/coarse maps are not automatic isomorphisms. |
| topology.lift / winding | Path-lifting/invariant operation under an explicit cover/orientation/domain; numeric sample sufficiency must be justified. |

AD over finite operation graphs is a derived transformation whose local Jacobian rules must describe the actual primal realization. A symbolic differentiator of a different theoretical function does not satisfy the contract. CDC's finite `flow` and actual `nest` derivatives are critical conformance cases.

## Q: channels and instruments

| Operation | Contract |
|---|---|
| quantum.zero | Create a specified joint |0...0> interface using explicit semantic initialization; physical allocation/reset requires a device realization. |
| quantum.unitary | Apply U rho U† after dimension/unitarity validation or a trusted gate construction; consume/return the joint linear resource. |
| quantum.instrument | Completely positive outcome-indexed maps whose sum satisfies the trace contract. |
| quantum.measure | Instrument returning classical outcome and declared post-measurement resource/discard policy. |
| quantum.reset / discard | Irreversible channels; not unitary inverses. |

H/CX and circuits are named derived gate families over these contracts. A simulator density/statevector is not physically inspectable unknown quantum data. Hardware execution additionally binds provider, device/job, circuit lowering, calibration/timing and result provenance.

## Derived, not new universal primitives

CDC flow/commit/nest; SQL joins; APL rank/scan; Erlang supervision; HTTP orchestration; Rust-style traits; Haskell-style abstraction; hygiene-preserving macros; U1 closure and U2 tangent/recurrence/spectrum; persistence and authority protocols all live above the structural kernel and appropriate theories. Each must earn native status separately. This ledger is intentionally larger than six keywords: semantic obligations cannot be eliminated by putting them behind a generic node constructor.
