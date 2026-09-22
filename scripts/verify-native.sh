#!/bin/sh
# Fast native conformance gate. No Python, foreign evaluator or package hooks.
set -eu
U_VERIFY_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
U_VERIFY_BUILD="$U_VERIFY_ROOT/build/native"
if [ ! -x "$U_VERIFY_BUILD/uc" ]; then sh "$U_VERIFY_ROOT/bootstrap/native-build.sh"; fi
U_VERIFY_WORK=$(mktemp -d "${TMPDIR:-/tmp}/u-native-check.XXXXXXXX")
u_verify_cleanup() {
  for U_VERIFY_NAME in smoke evidence cdc processes workers dynamics process-negative dynamics-negative; do
    rm -f "$U_VERIFY_WORK/$U_VERIFY_NAME.u" "$U_VERIFY_WORK/$U_VERIFY_NAME.c" "$U_VERIFY_WORK/$U_VERIFY_NAME"
  done
  rmdir "$U_VERIFY_WORK" 2>/dev/null || true
}
trap u_verify_cleanup EXIT HUP INT TERM
U_VERIFY_LIBS=-lm
if [ "$(uname -s)" != Darwin ]; then U_VERIFY_LIBS="-lm -lcrypto"; fi
u_fixture() {
  U_VERIFY_NAME=$1
  shift
  U_NATIVE_ALLOW=read,write "$U_VERIFY_BUILD/u-link" "$U_VERIFY_WORK/$U_VERIFY_NAME.u" "$@"
  U_NATIVE_ALLOW=read,write "$U_VERIFY_BUILD/uc" emit "$U_VERIFY_WORK/$U_VERIFY_NAME.u" "$U_VERIFY_WORK/$U_VERIFY_NAME.c" main
  cc -std=c11 -O1 -ffp-contract=off -I "$U_VERIFY_ROOT/native" "$U_VERIFY_WORK/$U_VERIFY_NAME.c" \
    "$U_VERIFY_ROOT/native/runtime.c" $U_VERIFY_LIBS -o "$U_VERIFY_WORK/$U_VERIFY_NAME"
}
u_run() { U_NATIVE_ALLOW=${2:-} U_NATIVE_STEPS=50000000 "$U_VERIFY_WORK/$1"; }
u_fixture smoke "$U_VERIFY_ROOT/stdlib/core.u" "$U_VERIFY_ROOT/tests/native_programs/smoke.u"
u_run smoke
u_fixture evidence "$U_VERIFY_ROOT/stdlib/core.u" "$U_VERIFY_ROOT/stdlib/evidence.u" "$U_VERIFY_ROOT/tests/native_programs/evidence.u"
u_run evidence
u_fixture cdc "$U_VERIFY_ROOT/stdlib/core.u" "$U_VERIFY_ROOT/stdlib/numeric.u" \
  "$U_VERIFY_ROOT/cdc/primitives.u" "$U_VERIFY_ROOT/cdc/analysis.u" "$U_VERIFY_ROOT/tests/native_programs/cdc_analysis.u"
u_run cdc
u_fixture processes "$U_VERIFY_ROOT/stdlib/core.u" "$U_VERIFY_ROOT/stdlib/numeric.u" \
  "$U_VERIFY_ROOT/stdlib/processes.u" "$U_VERIFY_ROOT/tests/native_programs/processes.u"
u_run processes actors
u_fixture workers "$U_VERIFY_ROOT/stdlib/core.u" "$U_VERIFY_ROOT/stdlib/numeric.u" \
  "$U_VERIFY_ROOT/stdlib/processes.u" "$U_VERIFY_ROOT/tests/native_programs/processes_workers.u"
u_run workers
u_fixture dynamics "$U_VERIFY_ROOT/stdlib/core.u" "$U_VERIFY_ROOT/stdlib/numeric.u" \
  "$U_VERIFY_ROOT/stdlib/dynamics.u" "$U_VERIFY_ROOT/tests/native_programs/dynamics.u"
u_run dynamics
u_fixture process-negative "$U_VERIFY_ROOT/stdlib/core.u" "$U_VERIFY_ROOT/stdlib/numeric.u" \
  "$U_VERIFY_ROOT/stdlib/processes.u" "$U_VERIFY_ROOT/tests/native_programs/processes_fail_closed.u"
for U_VERIFY_MODE in 0 1 2 3 4 5 6 7; do
  if U_NATIVE_ALLOW=actors U_NATIVE_STEPS=50000000 "$U_VERIFY_WORK/process-negative" "[$U_VERIFY_MODE]"; then
    printf '%s\n' "Expected process refusal did not occur: $U_VERIFY_MODE" >&2; exit 1
  else U_VERIFY_STATUS=$?; test "$U_VERIFY_STATUS" = 70; fi
done
u_fixture dynamics-negative "$U_VERIFY_ROOT/stdlib/core.u" "$U_VERIFY_ROOT/stdlib/numeric.u" \
  "$U_VERIFY_ROOT/stdlib/dynamics.u" "$U_VERIFY_ROOT/tests/native_programs/dynamics_fail_closed.u"
for U_VERIFY_MODE in 0 1 2 3 4 5; do
  if U_NATIVE_STEPS=50000000 "$U_VERIFY_WORK/dynamics-negative" "[$U_VERIFY_MODE]"; then
    printf '%s\n' "Expected DAE refusal did not occur: $U_VERIFY_MODE" >&2; exit 1
  else U_VERIFY_STATUS=$?; test "$U_VERIFY_STATUS" = 70; fi
done
printf '%s\n' "Native U conformance passed: six positive programs and fourteen expected refusals."
