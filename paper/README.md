# U formal manuscript

**U: Law-Bearing Operations — Explicit Semantic Composition and Earned Artifacts**

Author: Edward Ellis. Date: September 14, 2026.

This is an arXiv-intended manuscript prepared for author review. It has not been submitted, peer reviewed, assigned a DOI or assigned an arXiv identifier. The manuscript discloses AI assistance. No affiliation or funding claim is supplied.

## Read and reproduce

- [Compiled paper](main.pdf)
- [LaTeX source](main.tex)
- [Bibliography](references.bib)
- [Source bundle](arxiv-source.zip)
- [Source and scope notes](SOURCE_NOTES.md)

From this directory, run:

```sh
sh build.sh
```

The paper uses a conventional article class, standard mathematical packages and BibTeX. A first Tectonic build may fetch its TeX dependencies. The source does not use shell escape, remote embedded content or a private document template. The committed `main.bbl` is included so an eventual submission package can resolve bibliography entries without reconstructing the original author environment. Submission requirements should be checked at the time of an actual authorized submission.

The build script also packages the source, resolved bibliography and scope notes into `arxiv-source.zip` when the standard `zip` utility is available. This is a prepared source bundle, not an automated submission.

## What is proved

The manuscript gives ordinary mathematical proofs of explicit conditional statements: structural interface preservation; deterministic elaboration under deterministic rules; nonduplication in a finite ownership model; qualified commuting updates; relational adapter composition and an error bound; constructor-bounded proof authority; content identity and invalidation; monotonic earning and prerequisite non-fabrication; residual reconstruction laws; and CDC's gated return consequence.

These are statements about the defined abstract models. They are not a mechanized proof of the whole Python implementation, all proposed theory families, full language subsumption, or the complete production compiler. Actual implementation evidence is maintained in the repository's [claim ledger](../CLAIMS.md) and fresh verification receipts. The paper deliberately avoids transplanting historical BiDi test counts into current U results.

## Author review before submission

Review the formal assumptions and their implementation correspondence, verify all artifact claims against the intended release commit, approve the authorship/AI-assistance statement, and decide the final name and submission category. This repository prepares the paper; it does not submit it or make the private repository public.
