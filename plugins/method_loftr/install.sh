#!/bin/sh
set -eu
umask 077

[ "${1:-}" = "--non-interactive" ] && [ "${2:-}" = "--report" ] || exit 64
[ "${3:-}" = "${POSDK_INSTALL_REPORT:-}" ] || exit 64
[ "${POSDK_INSTALL_PROTOCOL:-}" = "posdk.package-install.v1" ] || exit 64
: "${POSDK_PACKAGE_ROOT:?}" "${POSDK_INSTALL_STATE_ROOT:?}" "${POSDK_PLUGIN_ID:?}" "${POSDK_TARGET_SYSTEM:?}" "${POSDK_TARGET_ARCH:?}"
[ "$POSDK_PLUGIN_ID" = method_loftr ] && [ "${POSDK_PLUGIN_VERSION:-}" = 0.3.0 ] || exit 65
[ "$POSDK_TARGET_SYSTEM" = macos ] && [ "$POSDK_TARGET_ARCH" = arm64 ] || exit 65
[ "${POSDK_INSTALL_NETWORK:-0}" = 1 ] || exit 69

mkdir -p "$POSDK_INSTALL_STATE_ROOT"
env_root="$POSDK_INSTALL_STATE_ROOT/env"
weights_root="$env_root/runtime/weights"
conda_lock="$POSDK_PACKAGE_ROOT/deps/conda-osx-arm64.lock"
pip_lock="$POSDK_PACKAGE_ROOT/deps/requirements-osx-arm64.lock"
smoke="$POSDK_PACKAGE_ROOT/deps/install-smoke.py"
for resource in "$conda_lock" "$pip_lock" "$smoke"; do
  [ -f "$resource" ] && [ ! -L "$resource" ] || exit 65
done

python_ready() {
  [ -x "$env_root/bin/python3.11" ] && [ ! -L "$env_root/bin/python3.11" ] || return 1
  "$env_root/bin/python3.11" -c 'import platform,torch,kornia,cv2,numpy; assert platform.machine()=="arm64"; assert platform.python_version()=="3.11.11"; assert torch.__version__=="2.2.2"; assert kornia.__version__=="0.7.4"; assert cv2.__version__=="4.10.0"; assert numpy.__version__=="1.26.4"' >/dev/null 2>&1
}

if ! python_ready; then
  if [ -e "$env_root" ]; then
    [ -d "$env_root" ] && [ ! -L "$env_root" ] || exit 65
    rm -rf "$env_root"
  fi
  conda_runner=""
  for candidate in /opt/homebrew/bin/micromamba /opt/homebrew/bin/mamba /opt/homebrew/bin/conda \
    /usr/local/bin/micromamba /usr/local/bin/mamba /usr/local/bin/conda \
    "$HOME/miniforge3/bin/mamba" "$HOME/miniforge3/bin/conda" \
    "$HOME/mambaforge/bin/conda" "$HOME/miniconda3/bin/conda" "$HOME/anaconda3/bin/conda"; do
    if [ -x "$candidate" ] && [ ! -L "$candidate" ]; then conda_runner="$candidate"; break; fi
  done
  if [ -z "$conda_runner" ]; then
    bootstrap="$POSDK_INSTALL_STATE_ROOT/bootstrap"
    mkdir -p "$bootstrap"
    curl --http1.1 --fail --location --retry 2 --connect-timeout 30 --max-time 600 --proto '=https' --tlsv1.2 \
      'https://micro.mamba.pm/api/micromamba/osx-arm64/2.3.2' -o "$bootstrap/micromamba.tar.bz2"
    [ "$(shasum -a 256 "$bootstrap/micromamba.tar.bz2" | awk '{print $1}')" = ae0b50b441fef93abd20711f18b074359a7f8f1f523a8046a4d6ab44aa68ff1b ] || exit 65
    tar -xjf "$bootstrap/micromamba.tar.bz2" -C "$bootstrap" bin/micromamba
    conda_runner="$bootstrap/bin/micromamba"
  fi
  case "$(basename "$conda_runner")" in
    micromamba) MAMBA_ROOT_PREFIX="$POSDK_INSTALL_STATE_ROOT/mamba-root" "$conda_runner" create --yes --prefix "$env_root" --file "$conda_lock" ;;
    mamba|conda) "$conda_runner" create --yes --no-default-packages --prefix "$env_root" --file "$conda_lock" ;;
    *) exit 69 ;;
  esac
  "$env_root/bin/python3.11" -m pip install --disable-pip-version-check --no-input --only-binary=:all: --require-hashes -r "$pip_lock"
  "$env_root/bin/python3.11" -m pip check
  python_ready || exit 70
fi

mkdir -p "$weights_root"
fetch_model() {
  model="$1"; digest="$2"
  destination="$weights_root/loftr_$model.ckpt"
  if [ ! -f "$destination" ] || [ "$(shasum -a 256 "$destination" | awk '{print $1}')" != "$digest" ]; then
    [ ! -L "$destination" ] || exit 65
    curl --http1.1 --fail --location --retry 2 --connect-timeout 30 --max-time 600 --proto '=https' --tlsv1.2 \
      "https://cmp.felk.cvut.cz/~mishkdmy/models/loftr_$model.ckpt" -o "$destination.part"
    [ "$(shasum -a 256 "$destination.part" | awk '{print $1}')" = "$digest" ] || exit 65
    mv "$destination.part" "$destination"
  fi
  chmod 400 "$destination"
}
outdoor_sha=21f5bec5968178e8bc8b7633441836fe5de4f47d861dd2cd7dc38e271b0479ec
indoor_sha=d73c54720370ba0690cd477e38404a626e881292cc0be09a6801da0cc6f53198
fetch_model outdoor "$outdoor_sha"
fetch_model indoor "$indoor_sha"
"$env_root/bin/python3.11" "$smoke" --weights "$weights_root/loftr_outdoor.ckpt" --sha256 "$outdoor_sha"

printf '{"schema_version":1,"status":"ok","plugin_id":"method_loftr","system":"macos","architecture":"arm64","dependencies":[{"id":"python","version":"3.11.11","provider":"conda-forge-explicit-lock"},{"id":"torch","version":"2.2.2","provider":"pypi-hash-lock"},{"id":"kornia","version":"0.7.4","provider":"pypi-hash-lock"},{"id":"loftr-outdoor","version":"%s","provider":"upstream-sha256"}],"bindings":[{"id":"python_bin","relative_path":"env/bin"}]}\n' "$outdoor_sha" > "$POSDK_INSTALL_REPORT"
