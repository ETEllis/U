# Authority and implementation decisions

1. Edward's 2026-09-14 instruction authorizes the complete build, new GitHub repository, substantial README/landing page, formal paper and commits. It supersedes the prior review-only boundary.
2. The preserved GPT-6 Pro dossier and execution directive define the end state. `provenance/review/RECONCILIATION.md` qualifies both reviews and is the final review authority.
3. `docs/SPEC.md` defines the implemented reference contracts. `CLAIMS.md`, fresh artifacts and `spec/capabilities.json` delimit demonstrated scope. Tests and views cannot silently extend it.
4. Original source documents and the 22 examples remain unchanged. New grammar, checker, formalization and implementation are new work, not recovered historical package files.

The sibling U and BiDi checkouts keep their histories independent. ETEllis/U uses private visibility, matching BiDi. The repository, manuscript and local landing page cross-reference BiDi. No change to BiDi's source, remote, naming, runtime or published claims is authorized by that cross-reference.

Stage0 uses available stable Python 3.13 with separately inspected modules. The initial environment had no Rust compiler; this choice makes a direct reference semantics executable and reviewable while retaining native-backend and self-hosting removal gates. It does not rename Python execution native machine-code execution. The repository's MIT license matches Edward's BiDi licensing.
