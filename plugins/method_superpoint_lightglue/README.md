# SuperPoint + LightGlue for PoSDK

Dataset-level frontend: data_images -> data_features + data_matches. Each image
is extracted once; every pair reuses its stable feature indices. Coordinates
are restored to the original image with the upstream pixel-center convention.

Choose execution_backend=local for the installer-managed isolated runtime.
For execution_backend=ssh, configure ssh_target and ssh_python_executable from
a matching remote installation. The local python_executable (or managed
python_bin_folder) runs a standard-library SSH controller. ssh_runtime_folder
may override the remote runtime; leave it empty for the managed sibling layout.
Images and the signed worker are uploaded to an owner-only per-job directory.
Input and output SHA-256 checks, bounded results, strict model/source identities,
timeouts and per-job cleanup are required. SSH uses existing keys/agent and
strict host-key checking; model or transport failures fail the run.

The initial signed package targets macOS arm64. Remote macOS arm64 is tested;
Linux/CUDA support in the worker requires the exact declared Python/package
versions and model identities and must be independently validated before use.
auto/cpu use CPU. Explicit mps keeps the extractor on CPU and runs LightGlue
on MPS; cuda explicitly requires CUDA for both stages.

Defaults: exhaustive pairs, 1280 keypoints, long edge 1024, seed 31. Run a
same-resolution/same-pair-graph comparison before selecting a larger budget.

## Wrapper source availability

Recipients of the wrapper binary can obtain the corresponding MPL-2.0-covered
wrapper source from the maintainer at qicaiCN@gmail.com. Dependency sources and weights
are obtained separately under the licenses and pinned recipes in this package.

## Research license

SuperPoint has a restrictive noncommercial research license. Read
THIRD_PARTY_NOTICES.md and deps/SUPERPOINT-LICENSE.txt before installation.
The PoSDK wrapper license does not override the upstream license.
