# The compiler is a U program

`compiler/compiler.u` implements U's scanner, parser, core source validator,
closure conversion, C generation, and compiler command interface. Its output
contains an ordinary C function for each U function and lambda. Calls and value
construction have an explicit evaluation order. The generated executable does
not contain a U parser or an interpreter for a serialized U program.

U uses C as a portable machine target. Generated C is build output; the algorithms
that decide what it means live in U. The native bridge provides values, lexical
environments, calls, arbitrary precision integers, storage, codecs, and access to
the operating system. Its contracts and remaining implementation limits are
listed in [`native/ABI.md`](../native/ABI.md).

## Rebuilding the compiler

The ordinary build starts from checked generated C, then uses the U-written
compiler to regenerate it. The optional independent seed translator uses the
historical Python parser to establish a separate initial executable from U
source. Python is not required by the normal route.

The reproducibility test has three executable stages:

1. The seed produces a compiler executable. That executable translates
   `compiler/compiler.u` into C.
2. The C output is compiled into a second compiler. It translates the same U
   source again.
3. A compiler built from the second output repeats the translation. The generated
   C must be byte-identical, and the executable tests must agree.

This is a compiler fixed point. It establishes that the U implementation can
reproduce its own translation through its native execution path. It does not
establish freedom from a compromised seed or C toolchain; independent bootstrap
implementations remain useful for that question. Neither a fixed point nor
self-hosting by itself proves a theory's laws.

Run the complete bootstrap and reproduction check with:

```sh
sh bootstrap/native-build.sh
```

This produces `build/native/uc` and the native command-line, package and audit
tools. Only a C11 toolchain and platform libraries are required. It also checks
that regenerated C exactly matches `bootstrap/compiler.generated.c`.

Generating the combined compiler, graph, formatter, WASM and editor tool uses a
1.5 GiB transient compiler arena. This build setting does not change the
default 1 GiB payload budget of generated user programs. Both limits are
allocation budgets; operating-system resident memory can be higher.

For an independent Python-seeded comparison, run `sh bootstrap/build.sh` with
a separate output directory. That optional route requires Python; all later
translation is performed by compiled U. C compilation still requires the
declared native toolchain.

The [September 14 verification](../tests/selfhost/verification.json) records three
identical generated C stages and byte-identical native executables built from
stages two and three. The binary comparison uses the same executable basename in
separate directories because macOS ad-hoc signatures include that name. 34
independent test groups passed with no interpreter available in the compiler's or
generated programs' search path. The receipt identifies the exact source, runtime,
toolchain, and output hashes.

## Surface and validation

The parser accepts version and import declarations, annotated definitions,
typed lambdas, calls, field access, lists, tuples, records, lexical blocks,
comments, arbitrary precision integer literals, binary64 literals, and Unicode
strings. Source coordinates are UTF-8 byte offsets. Repeated version and import
headers are accepted when source modules are concatenated for explicit linkage.
Duplicate global definitions remain errors.

Top-level value definitions are evaluated on first use and then memoized.
Unused definitions do not run their effects; a cycle during initialization is an
error. Each local `let` creates a new lexical frame after evaluating its
initializer. A closure therefore keeps the bindings visible when it was created,
even if a later local name shadows a global. Duplicate names within one block
remain a syntax error, as in the original surface contract.

`parse` establishes syntax coverage. `check` additionally resolves lexical names,
checks declared type names, direct function arity, and known scalar argument and
result compatibility. Native primitive signatures have explicit checks. Bindings
take precedence over primitive namespaces, including names such as `int` and
`list`.

An unshadowed native namespace admits only exact members from the native registry.
Misspelled operations and invented members of native functions fail before code
is emitted. Ordinary records can still use the same names and retain their own
field behavior.

The original domain vocabulary remains available, including ownership, tasks,
probability, signals, GPU regions, quantum registers, and CDC interfaces. Their
names retain their declared theory requirements. Constructor arity, natural
indices, explicit array shapes, and references to previously bound parameters are
checked; a signal's clock index must name a declared `Clock`. Dependent equality
terms and `Pi` binders retain their scope and produce an explicit proof obligation.
Accepting an annotation never supplies its missing theory evidence.

Generated functions guard their annotated parameters and declared results at
runtime as well. A foreign JSON argument or an explicitly dynamic call therefore
cannot pass a negative integer as `Nat` or return an integer as `Bool`. These
guards enforce storage kinds and natural-number nonnegativity. They do not
establish collection element types, a higher-order function's complete contract,
or dependent refinements.

Compiler data uses explicitly annotated `Value` boundaries. These boundaries are
reported as obligations. A value whose static representation is dynamic remains
subject to native argument guards; its annotation does not become a dependent
proof. General dependent type checking, linear resource proofs, theory admission,
and physical realization are separate requirements. A syntax pass never grants
them. Unsupported names and operations are refused rather than assigned a
plausible result.

A definition annotated `Type` is retained as an inert descriptor containing its
name and original syntax tree. Constructing that descriptor does not execute a
type expression or certify its theory. The check report lists the outstanding
type declaration explicitly. Proof libraries may inspect that syntax and enforce
their own admitted rules.

The source compiler deliberately keeps typed syntax available as closure
metadata. U proof and symbolic libraries can inspect that data. The native bridge
does not execute it: executable behavior remains the emitted closure functions.

## Commands

The compiled executable supports:

```text
uc parse INPUT.u
uc check INPUT.u
uc emit INPUT.u OUTPUT.c [ENTRY]
```

`emit` defaults to `main`. The emitted program can also call a selected function
with a JSON argument array through its native entry adapter.

Source formatting is implemented in U by `ff_format` in `compiler/format.u`, for
the production command line and language server. It normalizes indentation,
operator spacing, arguments, and record layouts while retaining literal spelling
and comment contents. Tests establish idempotence, independent parser agreement,
and unchanged structural graph identity across all 22 original examples, the
compiler, and the formatter itself.

Compiler reads and writes require the explicit native process capabilities used
by the trusted build launcher. Source parsing and checking do not execute source
definitions or annotation expressions. Emission creates C source only; it does
not run the program being compiled.

## Independent checks

`tests/selfhost/test_compiler.py` invokes the native compiler and native C
toolchain. Its executable cases cover captured closures, argument evaluation
order, namespace shadowing, Unicode and embedded NUL strings, integers larger
than machine words, concatenated modules, and closure descriptions. Negative
cases cover malformed syntax, duplicate names, unbound names, known type errors,
wrong arity, and calls to non-functions. The 22 original language examples are
parsed without modifying their bytes; that result is distinct from execution of
every theory appearing in them.

The original induction example is also compiled with the U proof library and
executed through its independent total kernel. A companion rejection test gives
an ordinary identity function a dependent proof annotation and confirms that
the kernel refuses to certify it. The annotation preserves the intended claim;
the checker must still establish it.

The tests use Python as an independent harness. Neither compiler execution nor
the generated programs depend on that harness. The native bridge uses a bounded
arena and releases its allocations when the process exits. Compiler symbol tables
and cached metadata reduce repeated allocation, but a long-running process retains
its temporary values. Tracing garbage collection remains a concrete runtime
requirement for those workloads.

The compiler also reuses immutable type and binding descriptors, caches short C
quotations and byte escapes, and skips lexical spans through generic byte-search
operations. A 2,145-line, 145,085-byte bundle containing the standard libraries and
CDC modules was translated within the unchanged 1 GiB allocation budget, then
compiled and executed to calculate the exact 100th Fibonacci number. The native
test suite repeats that complete-library check. The memory limit covers the
declared arena; it is not a promise about total operating-system memory use.
