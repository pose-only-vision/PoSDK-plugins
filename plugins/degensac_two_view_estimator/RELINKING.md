# Rebuilding and relinking with a modified CCMATH

CCMATH 2.2.1 is statically incorporated into the plugin dynamic library and
is licensed under LGPL-2.1-only. This public repository provides the complete
source form of both the library code and the work that uses it, so a recipient
can modify CCMATH and produce a replacement plugin dynamic library.

1. Clone the exact Git tag matching the installed package, for example
   `v1.1.1`.
2. Modify the CCMATH sources below
   `pydegensac/src/pydegensac/matutls/` while preserving the applicable
   notices and LGPL terms.
3. Follow `BUILDING.md` to rebuild
   `posdk_plugin_degensac_two_view_estimator.dylib` against a compatible PoSDK
   Plugin SDK.
4. Package the replacement for Developer installation:

```bash
export POSDK_PLUGIN_SDK_ROOT="/absolute/path/to/PoSDK Plugin SDK"

"$POSDK_PLUGIN_SDK_ROOT/bin/posdk" plugin package \
  --build-root "$PWD/build" \
  --id degensac_two_view_estimator \
  --type method \
  --version 1.1.1 \
  --installer-script "$PWD/install.sh" \
  --installer-config "$PWD/posdk-installer.json" \
  --distribution-model open_plugin_open_dependencies \
  --allow-unsigned \
  --output "$PWD/degensac-two-view-estimator-local.pospkg"
```

An unsigned package is accepted only by the Developer distribution with its
explicit unsigned-package option. Public Marketplace assets must instead be
signed by their publisher and pass the same package, isolated-installation,
runtime-contract, and algorithm gates as the official release.

The LGPL-2.1 text is available at
`pydegensac/src/pydegensac/matutls/lgpl.license`. These instructions describe
the project release layout and are not legal advice.

