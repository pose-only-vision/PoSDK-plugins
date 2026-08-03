# `degensac_two_view_estimator` dependencies

The release is intentionally self-contained with respect to its algorithm
code. It does not download or build a private runtime during installation.

- DEGENSAC/LO-RANSAC source: vendored under
  `pydegensac/src/pydegensac/degensac`, derived from pydegensac 0.1.2,
  <https://github.com/ducha-aiki/pydegensac>, MIT. The public release commit is
  the immutable source snapshot and includes PoSDK's deterministic sampler
  changes.
- CCMATH 2.2.1 source: vendored under
  `pydegensac/src/pydegensac/matutls`, LGPL-2.1-only. Its full license text is
  `pydegensac/src/pydegensac/matutls/lgpl.license`.
- LAPACK/BLAS: resolved to Apple's Accelerate framework for the certified
  macOS arm64 target; no copy is redistributed in the package.
- OpenCV and PoSDK runtime libraries: supplied by the matching PoSDK host ABI;
  they are not plugin-private dependencies and are not duplicated.

The signed `install.sh` is therefore a no-op dependency installer. See
`BUILDING.md`, `RELINKING.md`, and `THIRD_PARTY_NOTICES.md` for the exact
source-build and license boundary.
