#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
PYTHON_BIN="${U_PYTHON:-python3}"
"$PYTHON_BIN" -c 'import sys; assert sys.version_info >= (3,13), "Python 3.13+ required"'
"$PYTHON_BIN" scripts/verify.py
if [[ "${1:-}" == "--full" ]]; then
  "$PYTHON_BIN" scripts/verify_cdc.py
  "$PYTHON_BIN" scripts/verify_formal.py
  (cd paper && sh build.sh)
fi
