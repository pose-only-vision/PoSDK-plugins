# LoFTR runtime dependencies

The PoMVG wrapper retains its existing MPL-2.0 license. LoFTR, Kornia,
PyTorch, OpenCV, and the pretrained weights retain their upstream attribution
and licenses. The signed installer creates a plugin-private Python 3.11.11
environment; it does not change a user's Python or Conda environments.

- LoFTR: Jiaming Sun, Zehong Shen, Yu'ang Wang, Hujun Bao, and Xiaowei Zhou.
  https://github.com/zju3dv/LoFTR — Apache-2.0.
- Kornia 0.7.4: https://github.com/kornia/kornia/tree/v0.7.4 — Apache-2.0.
  The Kornia implementation loads the original LoFTR dual-softmax checkpoints.
  This integration does not include the optional SuperGlue optimal-transport code.
- Pretrained checkpoints are downloaded from the URLs published by Kornia,
  verified by SHA-256, and retained in the private runtime.
- PyTorch 2.2.2 and torchvision 0.17.2 retain their upstream aggregate license
  and third-party notices in the exact installed distributions.
- OpenCV Python headless 4.10.0.84 retains its complete wheel licenses,
  including its bundled libraries, in its installed distribution metadata.

The Conda explicit lock and pip hash lock enumerate the exact runtime closure.
All installed package license files remain with that private environment.
This plugin targets macOS arm64. A successful installation includes a real
CPU LoFTR inference check, not merely an import or a substitute matcher.
