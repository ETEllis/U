# Small machine-checked invariants

`U.lean` contains 23 Lean theorems about explicit finite and abstract
definitions. The proofs use structural induction, constructor reasoning,
equality substitution and closed arithmetic decisions. There are no admitted
proofs, new axioms, unsafe declarations or external solvers. The file prints
the axiom dependencies of every theorem so that the claim is inspectable.
Under the pinned toolchain, all 23 report `does not depend on any axioms`,
including no dependency on propositional extensionality or choice.

From the repository root, with the pinned toolchain already installed:

```sh
lean +leanprover/lean4:v4.31.0 --version
lean +leanprover/lean4:v4.31.0 formal/U.lean
python3 -m unittest tests.test_formal_mirrors -v
```

Use the explicit toolchain selector; no global Lean default needs to change.
The runtime tests require Python 3.13+ because the pinned CDC realization uses
`math.fma`. Lean itself has no dependency on the Python evaluator.

| Formal object | Earned statement | Limit |
| --- | --- | --- |
| `Graph` | Six constructors with natural-number input/output arities; sequential occurrence projection associates and scope preserves it | Arity is not U's full interface/type system. Generator projection is a structural traversal, not a denotation or fixed-point execution. |
| `runPath` | A concatenated list applies exact functions in source order; grouped concatenations give the same application; reversing source order can change results | This does not authorize reassociating floating-point matrix products or prove the CDC tangent implementation. |
| `Trit`, `PrefixValid` | Polarity inversion is involutive and negates each trit; an ordered sequence and its inverse are both prefix-admitted from zero exactly when every trit is zero | The aperture theorem begins with exact trits. It does not prove cosine quantization, deadband boundary behavior, covering geometry, or automatic polarity equivalence of CDC trajectories. |
| `AdmittedSplit` | A partition of a list of distinct nominal exclusive tokens cannot place the same token in both branches | Distinctness and partitioning are hypotheses. This does not establish the runtime allocator, borrowing checker, arbitrary memory model, or real-world capability authority. |
| `AccessState` | Borrowing suspends the owning state; a stale epoch cannot return a borrowed resource | This is a small transition model, not a full ownership/lifetime soundness theorem. |
| `Analysis` | An upstream hold invokes no continuation, produces no value, and adds no artifacts; a downstream hold preserves prior artifacts in order | The transition rules are defined here. They do not prove Python `analysis.then`, evidence authenticity, journal durability, or distributed consensus. |
| `Compatible` | A nonempty set of evidence entries that is compatible with two path identities forces those identities to be equal | Natural numbers model nominal path identities. No collision-resistance or cryptographic authentication theorem follows. |

The source-order counterexample and the trit-prefix counterexample are
mechanized negative witnesses: the library does not quietly assume parallel
commutativity or discard order-sensitive prefix checks. The trit aperture is
an exact conditional result, preserving CDC's distinct zero value rather than
turning zero into a rejected or held result.

The four Python tests exercise related live behavior without reimplementing a
second copy of the operator algorithms. They enumerate every nonempty trit
sequence through length six (1,092 sequences) and its inverse; verify that held
commits preserve existing latches; ensure analysis holds cannot execute a
downstream constructor; and check ordered retention of earlier artifacts.
These are empirical bridge checks, not a proved refinement from Python to
Lean. A full simulation relation, resource-sound graph checker, preservation
theorem and verified backend remain separate work.

The root verification receipt records fresh command results. A successful Lean
invocation checks these 23 declarations under the specified toolchain; it does
not by itself change any broader release gate to complete.
