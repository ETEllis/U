# Project contracts and provenance

The repository distinguishes its implemented contracts, implementation evidence and wider research target.

1. [Native specification](docs/SPEC.md) defines the current source and execution contracts.
2. [Support and evidence](CLAIMS.md), together with revision-specific verification reports, identifies what has been exercised or established.
3. The preserved design dossier and operator contracts define the wider architecture. The [review reconciliation](provenance/review/RECONCILIATION.md) records the accepted refinements to elaboration, adapters, admission, identity and independence.
4. Original design documents and the 22 source examples retain their provenance. New grammar, compiler, verification and formalization work is versioned separately.

CDC predates U. The BiDi source contract and pinned native implementation remain independent references for compatibility; U's results are verified against them rather than inherited from their historical evidence.

The implementation repositories are private. Public U and BiDi websites are distributed through separate assets-only repositories, with their manuscripts and source bundles. The project uses the MIT License.

The current implementation is the U-written compiler and libraries. A checked generated-C seed starts the ordinary build; the independent Python parser and seed remain optional comparison tools. The [native ABI](native/ABI.md) discloses handwritten platform code and its trust boundaries.
