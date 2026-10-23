# `method_loftr` private dependencies

This macOS arm64 plugin creates an isolated Python 3.11.11 runtime below
`POSDK_INSTALL_STATE_ROOT/env`. The explicit Conda lock fixes each package URL
and SHA-256. The pip lock fixes the complete binary-wheel closure, including
PyTorch 2.2.2, Kornia 0.7.4, NumPy 1.26.4 and OpenCV headless 4.10.0.84.
`install.sh` is the executable creation recipe and `install-smoke.py` loads
the real outdoor LoFTR model on CPU and checks finite correspondences.

The inference implementation is [Kornia 0.7.4 source](https://github.com/kornia/kornia/tree/v0.7.4)
(Apache-2.0), using the original [LoFTR project](https://github.com/zju3dv/LoFTR)
(Apache-2.0) and its dual-softmax checkpoints. The optional SuperGlue
optimal-transport implementation is not included. Upstream attribution and
license notices are retained in `THIRD_PARTY_NOTICES.md` and the installed
distributions.

Kornia's pinned model URLs use HTTPS at `cmp.felk.cvut.cz/~mishkdmy/models/`:

- `loftr_outdoor.ckpt`: `21f5bec5968178e8bc8b7633441836fe5de4f47d861dd2cd7dc38e271b0479ec`
- `loftr_indoor.ckpt`: `d73c54720370ba0690cd477e38404a626e881292cc0be09a6801da0cc6f53198`

The installer uses a local Conda-compatible environment creator, or downloads
micromamba 2.3.2 from its official osx-arm64 endpoint, SHA-256
`ae0b50b441fef93abd20711f18b074359a7f8f1f523a8046a4d6ab44aa68ff1b`.
It never activates or changes the user's environment. See
`THIRD_PARTY_NOTICES.md` for upstream licenses and authors; the full package
licenses remain in the installed distributions.

Native libraries included in the final package must be declared by the
plugin's same-stem `.dependencies.json` and use loader-relative paths.
