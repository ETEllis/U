# U formal manuscript

**U: Law-Bearing Operations. Explicit Semantic Composition and Evidence-Bound Computation**

Edward Ellis · September 2026 · Research preprint

## Read and reproduce

- [Paper](main.pdf)
- [LaTeX source](main.tex)
- [Bibliography](references.bib)
- [Source bundle](arxiv-source.zip)
- [Source and scope notes](SOURCE_NOTES.md)

From this directory:

```sh
sh build.sh
```

The paper uses the standard article class, mathematical packages and BibTeX. Tectonic may fetch TeX dependencies on the first build. No shell escape or custom document class is required. The build includes the resolved bibliography in the source bundle.

## Mathematical scope

The paper proves conditional structural preservation, deterministic elaboration, nonduplication in a finite ownership model, qualified commutation, relational adapter composition, an approximation bound, proof-authority conditions, content identity and invalidation, monotonic evidence accumulation, residual reconstruction and recurrence-gated return analysis.

Each result states its assumptions. Correspondence between those abstract models and an implementation is a separate obligation. The repository's [support inventory](../CLAIMS.md) and verification results identify the current executable fragments and evidence.

The implementation section describes the U-written compiler, six-constructor graph, native libraries, total proof kernel and CDC analysis. It reports reproducible self-compilation separately from mathematical correctness, and explains the remaining platform bridge, numerical contracts and runtime limits. The [native verification report](../artifacts/native-verification.json) records the release-specific checks.

The manuscript is prepared for arXiv and has not been submitted or peer reviewed. AI assistance is disclosed in the paper; responsibility for its claims remains with the author.
