# ALIKED + LightGlue for PoSDK

Experimental PoSDK Marketplace frontend for dataset-level
[ALIKED](https://github.com/Shiaoming/ALIKED) feature extraction and
[LightGlue](https://github.com/cvg/LightGlue) matching. It accepts
`data_images` and returns `data_features` plus `data_matches` with one stable
feature-index space per image, so pairwise matches can form valid multiview
tracks.

> **Release status: Experimental.** The plugin has reproducible runtime and
> Strecha evidence, but it does not yet outperform the SIFT-based
> `PoSDKPreprocessor` on every accuracy, runtime, coverage, and point-density
> measure. It is therefore not marked Stable.

## Platform and installation

- PoSDK 2.0 or newer
- macOS on Apple silicon (`arm64`)
- Marketplace `.pospkg` installation only; it is not bundled into the PoSDK
  App
- First install requires network access and about **0.8–0.9 GiB** for the
  isolated Python/model runtime; the installer allows up to 60 minutes for
  slower links
- Normal inference is offline after installation

The installer creates a plugin-private Python 3.11.11 environment, downloads
only version- and SHA-256-pinned sources/models, then runs real CPU
ALIKED+LightGlue inference before committing the `python_bin` binding. The
plugin never uses ambient Python and never silently falls back to SIFT or a
traditional matcher.

## Presets

| Preset | `resize_long_edge` | `max_keypoints` | Intended use |
|---|---:|---:|---|
| **Balanced (default)** | 1024 | 1280 | Accuracy-oriented general use |
| **Fast** | 896 | 512 | Lower latency and memory; sparser reconstruction |
| Dense (manual, experimental) | 1600 | 4096 | More sparse-map points; HJ8 smoke peaked near 5.4 GB RSS |
| Extreme (manual, diagnostic only) | 1600 | 8192 | Very high CPU/RSS cost; not recommended for normal use |

The remaining certified comparison settings are exhaustive pairing,
`detection_threshold=0.01`, `filter_threshold=0.10`,
`depth_confidence=0.95`, `width_confidence=0.99`, six CPU threads, and seed
31. Exhaustive matching scales quadratically with image count; use
`pair_mode=contiguous` for long sequences.

`device=auto` and `device=cpu` both select the certified all-CPU path. The
explicit `device=mps` mode is experimental: ALIKED remains on CPU while
LightGlue uses Metal. It is not selected automatically because it was slower
with the pinned runtime in current smoke tests.

## GUI use

1. Install **ALIKED + LightGlue** from PoSDK Marketplace.
2. Add the method under **SfM / Feature Extraction and Matching**.
3. Connect a `data_images` output to its input.
4. Connect `data_features` and `data_matches` to the same calibrated two-view,
   track-building, and reconstruction stages used by the comparison workflow.

The `image_folder` field is only a Developer fallback. Publishable workflows
should connect `data_images` explicitly.

## Corrected exploratory evidence

> **Protocol correction (2026-08-26):** an earlier version of this README
> reported results from PoSDK's `opencv-poengine-strecha` workflow as if they
> were LiRP-paper workflow evidence. Those claims are withdrawn. The corrected
> tests below use the current builtin
> `paper-gnc-ransac-gate-poengine-strecha` backend. They are frontend
> ablations, not reproductions of the paper's original tables.

The corrected comparison keeps the dataset loader, GNC-RANSAC LiRP estimator,
graph gate, rotation averaging, track builder, PoRobust reconstruction,
evaluation, and export fixed. Candidate runs replace only the frontend
provider at the same node and ports. Tests used the certified CPU path on an
Apple M4 Max. Each entry below is a single measured smoke run without warm-up,
so timing is descriptive only.

Values are `PoSDKPreprocessor → ALIKED + LightGlue Balanced`; lower error is
better.

| Scene | Frontend | Relative-pair coverage | Relative R / t median | Global R / t median | PLY points |
|---|---:|---:|---:|---:|---:|
| Herz-Jesus-P8 | 11.718 → 12.892 s | 75.0% → 100% | 0.03169° / 0.06255° → 0.04942° / 0.06040° | 0.02427° / 0.004034 → 0.05094° / 0.004681 | 21,467 → 1,606 |
| entry-P10 | 11.494 → 16.961 s | 82.2% → 100% | 0.02924° / 0.04960° → 0.09730° / 0.13648° | 0.01695° / 0.004632 → 0.09260° / 0.013239 | 18,766 → 2,188 |
| fountain-P11 | 18.238 → 18.124 s | 76.4% → 94.5% | 0.04251° / 0.04816° → 0.06692° / 0.08677° | 0.03401° / 0.002506 → 0.08678° / 0.004119 | 40,980 → 2,434 |

Balanced increased relative-pair coverage in all three scenes, but it achieved
a lower error in only one of the twelve scene/pose-metric cells. It therefore
does not establish an accuracy or runtime advantage over the default SIFT
frontend.

On HJ8, increasing the candidate budget raised PLY points from 1,606 at 1,280
keypoints/image to 5,284 at 4,096 and 9,678 at 8,192. The corresponding
frontend times were 12.9 s, 56.6 s, and 223.4 s, with candidate peak RSS of
approximately 2.9 GB, 5.4 GB, and 9.0 GB. The 8,192 setting still produced
only 45.1% of the default SIFT point count. Lowering LightGlue's filter
threshold from 0.10 to 0.05 at 4,096 keypoints added only 23 PLY points and
worsened all four median pose errors.

Raw-image ETH3D frontend checks are currently negative compatibility evidence,
not a release claim. The paper's ETH3D protocol consumes pre-existing
correspondences and evaluates N-point solvers; replacing that input with
exhaustive raw-image matches is a different experiment. In current `pipes` and
`meadow` checks, at least one arm failed complete-camera publishability or had
substantial accuracy regressions.

The plugin remains **Experimental**. It is useful as an optional learned
frontend with stable multiview IDs and often broader pair coverage, but current
evidence does not show cross-scene or cross-dataset superiority. Sparse-map
density also remains substantially below the default SIFT frontend unless very
high keypoint budgets are used.

## Reproducibility and runtime identity

The runtime is fixed to:

- Python 3.11.11, PyTorch 2.2.2, torchvision 0.17.2
- NumPy 1.26.4, Pillow 12.3.0 RGB decoding, Kornia 0.6.12
- LightGlue commit `eb42fee2d71449efb0aa5c10549752b5d75384d8`
- ALIKED commit `683d7c65197395c0b3f01ebe76e1084a27e73a65`

Source archives, canonical source trees, checkpoints, Python versions, package
versions, the Pillow decoder identity, requested/effective devices, and model
hashes are checked by both the worker and native plugin. The log report records
extraction, matching, worker and frontend wall time, peak RSS, feature counts,
pair counts, and the complete runtime identity. Profiler structured labels also
record the actual preset parameters, so runs with different settings remain
distinguishable.

## Limitations

- Current package target: macOS `arm64` only.
- Current outputs contain point coordinates and stable IDs, not learned
  descriptor tensors; downstream stages consume the LightGlue matches.
- Sparse learned features can produce substantially fewer reconstructed points
  than the SIFT baseline even when camera-pose errors improve.
- No dense/semi-dense correspondence canonicalization is provided.
- OpenCV is intentionally absent from the private runtime. A fail-closed shim
  satisfies only LightGlue's eager import of unused optional helpers; it does
  not implement an alternate image or feature path.
- The plugin fails closed when its private runtime or identity is invalid; it
  has no algorithm fallback.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency and license
information.
