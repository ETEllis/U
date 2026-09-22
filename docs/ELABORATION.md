# Source, graph and checking

The native frontend is written in U: [compiler.u](../compiler/compiler.u), [graph.u](../compiler/graph.u) and [format.u](../compiler/format.u). The independent historical parser is retained only under [bootstrap](../bootstrap/reference_source.py).

## Source contract

The scanner validates UTF-8 source, strings, escapes, number forms, delimiters and configured limits. Parsing checks version headers, declarations, duplicate bindings/fields and required result expressions. Coordinates use byte offsets; editor positions use UTF-16.

Function definitions carry their parameter list separately from value definitions. Local initializers see the preceding lexical scope. Rebinding in a nested scope does not alter an earlier captured environment. The formatter preserves comment, literal and numeric lexemes, is idempotent on the verified corpus and performs no source effects.

## Six constructors

| Form | Structural account |
|---|---|
| Bound reference | wire to an explicit lexical binding |
| Literal or operation | gen with its declared value or qualified operation |
| Ordered calls and statements | seq retaining source evaluation order |
| Fresh parameter or let | scope retaining binding and interface information |
| Explicit recursion | fix with its declared iteration discipline |
| Literal-only operand grouping | par with a checked empty-resource-footprint account |

Generators do not conceal executable source trees. Declared type syntax remains interface metadata. Source offsets are auxiliary provenance. The current structural identity also retains lexical name hints, exported names, field order, call order, literal encodings and profile contracts; it does not claim invariance under arbitrary renaming or semantic equivalence.

`gg_reconstruct` validates identity and constructor contracts, reconstructs definitions from the graph and relowers them for comparison. The production compiler uses these reconstructed user bodies. Exact export additionally compares a separately parsed source residual with the graph.

## Validation is scoped

`cc_validate` checks lexical names, ordinary function interfaces, supported concrete carriers and generated runtime guards. Unknown operations in unshadowed native namespaces are rejected. Ordinary records may shadow a namespace without becoming native operations.

Domain and dependent annotations can retain obligations. Consuming library handles enforce their supported rules at runtime. Complete static ownership and effect analysis remains open. A successful native execution does not prove Copy, Drop, independence or a dependent refinement merely because an annotation names it.

The graph records unresolved theory and resource requirements. Its structural identity is not a proof of global semantic equivalence. The total Nat/Pi/Eq kernel checks proof terms separately from ordinary compilation.

All 22 preserved examples are covered by native parse, formatting and graph round-trip checks. The self-compiler and formatter are also checked for cold-process formatting idempotence and independent syntax equality. Execution evidence is recorded separately in the [native verification report](../artifacts/native-verification.json).
