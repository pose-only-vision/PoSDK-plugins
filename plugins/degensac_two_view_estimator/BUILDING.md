# Building `degensac_two_view_estimator`

The repository tag `v1.1.1` is the complete source corresponding to the
signed `degensac_two_view_estimator-1.1.1-macos-arm64.pospkg`. It includes the
MPL-2.0 PoSDK adapter, MIT DEGENSAC/LO-RANSAC source, LGPL-2.1-only CCMATH
source, build files, configuration, notices, and package metadata.

## Certified target

- macOS 15 or later
- Apple Silicon (`arm64`)
- the PoSDK Plugin SDK that reports a compatible plugin and Factory ABI
- CMake 3.19 or later and Apple Clang

LAPACK and BLAS resolve to Apple's Accelerate framework. OpenCV and the PoSDK
libraries are host-ABI components supplied by the Plugin SDK and installed
PoSDK application; they are intentionally not copied into this repository.

## Build

```bash
export POSDK_PLUGIN_SDK_ROOT="/absolute/path/to/PoSDK Plugin SDK"

"$POSDK_PLUGIN_SDK_ROOT/bin/posdk" plugin build \
  --source "$PWD" \
  --build-dir "$PWD/build" \
  --sdk-root "$POSDK_PLUGIN_SDK_ROOT" \
  --target degensac_two_view_estimator \
  --config Release
```

The plugin is written to:

```text
build/plugins/methods/posdk_plugin_degensac_two_view_estimator.dylib
```

Verify the complete Marketplace repository and native loader closure before
signing:

```bash
"$POSDK_PLUGIN_SDK_ROOT/bin/posdk" plugin audit-release \
  --source "$PWD" \
  --build-root "$PWD/build" \
  --id degensac_two_view_estimator \
  --type method
```

`publish.sh` accepts only a clean standalone Git repository whose `origin`
matches the requested GitHub repository. It pushes that exact source commit,
then builds, signs, validates, and uploads the matching target-specific
`.pospkg` and `.sha256` assets. The publisher private key remains outside the
repository under `~/.config/PoSDK/publisher-keys/`.

