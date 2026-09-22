# Native implementation interfaces

| Component | U interface | Responsibility |
|---|---|---|
| Compiler | cc_parse, cc_validate, cc_emit | Source, lexical/core checking and C generation |
| Graph | gg_lower, gg_reconstruct | Six-constructor lowering and validated reconstruction |
| Formatter | ff_format | Comment/literal-preserving source formatting |
| WASM | ww_build | Interval-certified integer and Boolean emission |
| Driver | du_main | Public command line, linking, capability attenuation and build receipts |
| Language server | ls_main | Stdio document protocol, diagnostics and editor operations |
| Packages | pk_main | Exact local source snapshots, verification and installation |
| Artifact audit | au_main | Identity checks without an authenticity claim |

The U linker concatenates declared implementation modules; it does not execute imports. The production driver selects relevant standard-library modules, retains the actual bytes used for dependency identities, reconstructs user graph bodies, generates a public entry wrapper and prunes unreachable definitions.

Package commands accept `lock|verify|install [SOURCE] [--destination STORE]`. The default source is the working directory and the default store is `.u-packages`. Schema 2 locks bind the complete selected source set; package installation never executes hooks or grants manifest permissions.

The generic C ABI is documented in [native/ABI.md](native/ABI.md). No U syntax evaluator exists in that boundary. Internal library names and raw resource issuers are not public source capabilities.

The [runtime contract](docs/RUNTIME.md), [source specification](docs/SPEC.md) and [self-compilation procedure](docs/SELF_HOSTING.md) define the boundaries in more detail.
