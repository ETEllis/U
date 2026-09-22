#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
PYTHON_BIN="${U_PYTHON:-python3}"
sh bootstrap/native-build.sh
"$PYTHON_BIN" scripts/verify.py
if [[ "${1:-}" == "--full" ]]; then
  "$PYTHON_BIN" scripts/verify_formal.py
  (cd paper && sh build.sh)
fi
