# Version and migration contract

U 0.2 changes the implementation from a Python-hosted reference to a U-written native compiler and libraries. The source header remains `etellis.u/0.1`; all 22 original source files remain unchanged.

Graph, evidence, package and build receipts use explicit schemas. New receipts must not be relabeled as old receipts, and old identity encodings are not silently accepted under new profiles.

The native CLI prints ordinary program results directly. `build` emits a build receipt alongside its executable; `prove` returns the checked proof's public account without serializing its authority. This differs from the historical reference evaluator's output envelope.

`package lock` writes schema 2 and preserves any previous lock byte-for-byte under `.u-lock.previous-<sha256>`. Verify and install refuse schema 1 with explicit relocking guidance. Installation uses `<destination>/<name>/<version>/<full-snapshot-digest>` and rechecks an existing target before reuse. Installed source does not grant the permissions declared in its manifest, and install scripts never run.

```sh
./bin/etellis-u package lock .
./bin/etellis-u package verify .
./bin/etellis-u package install . --destination .u-packages
```

The source defaults to the working directory and the destination to `.u-packages`. An installation store inside its source tree must lie under an excluded directory. Failed staging directories remain available for inspection; the tool does not delete them.

The optional independent bootstrap uses [bootstrap/build.sh](../bootstrap/build.sh). Ordinary users use [bootstrap/native-build.sh](../bootstrap/native-build.sh) or the public launcher; Python is not required.

The complete historical implementation remains recoverable from commit `5dce6e72e93a165913f38713626b8c3e5c8a8f1b`. Historical receipts and test counts describe that implementation, not this native release.
