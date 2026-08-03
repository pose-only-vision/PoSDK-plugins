# PoSDK Rotation Averaging

`method_rotation_averaging` is a PoSDK Method plugin for global rotation
averaging. The signed Marketplace package provides two selectable backends:

- **Chatterjee** uses the `method_rotation_averaging_chatterjee` backend shipped
  with supported PoSDK Consumer and Developer distributions.
- **GraphOptim** uses the open-source GraphOptim `rotation_estimator`. The
  package installer builds it inside this plugin's private installation state
  and configures the stable executable path automatically.

## Installation

1. Open **Install & Manage Plugins → Marketplace** in PoSDK GUI.
2. Find **Rotation Averaging** under `SfM/Optimization`.
3. Select **Install** and review the publisher and target information.
4. PoSDK downloads the signed target-specific `.pospkg`, verifies its SHA-256,
   Ed25519 signature, ABI, distribution model, and payload hashes, then runs the
   package-internal `install.sh`.

No manual GraphOptim path is required. PoSDK validates the installed
`rotation_estimator`, maintains the stable `graphoptim_bin` binding, and writes
the verified directory to `graphoptim_bin_folder`.

## Supported target

- PoSDK 2.0 or newer
- macOS on Apple Silicon (`arm64`)
- Homebrew and Xcode Command Line Tools for the GraphOptim source build

The installer may use the network to install missing pinned build prerequisites
through Homebrew. It does not install GraphOptim into a system-wide prefix.

## Publication model

This release uses `closed_plugin_open_dependencies`:

- the PoSDK plugin implementation is distributed as a signed binary;
- GraphOptim remains open source under BSD-3-Clause;
- the exact GraphOptim revision, source-archive SHA-256, build recipe, and
  prerequisites are documented in [`deps/README.md`](deps/README.md);
- the public repository contains review metadata and scripts, while executable
  payloads are published only as signed GitHub Release `.pospkg` assets.

## Inputs and outputs

- Input: `data_relative_poses`
- Output: `data_global_poses`
- Group: `SfM/Optimization`

## Publisher handoff

The prepared public metadata is `dist/marketplace-repository`. In the shared
`pose-only-vision/PoSDK-plugins` repository, publish those files below
`plugins/method_rotation_averaging`, create Release tag `v0.1.0`, and upload
these two files without renaming them:

- `method_rotation_averaging-0.1.0-macos-arm64.pospkg`
- `method_rotation_averaging-0.1.0-macos-arm64.pospkg.sha256`

Then open the PoSDK Marketplace publisher form and submit the canonical plugin
directory URL
`https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_rotation_averaging`
and an optional Logo URL. PoSDK.net reads this README and all signed plugin
metadata automatically.

## License

The PoSDK plugin binary is distributed under `LicenseRef-PoSDK-Proprietary`.
GraphOptim and all other third-party components retain their respective
licenses. See [LICENSE.md](LICENSE.md) and
[`deps/README.md`](deps/README.md).
