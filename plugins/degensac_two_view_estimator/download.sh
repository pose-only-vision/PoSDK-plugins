#!/usr/bin/env bash
# Verify the immutable, patched DEGENSAC source snapshot shipped with this
# plugin. This script deliberately performs no network download and never
# replaces tracked sources with an unpinned upstream HEAD.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="${SCRIPT_DIR}/pydegensac"

required_files=(
  "LICENSE"
  "src/pydegensac/degensac/exp_ranF.c"
  "src/pydegensac/degensac/exp_ranF.h"
  "src/pydegensac/matutls/ccmath.h"
  "src/pydegensac/matutls/lgpl.license"
)

for relative_path in "${required_files[@]}"; do
  if [[ ! -f "${SOURCE_DIR}/${relative_path}" ]]; then
    echo "error: tracked DEGENSAC snapshot is incomplete: ${SOURCE_DIR}/${relative_path}" >&2
    echo "       restore this plugin from the same PoSDK source revision; do not clone an unpinned upstream HEAD." >&2
    exit 1
  fi
done

if ! grep -Fq "exp_ransacFcustomLAFSeeded" \
     "${SOURCE_DIR}/src/pydegensac/degensac/exp_ranF.c"; then
  echo "error: vendored DEGENSAC snapshot is missing PoSDK's deterministic seeded backend" >&2
  exit 1
fi

echo "ok: verified the tracked, patched DEGENSAC source snapshot"
