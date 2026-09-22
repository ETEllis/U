# U implementation contracts, version 0.2

The source profile remains `etellis.u/0.1`. Implementation, graph, evidence, package and ABI versions are independent.

## Source and compilation

[compiler/compiler.u](../compiler/compiler.u) owns scanning, parsing, lexical resolution, core checks, closure conversion and C generation. The source grammar is [grammar.ebnf](../spec/grammar.ebnf). Parsing does not execute definitions, type annotations or import hooks.

Function and value definitions are distinct. Names resolve simultaneously at module scope; local let initializers see their preceding scope. Captured environments preserve earlier bindings. Values initialize lazily and memoize. An initialization cycle is an error.

The formatter preserves comment, string and numeric lexemes and reparses its output. Native source coordinates are UTF-8 byte offsets; the language server translates them to UTF-16 positions.

## Graph and checking

[compiler/graph.u](../compiler/graph.u) lowers executable bodies to wire, gen, seq, par, scope and fix. It retains explicit lexical bindings, qualified operations, interfaces, order, profiles and obligations. Declared type syntax is interface metadata, not a hidden executable source payload.

Reconstruction checks graph identity, roles, bindings and arities, then relowers the reconstructed structure. Export independently reparses the retained source and checks its structural identity against the graph. A source hint is not authenticated by its presence.

Core validation checks supported concrete carriers and binding structure. Closed platform namespaces require registered qualified operation names unless an ordinary lexical binding shadows the namespace. Domain indices, refinements, stages and ownership may retain explicit obligations. Execution does not silently convert those obligations into proofs.

## Proof and authority

[stdlib/proof.u](../stdlib/proof.u) supports Nat, dependent Pi/lambda/application, equality, reflexivity, congruence and Nat induction in its admitted total fragment. Addition recurses on its second argument. Partial recursion, IO and numerical solvers have no proof-conversion rules.

Proof receipts bind the checked term and inferred judgment to an opaque local certificate. The command `prove` requires that certificate. JSON records, type names and receipt hashes cannot manufacture it. Resource exhaustion refuses construction rather than establishing a mathematical result.

CDC's analysis handles additionally use private issuance registries. Path, tangent, recurrence and return artifacts must refer to the same execution and permitted transformation.

## Runtime and effects

The general native target executes arbitrary-precision integers, lexical closures and supported U library operations. The C bridge owns generic storage, scalar operations, function calls, codec and OS boundaries, not U syntax execution.

Explicit capabilities gate platform effects. User executables do not inherit compiler grants. Workers have private mutable memory, transport only supported ordinary values and share the execution budget. Unsupported resource transfer fails.

Memory is bounded by a retaining process arena. There is no general tracing collector or hostile-host isolation claim. See [RUNTIME.md](RUNTIME.md) and [THREAT_MODEL.md](THREAT_MODEL.md).

## Identities and packages

Evidence profile 2 uses typed, length-framed canonical encoding. Exact integers, binary64 bit patterns, Text, Unit, lists, tuples and records remain distinct. Record keys are sorted in U. Domain separators and profile versions participate in identities.

Graph structural identity, evidence identity and build identity have their own schemas. None is a decision procedure for extensional semantic equivalence or authenticity.

Package schema 2 binds sorted `.u` files and `U.toml` members to their exact bytes and a typed snapshot identity. Dot-prefixed entries, `build`, `dist`, `work` and `provenance` are excluded. Symlinks in the source traversal and path components are rejected. Installation verifies the lock, rechecks bytes while copying, verifies the staging tree and publishes it by one rename. Package scripts never execute and manifest permissions confer no grants. The contract is trusted-local; network resolution, hostile filesystem races and crash-fsync durability are outside its scope.

## CDC and scientific realizations

The compatibility authority remains BiDi commit `1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157`. The finite source importer retains ordered attributes, native first-value selection, registry last-value metadata and exact source. Unsupported directives are explicit.

Flow uses a pre-step snapshot and source-ordered channel accumulation. Explicit fused multiply-add operations match the selected pinned numeric profile. Commit protects its current latch writes on a negative prefix; earlier state remains. Nest writes the updated parent belief into the child prior.

Local Jacobians differentiate the real extension of the executed finite map on a fixed itinerary, not IEEE rounding itself. Recurrence checks the full selected continuous and discrete state. Uniform integer phase restoration includes its action and derivative. A general dense spectral problem or incomplete U1 binding yields a hold with earlier earned artifacts retained.

The DAE, probability, temporal, GPU, clocked and coherent-state libraries expose bounded realizations. A numerical result, finite-model check or simulator run earns only its declared scope.
