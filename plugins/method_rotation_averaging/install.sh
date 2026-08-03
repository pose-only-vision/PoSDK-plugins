#!/bin/sh
set -eu
umask 077

[ "${1:-}" = "--non-interactive" ] || { echo "expected --non-interactive" >&2; exit 64; }
[ "${2:-}" = "--report" ] || { echo "expected --report <path>" >&2; exit 64; }
[ -n "${3:-}" ] && [ "$3" = "${POSDK_INSTALL_REPORT:-}" ] || { echo "invalid report path" >&2; exit 64; }
[ "${POSDK_INSTALL_PROTOCOL:-}" = "posdk.package-install.v1" ] || { echo "unsupported installer protocol" >&2; exit 64; }
: "${POSDK_PACKAGE_ROOT:?}" "${POSDK_INSTALL_STATE_ROOT:?}" "${POSDK_PLUGIN_ID:?}" "${POSDK_PLUGIN_TYPE:?}" "${POSDK_PLUGIN_VERSION:?}" "${POSDK_TARGET_SYSTEM:?}" "${POSDK_TARGET_ARCH:?}"
[ "$POSDK_PLUGIN_ID" = "method_rotation_averaging" ] && [ "$POSDK_PLUGIN_TYPE" = "method" ] || { echo "plugin identity mismatch" >&2; exit 65; }
[ "$POSDK_TARGET_SYSTEM" = "macos" ] && [ "$POSDK_TARGET_ARCH" = "arm64" ] || { echo "this GraphOptim package supports macOS arm64 only" >&2; exit 65; }
[ "$(uname -s)" = "Darwin" ] && [ "$(uname -m)" = "arm64" ] || { echo "this GraphOptim package requires Apple Silicon macOS" >&2; exit 65; }
case "$POSDK_INSTALL_STATE_ROOT" in
  */installer/method_rotation_averaging/*/macos-arm64) ;;
  *) echo "unsafe GraphOptim installer state root" >&2; exit 65 ;;
esac

minimum_macos="15.0"
host_macos="$(sw_vers -productVersion)"
version_at_least() {
  awk -v actual="$1" -v required="$2" 'BEGIN {
    split(actual, a, "."); split(required, r, ".");
    for (i = 1; i <= 3; ++i) {
      av = (a[i] == "" ? 0 : a[i]) + 0;
      rv = (r[i] == "" ? 0 : r[i]) + 0;
      if (av > rv) exit 0;
      if (av < rv) exit 1;
    }
    exit 0;
  }'
}
version_at_least "$host_macos" "$minimum_macos" || {
  echo "GraphOptim requires macOS $minimum_macos or newer" >&2
  exit 65
}

archive="$POSDK_PACKAGE_ROOT/deps/GraphOptim-source.tar.gz"
expected_sha256="e3a3bd61123091a6ebb254c5517fd3b83c0834b74c4ea87cee316b8ff25c5251"
[ -f "$archive" ] || { echo "signed GraphOptim source archive is missing" >&2; exit 66; }
actual_sha256="$(shasum -a 256 "$archive" | awk '{print $1}')"
[ "$actual_sha256" = "$expected_sha256" ] || { echo "GraphOptim source archive digest mismatch" >&2; exit 66; }

command -v brew >/dev/null 2>&1 || { echo "Homebrew is required to install GraphOptim dependencies" >&2; exit 69; }
command -v xcrun >/dev/null 2>&1 || { echo "Xcode Command Line Tools are required" >&2; exit 69; }
xcrun --sdk macosx --show-sdk-path >/dev/null 2>&1 || { echo "the macOS SDK is unavailable" >&2; exit 69; }
missing_formulae=""
formulae="cmake eigen ceres-solver glog gflags suite-sparse libomp tbb metis"
for formula in $formulae; do
  if ! brew list --versions "$formula" >/dev/null 2>&1; then
    missing_formulae="$missing_formulae $formula"
  fi
done
if [ -n "$missing_formulae" ]; then
  HOMEBREW_NO_AUTO_UPDATE=1 \
  HOMEBREW_NO_INSTALL_CLEANUP=1 \
  HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1 \
    brew install $missing_formulae
fi

source_root="$POSDK_INSTALL_STATE_ROOT/source"
build_root="$POSDK_INSTALL_STATE_ROOT/build"
bin_root="$POSDK_INSTALL_STATE_ROOT/bin"
contract="$POSDK_INSTALL_STATE_ROOT/graphoptim-build-contract.txt"
contract_candidate="$POSDK_INSTALL_STATE_ROOT/.graphoptim-build-contract.$$"
trap 'rm -f "$contract_candidate"' EXIT HUP INT TERM
{
  printf 'schema=posdk.graphoptim-build.v1\n'
  printf 'archive_sha256=%s\n' "$actual_sha256"
  printf 'plugin_version=%s\n' "$POSDK_PLUGIN_VERSION"
  printf 'system=%s\n' "$POSDK_TARGET_SYSTEM"
  printf 'architecture=%s\n' "$POSDK_TARGET_ARCH"
  printf 'minimum_macos=%s\n' "$minimum_macos"
  for formula in $formulae; do
    brew list --versions "$formula"
  done
} > "$contract_candidate"

needs_rebuild=1
if [ -x "$bin_root/rotation_estimator" ] && [ -f "$contract" ] \
    && [ ! -L "$contract" ] && cmp -s "$contract_candidate" "$contract"; then
  needs_rebuild=0
fi

if [ "$needs_rebuild" -eq 1 ]; then
  rm -rf "$source_root" "$build_root" "$bin_root"
  mkdir -p "$source_root" "$build_root" "$bin_root"
  libomp_root="$(brew --prefix libomp)"
  tar -xzf "$archive" -C "$source_root"
  MACOSX_DEPLOYMENT_TARGET="$minimum_macos" \
  cmake -S "$source_root" -B "$build_root" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_OSX_ARCHITECTURES="$POSDK_TARGET_ARCH" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$minimum_macos" \
    -DOPENMP_ENABLED=ON \
    -DOpenMP_ROOT="$libomp_root" \
    -DCMAKE_C_FLAGS="-Xpreprocessor -fopenmp -I$libomp_root/include" \
    -DCMAKE_CXX_FLAGS="-Xpreprocessor -fopenmp -I$libomp_root/include" \
    -DCMAKE_EXE_LINKER_FLAGS="-L$libomp_root/lib -lomp" \
    -DTESTS_ENABLED=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_COLMAP=ON
  cmake --build "$build_root" --target rotation_estimator --parallel "$(sysctl -n hw.ncpu)"
  install -m 755 "$source_root/bin/rotation_estimator" "$bin_root/rotation_estimator"
fi

archs="$(lipo -archs "$bin_root/rotation_estimator")"
[ "$archs" = "arm64" ] || { echo "rotation_estimator architecture is invalid: $archs" >&2; exit 70; }
binary_minos="$(vtool -show-build "$bin_root/rotation_estimator" | awk '/^[[:space:]]*minos / {print $2; exit}')"
[ -n "$binary_minos" ] || { echo "rotation_estimator minimum macOS is unavailable" >&2; exit 70; }
version_at_least "$minimum_macos" "$binary_minos" || {
  echo "rotation_estimator requires macOS $binary_minos, above package baseline $minimum_macos" >&2
  exit 70
}
for dependency in $(otool -L "$bin_root/rotation_estimator" | awk 'NR > 1 {print $1}'); do
  case "$dependency" in
    /opt/homebrew/*|/System/*|/usr/lib/*|@*) ;;
    *) echo "rotation_estimator contains a non-portable dependency: $dependency" >&2; exit 70 ;;
  esac
done

smoke_output="$("$bin_root/rotation_estimator" --help 2>&1 || true)"
printf '%s\n' "$smoke_output" | grep -q "g2o_filename" || { echo "rotation_estimator smoke test failed" >&2; exit 70; }
mv -f "$contract_candidate" "$contract"
chmod 600 "$contract"
trap - EXIT HUP INT TERM
printf '{"schema_version":1,"status":"ok","plugin_id":"%s","system":"%s","architecture":"%s","dependencies":[{"id":"graphoptim","version":"9d932464831bff46b1b5a118ececb2e00c42d5c6","provider":"bundled-source+homebrew"}],"bindings":[{"id":"graphoptim_bin","relative_path":"bin"}]}\n' \
  "$POSDK_PLUGIN_ID" "$POSDK_TARGET_SYSTEM" "$POSDK_TARGET_ARCH" \
  > "$POSDK_INSTALL_REPORT"
