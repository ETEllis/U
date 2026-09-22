# U 0.2 native implementation

The compiler, six-constructor graph, standard libraries, proof kernel, CDC algorithms and developer tools now execute as U-written native programs. A checked generated-C seed starts a fresh build, and the compiler reproduces its own C output through three matching stages.

The native target supports lexical closures, lazy values, arbitrary-precision integers and the supported scientific libraries. The U-written WASM emitter supplies a smaller interval-certified integer and Boolean target, including the U site's interactive contract model. Host integrations validate exact integer bounds before WebAssembly ABI conversion.

Formatting, source lift/export, stdio language-server behavior, local package snapshots and artifact auditing are native tools. The formatter preserves comment and literal lexemes and is idempotent on its verified corpus. Package schema 2 adds explicit migration backups, verified staging and content-addressed installation without hooks. The public U and BiDi websites remain separately deployable from the private implementation repositories.

The historical Python runtime has been retired from the ordinary build and execution path. Its source remains in Git history for independent comparison; the independent source parser and seed are explicitly scoped bootstrap tools.

This release does not imply completion of the full dependent/resource calculus, every CDC U1/U2 contract, foreign-language ingestion, general WASM compilation, long-running garbage collection or physical realization. See [support and evidence](../CLAIMS.md) and [remaining gates](FULL_COMPLETION_BACKLOG.md).
