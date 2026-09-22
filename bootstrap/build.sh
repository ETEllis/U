#!/bin/sh
# Independent seed, followed by two U-owned compiler generations.
set -eu
unset U_NATIVE_STEPS
U_BOOTSTRAP_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
U_BOOTSTRAP_OUT=${1:-"$U_BOOTSTRAP_ROOT/build/native"}
mkdir -p "$U_BOOTSTRAP_OUT"
U_BOOTSTRAP_OUT=$(CDPATH= cd -- "$U_BOOTSTRAP_OUT" && pwd)
U_BOOTSTRAP_CC=${CC:-cc}
U_BOOTSTRAP_PYTHON=${PYTHON:-python3}
U_BOOTSTRAP_LIBS=-lm
if [ "$(uname -s)" != Darwin ]; then U_BOOTSTRAP_LIBS="-lm -lcrypto"; fi
"$U_BOOTSTRAP_PYTHON" "$U_BOOTSTRAP_ROOT/bootstrap/seed.py" \
  "$U_BOOTSTRAP_ROOT/compiler/compiler.u" "$U_BOOTSTRAP_OUT/seed.c"
"$U_BOOTSTRAP_CC" -std=c11 -O1 -ffp-contract=off -I "$U_BOOTSTRAP_ROOT/native" \
  "$U_BOOTSTRAP_OUT/seed.c" "$U_BOOTSTRAP_ROOT/native/runtime.c" $U_BOOTSTRAP_LIBS -o "$U_BOOTSTRAP_OUT/uc-seed"
U_NATIVE_ALLOW=read,write,exec,console,env "$U_BOOTSTRAP_OUT/uc-seed" emit \
  "$U_BOOTSTRAP_ROOT/compiler/compiler.u" "$U_BOOTSTRAP_OUT/compiler-stage1.c"
"$U_BOOTSTRAP_CC" -std=c11 -O1 -ffp-contract=off -I "$U_BOOTSTRAP_ROOT/native" \
  "$U_BOOTSTRAP_OUT/compiler-stage1.c" "$U_BOOTSTRAP_ROOT/native/runtime.c" $U_BOOTSTRAP_LIBS -o "$U_BOOTSTRAP_OUT/uc-stage1"
U_NATIVE_ALLOW=read,write,exec,console,env "$U_BOOTSTRAP_OUT/uc-stage1" emit \
  "$U_BOOTSTRAP_ROOT/compiler/compiler.u" "$U_BOOTSTRAP_OUT/compiler-stage2.c"
cmp "$U_BOOTSTRAP_OUT/compiler-stage1.c" "$U_BOOTSTRAP_OUT/compiler-stage2.c"
"$U_BOOTSTRAP_CC" -std=c11 -O1 -ffp-contract=off -I "$U_BOOTSTRAP_ROOT/native" \
  "$U_BOOTSTRAP_OUT/compiler-stage2.c" "$U_BOOTSTRAP_ROOT/native/runtime.c" $U_BOOTSTRAP_LIBS -o "$U_BOOTSTRAP_OUT/uc"
U_NATIVE_ALLOW=read,write,exec,console,env "$U_BOOTSTRAP_OUT/uc" emit \
  "$U_BOOTSTRAP_ROOT/compiler/compiler.u" "$U_BOOTSTRAP_OUT/compiler-stage3.c"
cmp "$U_BOOTSTRAP_OUT/compiler-stage2.c" "$U_BOOTSTRAP_OUT/compiler-stage3.c"
printf '%s\n' "U compiler rebuilt itself: three generated C stages agree byte for byte." \
  "Compiler executable: $U_BOOTSTRAP_OUT/uc"
