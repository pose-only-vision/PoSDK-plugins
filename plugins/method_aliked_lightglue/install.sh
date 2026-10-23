#!/bin/sh
set -eu
umask 077

[ "${1:-}" = "--non-interactive" ] || { echo "expected --non-interactive" >&2; exit 64; }
[ "${2:-}" = "--report" ] || { echo "expected --report <path>" >&2; exit 64; }
[ -n "${3:-}" ] && [ "$3" = "${POSDK_INSTALL_REPORT:-}" ] || { echo "invalid report path" >&2; exit 64; }
[ "${POSDK_INSTALL_PROTOCOL:-}" = "posdk.package-install.v1" ] || { echo "unsupported installer protocol" >&2; exit 64; }
: "${POSDK_PACKAGE_ROOT:?}" "${POSDK_INSTALL_STATE_ROOT:?}" "${POSDK_PLUGIN_ID:?}" "${POSDK_PLUGIN_TYPE:?}" "${POSDK_PLUGIN_VERSION:?}" "${POSDK_TARGET_SYSTEM:?}" "${POSDK_TARGET_ARCH:?}"
[ "$POSDK_PLUGIN_ID" = "method_aliked_lightglue" ] && [ "$POSDK_PLUGIN_TYPE" = "method" ] || { echo "plugin identity mismatch" >&2; exit 65; }
[ "$POSDK_PLUGIN_VERSION" = "0.1.1" ] || { echo "plugin version mismatch" >&2; exit 65; }
[ "${POSDK_INSTALL_NETWORK:-0}" = "1" ] || { echo "network permission is required" >&2; exit 69; }
[ "$POSDK_TARGET_SYSTEM" = "macos" ] && [ "$POSDK_TARGET_ARCH" = "arm64" ] || { echo "unsupported target" >&2; exit 65; }

lightglue_commit="eb42fee2d71449efb0aa5c10549752b5d75384d8"
lightglue_archive_sha="e5c696a4c69be4e335b090ce3f70da1060f8aa52e61d463e96a930ebefd57a19"
lightglue_tree_sha="78cc712925a8de33ef9be7e807d0395f916dbd4eeb28f3644243ae2a00297d33"
aliked_commit="683d7c65197395c0b3f01ebe76e1084a27e73a65"
aliked_model_sha="5be8704840ed662d9d8c561bf7279c222092674e7eb05fd0feab94899e9d82f2"
lightglue_model_sha="d975e965b105311a6143194852297dff4f02aea5cc2e10cecfed966ca0e22503"

mkdir -p "$POSDK_INSTALL_STATE_ROOT"
env_root="$POSDK_INSTALL_STATE_ROOT/env"
runtime_root="$POSDK_INSTALL_STATE_ROOT/runtime"
lightglue_root="$runtime_root/LightGlue"
checkpoint_root="$runtime_root/torch/hub/checkpoints"
identity_path="$runtime_root/identity.json"
conda_lock="$POSDK_PACKAGE_ROOT/deps/conda-osx-arm64.lock"
pip_lock="$POSDK_PACKAGE_ROOT/deps/requirements-osx-arm64.lock"
smoke_script="$POSDK_PACKAGE_ROOT/deps/install-smoke.py"
[ -f "$conda_lock" ] && [ ! -L "$conda_lock" ] || { echo "missing signed Conda lock" >&2; exit 65; }
[ -f "$pip_lock" ] && [ ! -L "$pip_lock" ] || { echo "missing signed pip lock" >&2; exit 65; }
[ -f "$smoke_script" ] && [ ! -L "$smoke_script" ] || { echo "missing signed inference smoke test" >&2; exit 65; }

python_is_ready() {
  [ -x "$env_root/bin/python3.11" ] && [ ! -L "$env_root/bin/python3.11" ] || return 1
  "$env_root/bin/python3.11" -c \
    'import platform, PIL, kornia, numpy, torch, torchvision; assert platform.machine() == "arm64"; assert platform.python_version() == "3.11.11"; assert numpy.__version__ == "1.26.4"; assert PIL.__version__ == "12.3.0"; assert kornia.__version__ == "0.6.12"; assert torch.__version__ == "2.2.2"; assert torchvision.__version__ == "0.17.2"' \
    >/dev/null 2>&1
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
  "$env_root/bin/python3.11" -m pip install \
    --disable-pip-version-check --no-input --only-binary=:all: \
    --require-hashes --requirement "$pip_lock"
  "$env_root/bin/python3.11" -m pip check
  python_is_ready || { echo "isolated Python environment smoke test failed" >&2; exit 70; }
fi

runtime_is_ready() {
  [ -d "$lightglue_root" ] && [ ! -L "$lightglue_root" ] || return 1
  [ -f "$identity_path" ] && [ ! -L "$identity_path" ] || return 1
  [ -f "$checkpoint_root/aliked-n16.pth" ] && [ ! -L "$checkpoint_root/aliked-n16.pth" ] || return 1
  [ -f "$checkpoint_root/aliked_lightglue_v0-1_arxiv.pth" ] && [ ! -L "$checkpoint_root/aliked_lightglue_v0-1_arxiv.pth" ] || return 1
  "$env_root/bin/python3.11" "$smoke_script" \
    --runtime-root "$runtime_root" \
    --lightglue-commit "$lightglue_commit" \
    --tree-sha256 "$lightglue_tree_sha" \
    --aliked-sha256 "$aliked_model_sha" \
    --lightglue-sha256 "$lightglue_model_sha" \
    >/dev/null 2>&1
}

if ! runtime_is_ready; then
  if [ -e "$runtime_root" ]; then
    [ ! -L "$runtime_root" ] && [ -d "$runtime_root" ] || { echo "unsafe existing runtime" >&2; exit 65; }
    chmod -R u+rwX "$runtime_root" 2>/dev/null || true
    rm -rf "$runtime_root"
  fi
  mkdir -p "$lightglue_root" "$checkpoint_root"
  download_root="$(mktemp -d "$POSDK_INSTALL_STATE_ROOT/download.XXXXXX")"
  trap 'rm -rf "$download_root"' EXIT HUP INT TERM

  source_archive="$download_root/lightglue.tar.gz"
  curl --fail --location --proto '=https' --tlsv1.2 \
    "https://codeload.github.com/cvg/LightGlue/tar.gz/$lightglue_commit" \
    --output "$source_archive"
  actual_hash="$(shasum -a 256 "$source_archive" | awk '{print $1}')"
  [ "$actual_hash" = "$lightglue_archive_sha" ] || { echo "LightGlue source SHA-256 mismatch" >&2; exit 65; }
  tar -xzf "$source_archive" --strip-components=1 -C "$lightglue_root"

  curl --fail --location --proto '=https' --tlsv1.2 \
    "https://raw.githubusercontent.com/Shiaoming/ALIKED/$aliked_commit/models/aliked-n16.pth" \
    --output "$checkpoint_root/aliked-n16.pth"
  actual_hash="$(shasum -a 256 "$checkpoint_root/aliked-n16.pth" | awk '{print $1}')"
  [ "$actual_hash" = "$aliked_model_sha" ] || { echo "ALIKED model SHA-256 mismatch" >&2; exit 65; }

  curl --fail --location --proto '=https' --tlsv1.2 \
    'https://github.com/cvg/LightGlue/releases/download/v0.1_arxiv/aliked_lightglue.pth' \
    --output "$checkpoint_root/aliked_lightglue_v0-1_arxiv.pth"
  actual_hash="$(shasum -a 256 "$checkpoint_root/aliked_lightglue_v0-1_arxiv.pth" | awk '{print $1}')"
  [ "$actual_hash" = "$lightglue_model_sha" ] || { echo "LightGlue model SHA-256 mismatch" >&2; exit 65; }

  printf '%s\n' \
    "{\"schema_version\":1,\"lightglue_commit\":\"$lightglue_commit\",\"lightglue_archive_sha256\":\"$lightglue_archive_sha\",\"lightglue_tree_sha256\":\"$lightglue_tree_sha\",\"aliked_commit\":\"$aliked_commit\",\"aliked_model_sha256\":\"$aliked_model_sha\",\"lightglue_model_sha256\":\"$lightglue_model_sha\"}" \
    > "$identity_path"

  runtime_is_ready || { echo "pinned ALIKED+LightGlue inference smoke test failed" >&2; exit 70; }
  chmod -R u=rX,go= "$lightglue_root"
  chmod -R u=rX,go= "$runtime_root/torch"
  chmod 400 "$identity_path"
  rm -rf "$download_root"
  trap - EXIT HUP INT TERM
fi

printf '{"schema_version":1,"status":"ok","plugin_id":"%s","system":"%s","architecture":"%s","dependencies":[{"id":"python","version":"3.11.11","provider":"conda-forge-explicit-lock"},{"id":"torch","version":"2.2.2","provider":"pypi-hash-lock"},{"id":"pillow","version":"12.3.0","provider":"pypi-hash-lock"},{"id":"lightglue","version":"%s","provider":"github-commit+sha256"},{"id":"aliked-n16","version":"%s","provider":"github-commit+sha256"}],"bindings":[{"id":"python_bin","relative_path":"env/bin"}]}\n' \
  "$POSDK_PLUGIN_ID" "$POSDK_TARGET_SYSTEM" "$POSDK_TARGET_ARCH" \
  "$lightglue_commit" "$aliked_commit" \
  > "$POSDK_INSTALL_REPORT"
