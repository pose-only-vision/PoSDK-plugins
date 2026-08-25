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
| High resolution (manual) | 1600 | 4096 | Diagnostic accuracy mode; two-image smoke peaked near 5.4 GB RSS |

The remaining certified comparison settings are exhaustive pairing,
`detection_threshold=0.01`, `filter_threshold=0.10`,
`depth_confidence=0.95`, `width_confidence=0.99`, four CPU threads, and seed
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

## Strecha evidence

The comparison used the existing PoSDK Strecha pipeline with the frontend as
the only changed component. The baseline was `PoSDKPreprocessor` configured as
SIFT + `CASCADE_HASHING_L2`; calibration, two-view estimation, rotation
averaging, track building, global reconstruction, evaluation, and export were
held fixed. Tests ran on an Apple M4 Max using the certified CPU path.

For Herz-Jesus-P8, Balanced was measured after one warm-up over five alternating
A/B and B/A repetitions. Values are medians; lower error and time are better.

| Metric | PoSDKPreprocessor | ALIKED + LightGlue | Candidate ratio |
|---|---:|---:|---:|
| Frontend time | 11.752 s | 11.292 s | 0.961× |
| End-to-end wall time | 13.692 s | 12.605 s | 0.921× |
| Relative rotation error | 0.07892° | 0.05837° | 0.740× |
| Relative translation error | 0.07980° | 0.07504° | 0.940× |
| Global rotation error | 0.13166° | 0.04093° | 0.311× |
| Global translation error | 0.01778 | 0.00577 | 0.325× |
| Relative-pair coverage | 96.4% | 100% | — |
| Reconstructed points | 8610 | 979 | — |

The final Pillow runtime was also checked once on all six scenes with the Fast
preset. All 12 runs passed snapshot/runtime attestation. Frontend time improved
in 6 of 6 scenes; the per-scene candidate/baseline ratios were 0.552, 0.824,
0.538, 0.842, 0.561, and 0.878 (median 0.692). Eighteen of 24 pose-error cells
improved and all cameras registered. Entry-P10, fountain-P11, and
Herz-Jesus-P25 still contained pose-error regressions; castle-P19,
Herz-Jesus-P25, and castle-P30 had lower pair coverage. Every candidate point
cloud was much sparser than its SIFT baseline.

These results support two bounded claims: Balanced exceeded the baseline on
HJ8 under the repeated protocol, while Fast is a promising speed-oriented
preset across the six single-run checks. They do **not** establish uniform
accuracy superiority or dense-map equivalence. Full multi-scene repeated runs
and broader hardware coverage are required before a Stable designation.

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
