<p align="center"><img src="landing/mark.svg" alt="U: an open operation with explicit interfaces" width="112"></p>

<h1 align="center">U</h1>
<p align="center"><strong>Law-bearing operations</strong><br>One composition discipline. Explicit mathematical worlds.</p>
<p align="center"><a href="landing/index.html">Explore</a> · <a href="#run">Run</a> · <a href="docs/ARCHITECTURE.md">Architecture</a> · <a href="paper/main.pdf">Paper</a> · <a href="CLAIMS.md">Evidence boundary</a></p>

U is a programming language and semantic substrate in which an operation carries the laws governing how it can be connected, observed, repeated, or shared. Its interface includes the resources it uses, the theory that gives it meaning, and the evidence required to authorize its consequences.

The ambition is substantial: express values, memory, relations, probability, concurrent behavior, continuous dynamics, and quantum processes in a common inspectable representation, with native rules and explicit bridges between their mathematical worlds. Existing languages should be lifted into that representation without quietly discarding their meaning or their recoverable source.

This repository begins that implementation. Its reference bootstrap uses Python 3.13+ to parse and evaluate U directly. CDC compatibility uses explicit fused multiply-add operations to match its pinned native numeric profile. It is a research implementation with deliberately visible support boundaries; the complete production, foreign-language, specialized-backend, formal-correspondence, and self-hosting gates remain in the [original directive](provenance/source/CODEX_EXECUTION_DIRECTIVE.md) and the current [claim ledger](CLAIMS.md).

The verified local release currently has **121 passing tests**, **96 exact-bit CDC primitive oracle comparisons**, **23 Lean theorems with no axiom dependencies**, and an **18-page formal manuscript**. All 22 preserved examples parse and reconstruct from the operation graph. These are distinct achievements with the scopes recorded in [fresh verification](artifacts/verification.json), [CDC parity](artifacts/cdc-parity.json), and the [formal definitions](formal/README.md).

## The central distinction

An equation is not its numerical solver. A probability model is not a particular sequence of samples. A proof candidate is not a checked theorem. A classical circuit simulation is not physical quantum execution.

U preserves those distinctions in three connected objects:

| Object | What it records | What it can establish |
|---|---|---|
| **Meaning** | Typed operation graph, semantic profile, observables, numeric contracts, dependencies and assumptions | Which behaviors an operation permits and which transformations are legal |
| **Realization** | Evaluator, compiler, solver or device implementation and its exact contract | How a selected operation is carried out in a particular environment |
| **Evidence** | Subject identity, checker or runtime, inputs, result, scope and remaining obligations | What was actually checked, executed, refused or left conditional |

A backend can change without changing meaning only when every meaning-relevant contract remains fixed. A different precision, event order, observation policy or rounding rule may change the meaning itself.

## Six constructors; no hidden semantic bargain

The structural graph has six tags:

```text
wire   forward or permute existing ports
gen    apply an admitted operation with a resolved contract
seq    connect compatible operations in order
par    compose operations with a checked independence witness
scope  bind a fresh name or region with escape restrictions
fix    iterate under a declared recursion or feedback discipline
```

These tags make the structure small. They do not make the mathematical obligations disappear. A `gen` node is inspectable: the operation's signature, laws, model, rule or expansion, assumptions and implementation obligations remain separately accountable. A large foreign interpreter behind one node does not earn native semantic support.

`wire` cannot copy a linear resource. `par` does not prove physical simultaneity. `scope` does not allocate a device. `fix` does not allow a nonterminating computation to manufacture a proof.

Seven theory families retain the laws that distinguish their domains:

| Family | What it owns | A distinction that must survive |
|---|---|---|
| **V — values and proofs** | Data, closures, total proof terms, staged syntax | Checked proof versus partial computation |
| **M — resources and memory** | Ownership, borrowing, allocation identity, access | A resource versus its current contents |
| **R — relations and search** | Unification, multiplicity, queries, search policy | Relation versus the order in which answers are found |
| **P — measures and probability** | Measures, weighting, conditioning, inference | A distribution versus an observed sample |
| **C — behavior and clocks** | Causality, messages, schedules, registers | Possible behavior versus one executed trace |
| **D — paths and dynamics** | Equations, trajectories, events, derivatives, frames | Continuous specification versus a finite numerical map |
| **Q — channels and instruments** | Joint quantum state, gates, measurement | Coherent state versus classical mixture |

These are obligation families, not a proof that seven theories or six constructors are globally minimal. See [operator contracts](provenance/source/OPERATOR_CONTRACTS.md) for the original accounting and [architecture](docs/ARCHITECTURE.md) for the implementation boundaries.

## Run

Use the qualified command `etellis-u`. U and the `.u` extension have existing naming collisions; this project is **ETEllis/U**, with the language identifier **`etellis.u`**. The repository does not globally take ownership of `.u` files.

```sh
git clone https://github.com/ETEllis/U.git
cd U
python3 --version                 # Python 3.13 or later
./bin/etellis-u --help
./bin/etellis-u run examples/original/02_arithmetic.u --entry square --args '[7]'
```

The source surface uses ordinary UTF-8 text, a versioned header, named imports, definitions, calls and typed regions:

```u
u "etellis.u/0.1";
use "u.standard/0.1";

def square(x: Int) -> Int = int.mul(x, x);
```

The original [22 examples](examples/original/) cover the design's full breadth. They are preserved specimens with individually reported support; their presence does not mean every operation or realization is implemented. Consult the CLI's supported commands and [CLAIMS.md](CLAIMS.md) before choosing an execution path.

The command above returns `49` with a source-, meaning- and realization-bound execution receipt. To inspect the elaborated operation and its obligations, or check the supported natural-number induction theorem:

```sh
./bin/etellis-u explain examples/original/02_arithmetic.u
./bin/etellis-u prove examples/original/14_proof.u --entry checked
```

The latter independently checks the closed Nat/Pi/Eq induction judgment that `0 + n = n`. Its proof result and the broader frontend's unresolved dependent-type obligations are reported separately. This earns that theorem in the supported checker, not a complete dependent type system.

Parsing and checking do not authorize ambient effects. Execution that writes to a console, filesystem, network or device needs the corresponding supported capability. An unsupported operation produces an explicit diagnostic or held obligation rather than a successful-looking placeholder.

Run the full local release gate with Python 3.13, a C compiler, Node.js, the pinned Lean toolchain and Tectonic:

```sh
./scripts/verify.sh
U_PYTHON=python3.13 ./scripts/verify.sh --full
./bin/etellis-u run examples/original/03_fibonacci.u --entry fib --args '[100]'
```

The Fibonacci result is `354224848179261915075`. `eval` and `--reference` allow explicit reference execution of supported domain operations whose broader static checking is still incomplete. The full gate compiles the independent pinned CDC oracle, checks Lean and builds the paper. The ordinary suite reports missing C/Node tools as visible skips; the full command requires its tools. The oracle source is vendored byte-for-byte only for differential testing, so CI needs no credentials for the separate private BiDi repository.

Compile a bounded exact-integer fragment into independently executable native code or WebAssembly:

```sh
./bin/etellis-u build examples/original/02_arithmetic.u --entry square \
  --target wasm --bounds '[[-1000000,1000000]]' --output build/square.wasm
./bin/etellis-u build examples/original/02_arithmetic.u --entry square \
  --target native --bounds '[[-1000000,1000000]]' --output build/square-native
./build/square-native 721
```

The compiler certifies every intermediate and guards the input domain. Other graph fragments remain reference evaluated. For the landing page, serve the repository with `python3 -m http.server 4287 --bind 127.0.0.1` and open `http://127.0.0.1:4287/landing/`.

## What makes composition lawful

Two operations can each make sense while their composition does not. U requires either one admitted composite profile or a checked adapter at the boundary. A readable program need not spell every adapter by hand: deterministic elaboration must emit the same explicit graph witness that an explicit spelling would require.

Consider a fair coin and a nondeterministic decision. Choose a bit before seeing the coin, and the best success probability is one half. Observe the coin before choosing, and it is one. The operation names are identical; access to information changes the result. Reordering them requires a law that this example does not satisfy.

An adapter therefore records an interface map, a preservation relation, the observable information, assumptions, loss, checker identity and residual policy. Exact, refining, approximate and unsupported bridges remain different categories. An approximate numerical solver must carry its norm and error assumptions; a local tolerance setting is not automatically a global error theorem.

## Resources, proof and trust

The first independence discipline is intentionally conservative: split owned resources, permit shared reads only through an explicit witness, and reject overlap with a writer. Two different variable names do not prove two different allocations. A causal edge establishes an ordering constraint, not independence.

Admission has several simultaneous dimensions:

- origin: primitive, derived, library or imported;
- support: reference, native compiler, simulator, external device or unsupported;
- evidence: checked laws, executed cases, assumptions and open obligations;
- trust: the exact checker, implementation and external dependencies.

An operator may be an admitted primitive, have one checked law and one unresolved assumption, and still lack a device backend. One status badge cannot express that object accurately.

The proof boundary is similarly explicit. Search and tactics may propose proof terms; only the supported total checker may accept them. Unrestricted recursion, a solver's success message, an arbitrary JSON record or an unsafe cast cannot confer proof authority. The paper's mathematical arguments and an implementation's tests remain distinct evidence.

## CDC and BiDi: the lineage matters

[BiDi](https://github.com/ETEllis/BiDi) was built first. Its Coherence-Delta Calculus (CDC) supplies a concrete native language, source contract, state reducer and evidence discipline. U is a later abstraction that develops a more general substrate beneath that experience. CDC was never secretly implemented in U.

The compatibility reference is BiDi commit `1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157`: release 0.3.0, grammar 1, C ABI 1.5. U does not rename or replace the `cdc` command, `.cdc` source, ABI, C symbols or proof namespaces. A U realization must earn its own parity against that independent implementation.

CDC's primitive reductions remain specialized operations:

- `flow(d)` performs the pinned synchronous finite phase update;
- `commit(m)` quantizes to balanced ternary and applies its prefix barrier;
- `nest(parent, child)` performs the pinned belief/prior exchange.

U1 adds guarded lifted-cover acceptance. U2 differentiates the executed maps, then independently checks recurrence before authorizing a return operator and spectrum. A valid tangent does not prove recurrence. A held spectrum does not erase an already earned return operator. Current CDC flow is a finite map and does not automatically inherit exact ODE flow laws.

The [formal paper](paper/main.pdf) develops these distinctions mathematically. The [BiDi crosswalk](docs/ARCHITECTURE.md#bidi-crosswalk) connects the current source language, formal spine, frameworks, verification matrix, toolchain history, U2 and RFTC to U's corresponding obligations. BiDi's native C and historical self-hosting program do not make U's Python stage-0 implementation self-hosted.

## Native possession and faithful import

Native support requires more than reproducing an output. For each claimed fragment, U must expose its operations, compose their meanings, execute direct U rules or checked expansions, analyze them, preserve recoverability and survive differential and adversarial checks.

Import has the shape:

```text
source → typed U graph + residual + provenance
```

Residuals retain lexical details, names and intensional choices, unsupported semantic regions, source environment and approximation information. Exact byte reconstruction is one useful property. Semantic preservation is a separate relation over the selected source and target observations. Keeping the original text can satisfy reconstruction while providing no native understanding at all.

Transformations must update that residual through a valid reconstruction rule or invalidate exact export. A digest of deleted history is not the history, and a source hash is not a semantic equivalence decision.

## Verification and release boundaries

The build follows the original directive's complete release program: foundational contracts; executable values, resources and proof; CDC compatibility; theory conformance; semantic ingestion and transformations; production classical execution; specialized backends; stronger formal correspondence; self-hosting with an independent oracle.

The current evidence inventory is [CLAIMS.md](CLAIMS.md). It must distinguish:

```sh
./bin/etellis-u test
```

The fresh [CDC compatibility receipt](artifacts/cdc-parity.json) records the exact pinned profile and individual comparisons. The complete release report is maintained under `artifacts/`; an invocation that fails or reports unsupported work must retain that result.

| Evidence | Earns | Does not automatically earn |
|---|---|---|
| Parse / elaborate | Accepted syntax and a resolved representation | Execution or complete type soundness |
| Reference execution | One result under a named interpreter and profile | All-schedule equivalence or production performance |
| Differential test | Agreement on the exercised cases | Full-language equivalence |
| Mathematical proof | Its exact statement under its assumptions | Correctness of an unconnected implementation |
| Native / WASM parity | Agreement for the admitted compiled fragment | Full compiler coverage or self-hosting |
| Device simulation | Its declared simulator behavior | Physical execution or physical advantage |

The package's [paper](paper/main.pdf) proves scoped structural and evidence results on explicitly stated mathematical models. Those arguments do not silently mechanize the Python implementation. Fresh verification receipts, unsupported paths and remaining release gates remain visible even when a narrower path passes.

## Repository map

```text
u/                 source, graph, checker, direct evaluator and theory modules
bin/               qualified command-line entry point
examples/original/ the 22 preserved design examples
tests/             mechanism, negative and integration tests
docs/              implementation contracts and design decisions
landing/           local research page and explanatory instrument
paper/             formal paper, bibliography, PDF and build instructions
provenance/        byte-preserved supplied sources and review reconciliation
```

The original missing grammar, audit checker, source ledger and structured package were not recovered. New implementation files are new work and are not represented as those historical assets. The five supplied source files and the recovered examples retain their preservation history.

## Research position and citation

U builds on established work in typed intermediate representations, algebraic effects, resource-sensitive reasoning, open-system composition and bidirectional transformations. Its research question concerns the integrated protocol: can one practical language make laws, realization, preservation, residuals and earned consequences explicit enough to support real cross-domain work?

No claim of universal subsumption, globally minimal foundations, universal performance superiority, physical law or quantum advantage is made. Those would require different evidence from this implementation.

The [arXiv-intended manuscript](paper/main.pdf), [LaTeX source](paper/main.tex), [bibliography](paper/references.bib) and [citation metadata](CITATION.cff) are included. It is a manuscript prepared for author review, not an arXiv submission or an endorsed publication. Edward Ellis is the author; AI-assisted design, implementation and drafting are disclosed in the manuscript.
