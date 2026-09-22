# Native execution

U source compiles to native functions through the compiler in [compiler/compiler.u](../compiler/compiler.u). The normal launcher starts with a checked generated-C seed when no local compiler exists. Python is confined to the optional independent seed and development verification.

The execution path is:

```text
U source → U parser → six-constructor graph → validated reconstruction
         → linked U libraries → U C emitter → platform compiler → executable
```

C is the machine target and the implementation language of the disclosed generic [platform bridge](../native/ABI.md). No runtime routine interprets U syntax trees or delegates U bodies to a foreign evaluator.

## Values and lifetime

Integers are arbitrary precision. Binary64 arithmetic preserves explicit operation order, with fused operations requested explicitly. Text is UTF-8; byte-oriented operations and file hashing have separate contracts. Lists, tuples, records, closures, cells, tables and opaque handles remain distinct.

Functions close over lexical environments. Each let extends the environment instead of mutating an earlier binding visible to a captured closure. Top-level values initialize lazily and memoize; initializer cycles fail. Runtime annotation guards check supported concrete carriers. A nominal or dependent annotation still carries its unresolved obligations.

The process arena has a default 1 GiB payload budget and is released at process exit. This is not tracing garbage collection and is not a bound on total operating-system RSS. Compiler passes use bounded caches and shared descriptors. HMC runs bounded, unprivileged batches in isolated workers so temporary inference allocations can be reclaimed while the exact random-stream position and Markov state continue. The original four-chain workload completes with 1,000 retained draws and 500 warmup transitions per chain; a shorter run verifies exact agreement with the unbatched seeded stream. Completion and stream agreement do not establish statistical convergence.

## Effects and failures

Platform access is granted explicitly through supported capabilities: console, filesystem read/write, subprocess execution, environment access, network and actor facilities. Compilation privileges are not inherited by a generated program. Subprocess and worker paths attenuate their grants.

A capability is not an OS security sandbox. Granting arbitrary filesystem or subprocess access permits the corresponding real actions. The [threat model](THREAT_MODEL.md) identifies the trusted boundary.

User execution has a call-dispatch budget. Worker trees share that budget, so a child cannot exhaust it and manufacture a successful parent result. Allocation failure, invalid continuation use and exhausted execution budgets are terminal refusals. Catchable ordinary faults do not roll back earlier effects.

## U libraries

- `core.u`: collection algorithms, structural Nat recursion, options and sums.
- `relational.u`: finite terms, occurs checks, ordered search, bag joins and NULL.
- `numeric.u`: strict contraction, complex arithmetic and consuming joint-statevector handles.
- `probability.u`: measure construction, likelihood weighting and seeded numerical HMC.
- `syntax.u`: compiled-region quotation, splicing and single-evaluation binding.
- `proof.u`: closed total Nat/Pi/Eq judgments and checked certificates.
- `evidence.u`: typed canonical identities, dependencies and admission accounts.
- `processes.u`: tasks, shared results, cancellation, HTTP, actors, temporal checks and resource/simulator contracts.
- `dynamics.u`: affine DAE coefficient extraction and backward-Euler integration.
- `cdc/`: pinned finite reductions, source import and guarded analysis.

Each scientific realization retains its assumptions. Numerical inference is not a convergence proof. Finite temporal checking is not coverage of arbitrary schedules. HDL, GPU and coherent-state simulation do not establish physical device execution.

## Independent execution

A named native build is standalone and accepts a JSON array as its first argument:

```sh
./bin/etellis-u build examples/original/02_arithmetic.u --entry square --output build/square
./build/square '[123]'
```

The build receipt binds source, graph, library, compiler, runtime and executable identities. Artifact audit checks those identities against current bytes; it does not authenticate a signer or prove the recorded proposition.

The U WASM emitter supports exact integer expressions and Boolean predicates over explicitly certified signed-i64 intervals. It is a narrower target than the general native compiler.
