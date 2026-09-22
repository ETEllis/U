#!/bin/sh
# Normal bootstrap: the checked C seed starts the compiler written in U.
set -eu
unset U_NATIVE_STEPS
U_BOOTSTRAP_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
U_BOOTSTRAP_OUT=${1:-"$U_BOOTSTRAP_ROOT/build/native"}
mkdir -p "$U_BOOTSTRAP_OUT"
U_BOOTSTRAP_OUT=$(CDPATH= cd -- "$U_BOOTSTRAP_OUT" && pwd)
U_BOOTSTRAP_CC=${CC:-cc}
U_BOOTSTRAP_LIBS=-lm
if [ "$(uname -s)" != Darwin ]; then U_BOOTSTRAP_LIBS="-lm -lcrypto"; fi
u_link_c() {
  "$U_BOOTSTRAP_CC" -std=c11 -O1 -ffp-contract=off -I "$U_BOOTSTRAP_ROOT/native" \
    "$1" "$U_BOOTSTRAP_ROOT/native/runtime.c" $U_BOOTSTRAP_LIBS -o "$2"
}
u_emit() {
  U_NATIVE_ALLOW=read,write,console,env "$U_BOOTSTRAP_OUT/uc" emit "$1" "$2" "${3:-main}"
}
u_link_c "$U_BOOTSTRAP_ROOT/bootstrap/compiler.generated.c" "$U_BOOTSTRAP_OUT/uc-seed"
U_NATIVE_ALLOW=read,write,console,env "$U_BOOTSTRAP_OUT/uc-seed" emit \
  "$U_BOOTSTRAP_ROOT/compiler/compiler.u" "$U_BOOTSTRAP_OUT/compiler-stage1.c"
cmp "$U_BOOTSTRAP_ROOT/bootstrap/compiler.generated.c" "$U_BOOTSTRAP_OUT/compiler-stage1.c"
u_link_c "$U_BOOTSTRAP_OUT/compiler-stage1.c" "$U_BOOTSTRAP_OUT/uc"
u_emit "$U_BOOTSTRAP_ROOT/compiler/compiler.u" "$U_BOOTSTRAP_OUT/compiler-stage2.c"
cmp "$U_BOOTSTRAP_OUT/compiler-stage1.c" "$U_BOOTSTRAP_OUT/compiler-stage2.c"
u_link_c "$U_BOOTSTRAP_OUT/compiler-stage2.c" "$U_BOOTSTRAP_OUT/uc-stage2"
U_NATIVE_ALLOW=read,write,console,env "$U_BOOTSTRAP_OUT/uc-stage2" emit \
  "$U_BOOTSTRAP_ROOT/compiler/compiler.u" "$U_BOOTSTRAP_OUT/compiler-stage3.c"
cmp "$U_BOOTSTRAP_OUT/compiler-stage2.c" "$U_BOOTSTRAP_OUT/compiler-stage3.c"
u_emit "$U_BOOTSTRAP_ROOT/tools/link.u" "$U_BOOTSTRAP_OUT/link.c" main
u_link_c "$U_BOOTSTRAP_OUT/link.c" "$U_BOOTSTRAP_OUT/u-link"
U_NATIVE_ALLOW=read,write,console,env "$U_BOOTSTRAP_OUT/u-link" "$U_BOOTSTRAP_OUT/driver.u" \
  "$U_BOOTSTRAP_ROOT/compiler/compiler.u" "$U_BOOTSTRAP_ROOT/compiler/graph.u" \
  "$U_BOOTSTRAP_ROOT/compiler/format.u" "$U_BOOTSTRAP_ROOT/compiler/wasm.u" \
  "$U_BOOTSTRAP_ROOT/tools/lsp.u" "$U_BOOTSTRAP_ROOT/tools/driver.u"
# The full compiler, graph, formatter, WASM and editor bundle needs a larger
# transient compiler arena. Generated user programs keep their own default.
U_NATIVE_ALLOW=read,write,console,env U_NATIVE_MEMORY_MB=1536 "$U_BOOTSTRAP_OUT/uc" emit \
  "$U_BOOTSTRAP_OUT/driver.u" "$U_BOOTSTRAP_OUT/driver.c" du_main
u_link_c "$U_BOOTSTRAP_OUT/driver.c" "$U_BOOTSTRAP_OUT/etellis-u-driver"
U_NATIVE_ALLOW=read,write,console,env "$U_BOOTSTRAP_OUT/u-link" "$U_BOOTSTRAP_OUT/package.u" \
  "$U_BOOTSTRAP_ROOT/stdlib/core.u" "$U_BOOTSTRAP_ROOT/stdlib/evidence.u" \
  "$U_BOOTSTRAP_ROOT/tools/package.u"
u_emit "$U_BOOTSTRAP_OUT/package.u" "$U_BOOTSTRAP_OUT/package.c" pk_main
u_link_c "$U_BOOTSTRAP_OUT/package.c" "$U_BOOTSTRAP_OUT/etellis-u-package"
U_NATIVE_ALLOW=read,write,console,env "$U_BOOTSTRAP_OUT/u-link" "$U_BOOTSTRAP_OUT/audit.u" \
  "$U_BOOTSTRAP_ROOT/stdlib/core.u" "$U_BOOTSTRAP_ROOT/stdlib/evidence.u" "$U_BOOTSTRAP_ROOT/tools/audit.u"
u_emit "$U_BOOTSTRAP_OUT/audit.u" "$U_BOOTSTRAP_OUT/audit.c" au_main
u_link_c "$U_BOOTSTRAP_OUT/audit.c" "$U_BOOTSTRAP_OUT/etellis-u-audit"
U_NATIVE_ALLOW=read,write,console,env "$U_BOOTSTRAP_OUT/u-link" "$U_BOOTSTRAP_OUT/cdc.u" \
  "$U_BOOTSTRAP_ROOT/stdlib/core.u" "$U_BOOTSTRAP_ROOT/stdlib/numeric.u" \
  "$U_BOOTSTRAP_ROOT/stdlib/inspection.u" "$U_BOOTSTRAP_ROOT/cdc/primitives.u" \
  "$U_BOOTSTRAP_ROOT/cdc/analysis.u" "$U_BOOTSTRAP_ROOT/cdc/source.u" "$U_BOOTSTRAP_ROOT/tools/cdc.u"
u_emit "$U_BOOTSTRAP_OUT/cdc.u" "$U_BOOTSTRAP_OUT/cdc.c" dc_main
u_link_c "$U_BOOTSTRAP_OUT/cdc.c" "$U_BOOTSTRAP_OUT/etellis-u-cdc"
touch "$U_BOOTSTRAP_OUT/complete.stamp"
printf '%s\n' "U native build complete. Three self-generated C stages match the checked seed."
