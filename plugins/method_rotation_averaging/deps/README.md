# GraphOptim dependency

This plugin uses the open-source
[AIBluefisher/GraphOptim](https://github.com/AIBluefisher/GraphOptim) project at commit
`9d932464831bff46b1b5a118ececb2e00c42d5c6` (BSD-3-Clause). The signed
`.pospkg` embeds a source archive containing the PoSDK compatibility fixes.
The signed archive is named `deps/GraphOptim-source.tar.gz` and has SHA-256
`e3a3bd61123091a6ebb254c5517fd3b83c0834b74c4ea87cee316b8ff25c5251`.
`install.sh` verifies that archive, installs the pinned build prerequisites
through Homebrew, builds only `rotation_estimator`, and places the executable
below `POSDK_INSTALL_STATE_ROOT/bin`.

The build prerequisites are `cmake`, `eigen`, `ceres-solver`, `glog`, `gflags`,
`suite-sparse`, `libomp`, `tbb`, and `metis`. The installer records their exact
Homebrew versions in the local build contract and rebuilds GraphOptim whenever
that contract changes. The target build is restricted to macOS arm64 and uses
the package deployment baseline rather than the build machine's default.

The installer reports `bin` through the signed `graphoptim_bin` configuration
binding. PoSDK validates `rotation_estimator` and writes the resulting absolute
directory into `method_rotation_averaging.graphoptim_bin_folder`; users do not
need to locate or enter the path manually.
