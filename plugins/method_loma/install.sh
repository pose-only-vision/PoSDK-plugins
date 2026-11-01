#!/bin/sh
set -eu
umask 077
[ "${1:-}" = "--non-interactive" ] || exit 64
[ "${2:-}" = "--report" ] || exit 64
[ -n "${3:-}" ] && [ "$3" = "${POSDK_INSTALL_REPORT:-}" ] || exit 64
[ "${POSDK_INSTALL_PROTOCOL:-}" = "posdk.package-install.v1" ] || exit 64
: "${POSDK_PACKAGE_ROOT:?}" "${POSDK_INSTALL_STATE_ROOT:?}" "${POSDK_PLUGIN_ID:?}" "${POSDK_PLUGIN_VERSION:?}" "${POSDK_TARGET_SYSTEM:?}" "${POSDK_TARGET_ARCH:?}"
[ "$POSDK_PLUGIN_ID" = "method_loma" ] && [ "$POSDK_PLUGIN_VERSION" = "0.1.0" ] || exit 65
[ "$POSDK_TARGET_SYSTEM" = "macos" ] && [ "$POSDK_TARGET_ARCH" = "arm64" ] || exit 65
[ "${POSDK_INSTALL_NETWORK:-0}" = "1" ] || exit 69
mkdir -p "$POSDK_INSTALL_STATE_ROOT"
env_root="$POSDK_INSTALL_STATE_ROOT/env"
conda_lock="$POSDK_PACKAGE_ROOT/deps/conda-osx-arm64.lock"
[ -f "$conda_lock" ] && [ ! -L "$conda_lock" ] || exit 65
python_is_ready() {
  [ -x "$env_root/bin/python3.11" ] && [ ! -L "$env_root/bin/python3.11" ] || return 1
  "$env_root/bin/python3.11" -I -c \
    'import platform,ssl,subprocess,tarfile,hashlib,json; assert platform.machine()=="arm64"; assert platform.python_version()=="3.11.11"' >/dev/null 2>&1
}
if ! python_is_ready; then
  if [ -e "$env_root" ]; then
    [ ! -L "$env_root" ] && [ -d "$env_root" ] || { echo "unsafe existing environment" >&2; exit 65; }
    rm -rf "$env_root"
  fi
  conda_runner=""
  for candidate in \
    /opt/homebrew/bin/micromamba /opt/homebrew/bin/mamba /opt/homebrew/bin/conda \
    /usr/local/bin/micromamba /usr/local/bin/mamba /usr/local/bin/conda \
    "$HOME/miniforge3/bin/mamba" "$HOME/miniforge3/bin/conda" \
    "$HOME/mambaforge/bin/mamba" "$HOME/mambaforge/bin/conda" \
    "$HOME/miniconda3/bin/conda" "$HOME/anaconda3/bin/conda"
  do
    if [ -x "$candidate" ] && [ ! -L "$candidate" ]; then
      conda_runner="$candidate"
      break
    fi
  done
  if [ -z "$conda_runner" ]; then
    bootstrap_root="$POSDK_INSTALL_STATE_ROOT/bootstrap"
    bootstrap_archive="$bootstrap_root/micromamba-2.3.2-osx-arm64.tar.bz2"
    mkdir -p "$bootstrap_root"
    curl --fail --location --proto '=https' --tlsv1.2 \
      'https://micro.mamba.pm/api/micromamba/osx-arm64/2.3.2' \
      --output "$bootstrap_archive"
    actual_hash="$(shasum -a 256 "$bootstrap_archive" | awk '{print $1}')"
    [ "$actual_hash" = "ae0b50b441fef93abd20711f18b074359a7f8f1f523a8046a4d6ab44aa68ff1b" ] || {
      echo "micromamba SHA-256 mismatch" >&2
      exit 65
    }
    tar -xjf "$bootstrap_archive" -C "$bootstrap_root" bin/micromamba
    conda_runner="$bootstrap_root/bin/micromamba"
    [ -x "$conda_runner" ] && [ ! -L "$conda_runner" ] || { echo "invalid micromamba bootstrap" >&2; exit 65; }
  fi
  case "$(basename "$conda_runner")" in
    micromamba)
      MAMBA_ROOT_PREFIX="$POSDK_INSTALL_STATE_ROOT/mamba-root" \
        "$conda_runner" create --yes --prefix "$env_root" --file "$conda_lock"
      ;;
    mamba|conda)
      "$conda_runner" create --yes --no-default-packages \
        --prefix "$env_root" --file "$conda_lock"
      ;;
    *) echo "unsupported Conda runner" >&2; exit 69 ;;
  esac
  python_is_ready || { echo "isolated Python environment smoke test failed" >&2; exit 70; }
fi

"$env_root/bin/python3.11" -I "$POSDK_PACKAGE_ROOT/deps/client-smoke.py"
printf '{"schema_version":1,"status":"ok","plugin_id":"%s","system":"%s","architecture":"%s","dependencies":[{"id":"controller_python","version":"3.11.11","provider":"conda-forge-explicit-lock"}],"bindings":[{"id":"python_bin","relative_path":"env/bin"}]}\n' \
  "$POSDK_PLUGIN_ID" "$POSDK_TARGET_SYSTEM" "$POSDK_TARGET_ARCH" > "$POSDK_INSTALL_REPORT"
