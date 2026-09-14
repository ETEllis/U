# Reference release trust boundary

U source is untrusted input to bounded parsing and explicit interpretation. Host Python modules, the standard library, native compiler/libm, Lean kernel and operating system remain trusted dependencies. The parser, formatter and static checker do not run source effects or package scripts.

Runtime console, network and actor capabilities are minted by the evaluator from explicit caller grants. The reference lower memory/authority models use process-local identity checks and consuming rights. They are tested against fabricated records, stale handles, lifetime escape, wrong frames, repeated nonces, nonfinite times and overlapping footprints. A user able to execute arbitrary host Python can inspect or modify this implementation and is outside its hostile-source boundary.

Checked proof and recurrence values have checked constructors or private issuance registries. Ordinary U records cannot mint them. Mathematical proof syntax is processed by a distinct total checker with a closed rule set. Canonical marker-shaped maps are encoded unambiguously; neither hashes nor signed-looking JSON are treated as truth or external authentication.

Package installation only copies the locally locked `.u` and manifest snapshot. Symlinks, source changes during installation and digest changes are refused. There is no arbitrary installation-script hook. Network package resolution and process sandboxing of third-party host extensions remain unsupported. External HTTP requests require a caller-granted network capability; finer origin policies are an open production requirement.

The local journal distinguishes a torn final record from hash-chain corruption. It uses advisory locking and fsync on one host. It is not a consensus log, authenticated multihost history, or an exactly-once external-effects protocol. Crashes during external operations may require an Indeterminate outcome and reconciliation; no global rollback is promised.

Production deployment requires stronger isolation, stress/fuzz/security coverage, resource budgeting across all host algorithms, untrusted artifact schema validation, code signing, credential lifecycle and device/provider policies. The current reference release is suitable for controlled development and research, with those boundaries visible.
