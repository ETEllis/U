# U website

The public site is [etellis.github.io/U-web](https://etellis.github.io/U-web/). Its resource decision is written in [U](model.u), compiled to [WebAssembly](model.wasm), and executed in the browser. The interface makes the result visible. The [compilation receipt](model.wasm.json) records the input contract and binary checksum.

The model distinguishes duplicated ownership, conflicting access, a missing sharing witness, an unadmitted profile and an admissible split. All 16 input combinations match the declared resource rules. An admissible result applies to this model; it does not issue graph-level authority or establish physical simultaneity.

Controls remain disabled until the module and its receipt load and agree. The browser validates exact input bounds before crossing the WebAssembly integer interface. Missing or inconsistent assets leave the check unavailable, with no JavaScript decision fallback.

Publish this directory at the site root, keeping `model.u`, `model.wasm` and `model.wasm.json` together. Include `paper/main.pdf`, `paper/arxiv-source.zip` and `assets/banner.svg`. Hosting is static; compilation happens before publication. The website uses local fonts, no tracking and no third-party JavaScript.

Constructor tabs support Left/Right, Home and End. The layout uses native controls, adapts to narrow screens and honors reduced-motion preferences. The implementation repository remains separate from the public website package.
