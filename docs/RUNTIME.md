# Stage-0 reference runtime

The runtime directly interprets the U expression tree. It has lexical closures,
explicit operator registration, ordered evaluation, checked scalar and indexed
input boundaries, and a shared evaluation-fuel budget. U text never passes to
Python `eval`, `exec`, a foreign interpreter, a database query-string engine, or
a model service. Python 3.13+ and its standard library are the declared bootstrap
trust base. This is an executable reference realization, not a self-hosted U
implementation or proof of every language-level resource/type law.

`Evaluator(module, capabilities=(), budget=100000)` accepts the parsed module.
`run(entry, args=[])` resolves a definition and executes a function or returns a
value. `evaluate(expr, env)`, `invoke(function, args)`, and
`register(qualified_name, callable)` are the bootstrap interfaces. The
`registry` and `supported_operators()` expose the actual registered operation
surface. CDC and proof operations are registered by the independent `u.cdc` and
`u.proof` modules. Registration alone does not certify a descriptor, adapter,
proof, backend, or arbitrary source-language subsumption.

## Values, errors, and effects

Integers are arbitrary precision; lists, tuples, and dictionaries represent
finite values. A tagged sum is `Sum(tag, value)`. `Option(present, value)` and
`Partial(status, value, steps)` retain their outcome distinctions. Closures
capture their lexical environment. Global value definitions are evaluated on
demand, cached per evaluator, and reject cyclic value initialization. Function
annotations remain syntax until a checker or a runtime interface check uses
them. The proof checker receives the original closure body without executing
its candidate proof.

`RuntimeFault` carries `code`, `message`, and `details`; `.as_dict()` preserves
these fields. Unknown registered-namespace operations raise
`UNSUPPORTED_OPERATION`. Unsupported syntax, missing capabilities, exhausted
fuel, expired lifetimes, shape differences, and invalid domain inputs are
distinct failures. Some operators produce domain outcomes instead: an
incomplete finite search is `unknown`, zero-mass normalization is `held`, and
budgeted partial recursion is `budget_exhausted`, never a completed result.

Console, HTTP, and actor-system operations require capabilities granted to the
evaluator. A capability with copied contents is insufficient: runtime authority
depends on the granted object's identity. Parsing, checking, constructing the
evaluator, constructing a probability model, and constructing a quantum program
perform no console or network operations. HTTP tasks begin their side effects
only when scheduled or awaited with a valid granted network capability.

Fuel is charged at expression/application boundaries and loops in domain
realizations. It is an operational limit, not a uniform machine-cost bound or a
termination theorem. Arbitrary-precision host arithmetic, graph construction,
and numerical kernels have additional size-dependent costs. Host allocation
failure and denial-of-service isolation remain deployment responsibilities.

## Implemented semantic fragments

| Domain | Executed mechanism | Explicit boundary |
| --- | --- | --- |
| Values and recursion | Lexical closures, structural `nat.rec`, lists, records, sums, arbitrary-precision Nat/Int, wrapping eight-bit arithmetic, binary32 rounding and binary64 arithmetic | Recursive function re-entry is rejected; general recursion must use `core.fix_partial`. The reference fixed-point loop realizes tail continuations and returns fuel exhaustion separately. |
| Staging | Quotation preserves body syntax; `bind_once` introduces a fresh identifier outside source identifier syntax; each splice retains its lexical environment; explicit `realize_code` performs stage-one realization | Runtime checks stage boundaries and sharing. Full static `Code(stage, environment, type, effects)` judgments remain checker obligations. No ambient source-text evaluation. |
| Memory | Owning buffers, exclusive scoped mutable views, owner suspension, required view return, epoch invalidation, bounds checks, consumed handles, no await during active mutable borrow | This layer checks live reference objects. Full static affine capture, arbitrary layouts, raw pointers, ABI behavior, atomics and physical memory models are not implied. Root's allocation/provenance model is separately inspectable. |
| Logic | Finite tuples/records/constructor terms, scoped variables, typed primitive variables, occurs checks, conjunction/disjunction, explicit leftmost depth-first ordered answer enumeration | Only the named finite-term search strategy is admitted. Budget exhaustion preserves earlier answers and says `unknown`. Rational trees, cut, tabling and arbitrary foreign Prolog are not supplied. |
| SQL | Finite bags represented with repeated rows; join multiplies multiplicity; projection retains duplicates; `NULL` is `None`; equality and conjunction use three-valued truth | No database engine, transactions, SQL strings, collation suite, or implicit row-order theorem. Iteration order is a reference implementation detail, not SQL bag semantics. |
| Arrays | Rectangular rank/shape checks, paired unique contraction axes, output shape construction, explicit binary64 multiplication followed by left-fold addition | `f64.strict_left_fold` is the admitted contract. No reassociation, hidden BLAS reduction, arbitrary-precision real arithmetic, or GPU hardware claim. |
| Probability | Model regions build normal-binding and observation objects without draws; finite atomic Dirac/weight/bind/normalization; explicit seeded HMC over finite normal models | HMC uses binary64 central-difference gradients, a declared step size and leapfrog count. Results are approximate with acceptance/divergence diagnostics, no convergence certificate. General measurable functions, arbitrary distributions, density disintegration, and exact real normalization are open. |
| Tasks | Lazily scheduled thread-backed actions with pending/running/completed/failed/cancelled states, continuations, await and cancellation request tracking | Running side effects are not rolled back by cancellation. HTTP has a 15-second timeout and an 8 MiB response ceiling. No distributed cancellation or structured-concurrency theorem is claimed. |
| Actors | FIFO message queue, state transitions, child failure, one-for-one restart and a checked restart-intensity window, explicit simulator-time advancement | Deterministic in-process simulator; no operating-system isolation, distributed delivery, physical timing, or full Erlang runtime. |
| Temporal behavior | Explicit finite reachable graph, closure checks, stuttering, always/eventually/conjunction, weak action fairness, fair recurrent-set counterexamples | Scalar finite states and deterministic option-valued transitions. Finite graph verification is not an infinite-state theorem or complete LTL model checker. |
| Hardware | Explicit clock identity, pre-edge register values, simultaneous publication, reset and wrapping bits, input-signal tick bounds | Two-state positive-edge simulation only. No synthesis, device timing, four-state HDL equivalence, or fabricated FPGA receipt. |
| GPU | One-dimensional grid/block enumeration; out-of-range padded lanes never enter a region; each active lane gets its own temporary output lease; input read data is immutable | Scheduled CPU simulation with dependent-length checks and explicit output-owner transfer. No barrier, shared-memory, race model, driver, or accelerator-performance claim. |
| Quantum | One joint complex statevector, consuming H/CX and validated whole-register unitaries, computational-basis measurement, outcome-indexed single-Kraus instruments with completeness checks | Explicit statevector simulation, at most 20 qubits, finite tolerance validation. Conditional statevectors are simulator artifacts. General mixed-state/multi-Kraus-per-outcome channels and QPU execution remain extensions. |
| Acausal dynamics | Symbolic trajectory variables, equations, derivatives and initial conditions remain a DAE object; explicit affine index-1 backward Euler determines differential/algebraic causality and solves each coupled step | Square affine first-derivative systems with consistent differential initial values. Pivot singularity is refused. Step residuals are reported; global error bounds, nonlinear/higher-index DAEs, exact trajectories and physical realization are not invented. |

Array arguments retain the declared shape; GPU buffers retain their dependent
length; qubit counts, bit widths, owning-buffer lengths, list and pair
interfaces, and scalar Nat/Int/Bool/Text/PositiveReal receive dynamic checks.
These selected checks do not replace the graph/type checker. Unsupported
dependent interfaces must not receive a general static soundness claim from
their successful execution.

## Separation of specification and realization

The probability example's `model` returns a `Model` containing latent bindings,
likelihood observations, and an output expression. `prob.infer(..., "hmc", ...)`
selects a numerical procedure and seed separately. Its four chains and 1,000
retained draws per chain are actual sampler outputs when that original example
is run; they are not baked into a fixture.

The quantum example's `experiment` returns `QuantumProgram(region)` without
running gates. `quantum.simulate(program)` executes the region and returns an
`Instrument` with simulator probabilities. Measurement consumes the register
and states whether it discards it or reports a conditional simulator state.
Neither branch probabilities nor complex amplitudes are asserted to be
observations of unknown physical quantum data.

The RC example returns `DAE(variables, equations, initial)`. The explicit
`dynamics.solve(dae, "backward_euler.affine_index1", options)` realization
reports method, step size, trajectory, numerical step residuals and the absence
of an earned global error bound. It first solves the initial algebraic
constraints; initial current is not silently set to zero. Both equations are
assembled into the coupled numerical system at each step.

## Verification

Run `python3 -m unittest tests.test_runtime -v` from the repository root.
Fixtures parse the preserved examples rather than substituting canned ASTs for
their computations. Tests exercise actual parameterized behavior and rival
outcomes: arbitrary-precision arithmetic, lexical capture, nested-borrow
refusal, lifetime expiry, one-evaluation staging, occurs checks, bag
multiplicity, NULL, strict reduction order, input-shape failures, seeded HMC
data sensitivity, zero-mass holds, task cancellation, actor restart limits,
liveness with and without fairness, model closure, simultaneous register
publication, wrapping, padded GPU lanes, dependent GPU lengths, quantum
interference and nonunitary refusal, and decreasing RC error under step
refinement. The native Turing construction executes supplied transitions and
distinguishes halting from fuel exhaustion.

The root's verification receipt supplies fresh aggregate counts and separately
tests CDC/proof/evidence/graph paths. Successful runtime fixtures demonstrate
the named mechanisms within these fragments; they do not close every original
production, self-hosting, source-language preservation, physical backend, or
research theorem gate.
