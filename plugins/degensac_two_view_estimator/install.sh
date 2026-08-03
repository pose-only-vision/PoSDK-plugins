#!/bin/sh
set -eu
umask 077

[ "${1:-}" = "--non-interactive" ] || { echo "expected --non-interactive" >&2; exit 64; }
[ "${2:-}" = "--report" ] || { echo "expected --report <path>" >&2; exit 64; }
[ -n "${3:-}" ] && [ "$3" = "${POSDK_INSTALL_REPORT:-}" ] || { echo "invalid report path" >&2; exit 64; }
[ "${POSDK_INSTALL_PROTOCOL:-}" = "posdk.package-install.v1" ] || { echo "unsupported installer protocol" >&2; exit 64; }
: "${POSDK_PACKAGE_ROOT:?}" "${POSDK_INSTALL_STATE_ROOT:?}" "${POSDK_PLUGIN_ID:?}" "${POSDK_PLUGIN_TYPE:?}" "${POSDK_PLUGIN_VERSION:?}" "${POSDK_TARGET_SYSTEM:?}" "${POSDK_TARGET_ARCH:?}"
[ "$POSDK_PLUGIN_ID" = "degensac_two_view_estimator" ] && [ "$POSDK_PLUGIN_TYPE" = "method" ] || { echo "plugin identity mismatch" >&2; exit 65; }

# The target-specific plugin binary contains the DEGENSAC and CCMATH objects.
# LAPACK/BLAS resolve to the macOS Accelerate framework, while OpenCV and PoSDK
# libraries are host-ABI dependencies supplied by PoSDK. No user environment,
# package manager, or system directory is modified by this installer.
mkdir -p "$POSDK_INSTALL_STATE_ROOT"
printf '{"schema_version":1,"status":"ok","plugin_id":"%s","system":"%s","architecture":"%s","dependencies":[],"bindings":[]}\n' \
  "$POSDK_PLUGIN_ID" "$POSDK_TARGET_SYSTEM" "$POSDK_TARGET_ARCH" \
  > "$POSDK_INSTALL_REPORT"
