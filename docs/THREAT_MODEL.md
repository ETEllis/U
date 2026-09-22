# Native trust boundary

U source is untrusted input to a bounded U-written compiler. The checked C seed, handwritten native bridge, C toolchain, system libraries and operating system are trusted. The optional Python seed and independent test harnesses are development tools, not production runtime dependencies.

Parsing, checking, formatting and graph reconstruction do not execute source effects or package hooks. User code cannot directly access reserved raw resource issuers, internal library names or the private proof constructors through the public compiler path. Closed platform namespaces reject unknown operations.

## Capabilities and processes

The compiler's read/write/exec grants are distinct from the generated program's grants. The launcher and U driver explicitly attenuate the latter. OS workers have private mutable memory, explicit grants and a shared call budget. Transport rejects closures, mutable storage and resource authority where transfer has no supported rule.

Read, write and subprocess capabilities permit real host actions. They are broad trusted-local grants, not a filesystem jail, syscall sandbox or protection against a compromised OS. Arbitrary subprocess permission can run arbitrary installed programs. A person modifying trusted U libraries or C code can alter the implementation and lies outside the hostile-source contract.

A checked certificate cannot be reconstructed from JSON. CDC handles require a private issuer registry as well as correct subject binding. Hashes identify artifacts; they do not authenticate people, authorize effects or establish mathematical truth.

## Operational limits

- Retaining arenas are bounded and reclaimed at process exit; general long-running garbage collection is not implemented.
- User calls share a finite budget, including isolated workers.
- Catching a fault does not roll back prior effects.
- Local package snapshots reject symlinks and unsafe member paths, verify exact bytes, and never execute installation scripts or grant manifest permissions. Exclusive staging writes and one publication rename protect the ordinary local workflow. They do not constitute a hostile concurrent-filesystem sandbox or a crash-fsync guarantee.
- HTTP uses a bounded, fixed-argument platform transport with explicit network permission; it is not a proof of remote content safety.
- Native and browser simulators do not issue physical-device evidence.

Independent seeds, negative tests, sanitizer checks and reproducible compilation reduce specific risks. They do not establish complete compiler correctness or supply-chain immunity.
