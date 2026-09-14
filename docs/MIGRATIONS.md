# Version and migration contract

The language header, semantic profile, graph encoding, ABI, package and implementation version are distinct. This release is the initial `etellis.u/0.1` source profile and stage-0 0.1.0 implementation. No earlier development receipt is a release migration guarantee.

A semantic behavior change creates a new profile or explicit migration relation. In particular, changing precision, multiply/add contraction, observable event order, resource permissions, conditioning convention or refusal policy cannot silently reuse an incompatible semantic identity.

Raw source bytes, formatted syntax, structural graph, semantic candidate, realization, execution and resource identities have different scopes. Original bytes remain provenance after a transformation. Export after graph change requires a checked residual update; absent that update it is refused.

The initial named total checker refuses binder shadowing. Future alpha-renaming support, Sigma/inductive extensions or new conversion rules require explicit checker identity and proof trust changes. Generator registration does not modify the proof kernel.

CDC compatibility retains the original BiDi pin and numerical contract. A future more mathematical ODE profile must be separately named. U is not a user-facing replacement imposed on `.cdc`, and the existing BiDi implementation/history remains independent.
