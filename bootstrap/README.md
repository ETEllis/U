# Bootstrap and independent reference

Ordinary builds use `native-build.sh`. It compiles `compiler.generated.c`, asks
the resulting U compiler to translate `compiler/compiler.u`, and compares three
generated stages against the checked seed. It builds the public native tools
without Python.

`build.sh` is the optional independent route. `seed.py` and the preserved
`reference_source.py` translate the compiler once; subsequent stages execute U.
They are not a runtime for user programs.

The complete earlier Python implementation and its reference-only tests remain
in commit `5dce6e72e93a165913f38713626b8c3e5c8a8f1b`. They were removed from the
current production tree when the native path replaced them. For a separate
historical comparison checkout:

```sh
git worktree add --detach ../U-reference 5dce6e72e93a165913f38713626b8c3e5c8a8f1b
```

Generated C is marked generated because its bytes are reproducibly checked.
Handwritten bridge, shell, browser, formal and development-test code is not
relabeled or concealed. GitHub language recognition is not execution evidence.
