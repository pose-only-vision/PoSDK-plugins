#!/usr/bin/env python3
"""Pinned dataset-level ALIKED + LightGlue worker for PoSDK.

The worker intentionally has no fallback implementation. Any dependency,
source, model, device, image, or protocol mismatch fails the complete run.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import pathlib
import platform
import resource
import struct
import sys
import tempfile
import time
import traceback
import types
from typing import BinaryIO, Iterable


REQUEST_MAGIC = b"PALGREQ1"
RESULT_MAGIC = b"PALGRES1"
PROTOCOL_VERSION = 2
EXTRACTOR_NAME = "aliked-n16"
MATCHER_NAME = "aliked_lightglue"
EXPECTED_PYTHON = "3.11.11"
EXPECTED_TORCH = "2.2.2"
EXPECTED_TORCHVISION = "0.17.2"
EXPECTED_KORNIA = "0.6.12"
EXPECTED_PILLOW = "12.3.0"
IMAGE_DECODER_IDENTITY = f"Pillow-RGB-{EXPECTED_PILLOW}"
MAX_REQUEST_BYTES = 16 * 1024 * 1024
MAX_IMAGES = 10000
MAX_PATH_BYTES = 1024 * 1024


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="PoSDK dataset-level ALIKED + LightGlue worker"
    )
    parser.add_argument("--request", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--runtime-root", required=True)
    parser.add_argument("--device", choices=("auto", "mps", "cpu"), required=True)
    parser.add_argument(
        "--pair-mode", choices=("exhaustive", "contiguous"), required=True
    )
    parser.add_argument("--pair-window", type=int, required=True)
    parser.add_argument("--max-keypoints", type=int, required=True)
    parser.add_argument("--resize-long-edge", type=int, required=True)
    parser.add_argument("--detection-threshold", type=float, required=True)
    parser.add_argument("--filter-threshold", type=float, required=True)
    parser.add_argument("--depth-confidence", type=float, required=True)
    parser.add_argument("--width-confidence", type=float, required=True)
    parser.add_argument("--cpu-threads", type=int, required=True)
    parser.add_argument("--seed", type=int, required=True)
    parser.add_argument("--expected-lightglue-commit", required=True)
    parser.add_argument("--expected-lightglue-archive-sha256", required=True)
    parser.add_argument("--expected-lightglue-tree-sha256", required=True)
    parser.add_argument("--expected-aliked-commit", required=True)
    parser.add_argument("--expected-aliked-model-sha256", required=True)
    parser.add_argument("--expected-lightglue-model-sha256", required=True)
    return parser.parse_args()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def require_regular_file(path: pathlib.Path, description: str) -> pathlib.Path:
    require(path.is_absolute(), f"{description} path is not absolute")
    stat_result = path.lstat()
    require(not path.is_symlink(), f"{description} must not be a symlink")
    require(path.is_file(), f"{description} is not a regular file")
    require(stat_result.st_nlink == 1, f"{description} must have one hard link")
    return path


def install_unavailable_cv2_stub() -> None:
    """Satisfy unused LightGlue optional imports without installing OpenCV.

    The pinned LightGlue package imports its SIFT and image helper modules from
    ``__init__`` even when only ALIKED is requested. Those optional paths refer
    to ``cv2``. This fail-closed stub exposes only the annotation used while
    importing SIFT; any attempt to execute an OpenCV path raises immediately.
    """
    require("cv2" not in sys.modules, "OpenCV was imported before the compatibility gate")
    stub = types.ModuleType("cv2")
    stub.Feature2D = object

    def unavailable(name: str):
        raise AttributeError(
            f"OpenCV API {name!r} is unavailable in the certified Pillow runtime"
        )

    stub.__getattr__ = unavailable
    sys.modules["cv2"] = stub


def load_rgb_tensor(image_path: pathlib.Path, image_module, numpy_module, torch_module):
    """Decode one image as an owned RGB uint8 array and normalize to CHW."""
    with image_module.open(image_path) as image:
        image.load()
        pixels = numpy_module.array(
            image.convert("RGB"), dtype=numpy_module.uint8, copy=True
        )
    require(
        pixels.ndim == 3 and pixels.shape[2] == 3 and pixels.size > 0,
        "Pillow returned an invalid RGB image",
    )
    return (
        torch_module.from_numpy(pixels)
        .permute(2, 0, 1)
        .contiguous()
        .to(dtype=torch_module.float32)
        .div_(255.0)
    )


def read_exact(stream: BinaryIO, count: int) -> bytes:
    data = stream.read(count)
    require(len(data) == count, "request is truncated")
    return data


def read_u32(stream: BinaryIO) -> int:
    return struct.unpack("<I", read_exact(stream, 4))[0]


def read_request(path: pathlib.Path) -> list[pathlib.Path]:
    path = require_regular_file(path.resolve(strict=True), "request")
    require(path.stat().st_size <= MAX_REQUEST_BYTES, "request is too large")
    with path.open("rb") as stream:
        require(read_exact(stream, 8) == REQUEST_MAGIC, "request magic mismatch")
        require(read_u32(stream) == PROTOCOL_VERSION, "request version mismatch")
        image_count = read_u32(stream)
        require(2 <= image_count <= MAX_IMAGES, "request image count is invalid")
        images: list[pathlib.Path] = []
        for _ in range(image_count):
            length = read_u32(stream)
            require(0 < length <= MAX_PATH_BYTES, "request image path is invalid")
            raw_path = read_exact(stream, length)
            require(b"\0" not in raw_path, "request image path contains NUL")
            image = pathlib.Path(raw_path.decode("utf-8", errors="strict"))
            image = image.resolve(strict=True)
            require_regular_file(image, "image")
            images.append(image)
        require(stream.read(1) == b"", "request contains trailing data")
    return images


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_tree_sha256(root: pathlib.Path) -> str:
    require(root.is_absolute() and root.is_dir(), "LightGlue source root is invalid")
    files: list[pathlib.Path] = []
    for path in root.rglob("*"):
        relative = path.relative_to(root)
        if ".git" in relative.parts or "__pycache__" in relative.parts:
            continue
        if path.suffix == ".pyc":
            continue
        if path.is_symlink():
            raise RuntimeError(f"LightGlue source contains a symlink: {relative}")
        if path.is_file():
            files.append(path)

    digest = hashlib.sha256()
    for path in sorted(files, key=lambda item: item.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix().encode("utf-8")
        size = path.stat().st_size
        digest.update(struct.pack("<I", len(relative)))
        digest.update(relative)
        digest.update(struct.pack("<Q", size))
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    return digest.hexdigest()


def load_runtime_identity(
    runtime_root: pathlib.Path, args: argparse.Namespace
) -> tuple[pathlib.Path, pathlib.Path, pathlib.Path]:
    identity_path = require_regular_file(
        (runtime_root / "identity.json").resolve(strict=True), "runtime identity"
    )
    identity = json.loads(identity_path.read_text(encoding="utf-8"))
    required_keys = {
        "schema_version",
        "lightglue_commit",
        "lightglue_archive_sha256",
        "lightglue_tree_sha256",
        "aliked_commit",
        "aliked_model_sha256",
        "lightglue_model_sha256",
    }
    require(set(identity) == required_keys, "runtime identity schema mismatch")
    require(identity["schema_version"] == 1, "runtime identity version mismatch")
    require(
        identity["lightglue_commit"] == args.expected_lightglue_commit,
        "LightGlue commit mismatch",
    )
    require(
        identity["lightglue_archive_sha256"]
        == args.expected_lightglue_archive_sha256,
        "LightGlue archive identity mismatch",
    )
    require(
        identity["lightglue_tree_sha256"]
        == args.expected_lightglue_tree_sha256,
        "LightGlue tree identity mismatch",
    )
    require(
        identity["aliked_model_sha256"] == args.expected_aliked_model_sha256,
        "ALIKED model identity mismatch",
    )
    require(
        identity["aliked_commit"] == args.expected_aliked_commit,
        "ALIKED commit mismatch",
    )
    require(
        identity["lightglue_model_sha256"]
        == args.expected_lightglue_model_sha256,
        "LightGlue model identity mismatch",
    )

    source_root = (runtime_root / "LightGlue").resolve(strict=True)
    require(
        source_tree_sha256(source_root) == args.expected_lightglue_tree_sha256,
        "LightGlue source tree SHA-256 mismatch",
    )
    checkpoint_root = (runtime_root / "torch/hub/checkpoints").resolve(strict=True)
    aliked_model = require_regular_file(
        (checkpoint_root / "aliked-n16.pth").resolve(strict=True), "ALIKED model"
    )
    lightglue_model = require_regular_file(
        (checkpoint_root / "aliked_lightglue_v0-1_arxiv.pth").resolve(strict=True),
        "LightGlue model",
    )
    require(
        sha256_file(aliked_model) == args.expected_aliked_model_sha256,
        "ALIKED model SHA-256 mismatch",
    )
    require(
        sha256_file(lightglue_model) == args.expected_lightglue_model_sha256,
        "LightGlue model SHA-256 mismatch",
    )
    return source_root, aliked_model, lightglue_model


def selected_pairs(
    image_count: int, pair_mode: str, pair_window: int
) -> list[tuple[int, int]]:
    if pair_mode == "exhaustive":
        return [
            (image_i, image_j)
            for image_i in range(image_count)
            for image_j in range(image_i + 1, image_count)
        ]
    return [
        (image_i, image_j)
        for image_i in range(image_count)
        for image_j in range(
            image_i + 1, min(image_count, image_i + pair_window + 1)
        )
    ]


def synchronize(torch_module, device) -> None:
    if device.type == "mps":
        torch_module.mps.synchronize()
    elif device.type == "cuda":
        torch_module.cuda.synchronize(device)


def move_feature_dict(torch_module, features: dict[str, object], device):
    return {
        key: value.to(device) if isinstance(value, torch_module.Tensor) else value
        for key, value in features.items()
    }


def pack_string(stream: BinaryIO, value: str) -> None:
    encoded = value.encode("utf-8")
    require(len(encoded) <= 256, "identity string is too long")
    stream.write(struct.pack("<I", len(encoded)))
    stream.write(encoded)


def atomic_write_result(
    output_path: pathlib.Path,
    metrics: dict[str, int],
    identity: dict[str, str],
    keypoints: list[list[tuple[float, float, float]]],
    pairs: list[tuple[int, int, list[tuple[int, int, float]]]],
) -> None:
    output_parent = output_path.parent.resolve(strict=True)
    require(not output_path.exists(), "worker output already exists")
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w+b", prefix="result-", suffix=".tmp", dir=output_parent, delete=False
        ) as stream:
            temporary_name = stream.name
            os.chmod(temporary_name, 0o600)
            stream.write(RESULT_MAGIC)
            stream.write(
                struct.pack(
                    "<IIIQQQQQ",
                    PROTOCOL_VERSION,
                    len(keypoints),
                    len(pairs),
                    metrics["startup_ns"],
                    metrics["extraction_ns"],
                    metrics["matching_ns"],
                    metrics["worker_total_ns"],
                    metrics["peak_rss_bytes"],
                )
            )
            for name in (
                "extractor",
                "matcher",
                "requested_device",
                "device",
                "extractor_device",
                "matcher_device",
                "python_version",
                "torch_version",
                "torchvision_version",
                "kornia_version",
                "image_decoder",
                "lightglue_commit",
                "lightglue_archive_sha256",
                "lightglue_tree_sha256",
                "aliked_commit",
                "aliked_model_sha256",
                "lightglue_model_sha256",
            ):
                pack_string(stream, identity[name])

            for image_keypoints in keypoints:
                stream.write(struct.pack("<I", len(image_keypoints)))
                for x, y, score in image_keypoints:
                    stream.write(struct.pack("<fff", x, y, score))

            for image_i, image_j, image_matches in pairs:
                stream.write(struct.pack("<III", image_i, image_j, len(image_matches)))
                for feature_i, feature_j, score in image_matches:
                    stream.write(struct.pack("<IIf", feature_i, feature_j, score))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, output_path)
        temporary_name = None
    finally:
        if temporary_name is not None:
            try:
                os.unlink(temporary_name)
            except FileNotFoundError:
                pass


def main() -> int:
    total_start = time.perf_counter_ns()
    args = parse_args()
    require(platform.system() == "Darwin", "worker supports macOS only")
    require(platform.machine() == "arm64", "worker supports arm64 only")
    require(platform.python_version() == EXPECTED_PYTHON, "Python version mismatch")
    require(128 <= args.max_keypoints <= 20000, "max-keypoints is invalid")
    require(256 <= args.resize_long_edge <= 4096, "resize-long-edge is invalid")
    require(1 <= args.pair_window <= 1000, "pair-window is invalid")
    require(1 <= args.cpu_threads <= 64, "cpu-threads is invalid")
    require(-1.0 <= args.detection_threshold <= 1.0, "detection threshold invalid")
    require(0.0 <= args.filter_threshold <= 1.0, "filter threshold invalid")
    require(-1.0 <= args.depth_confidence <= 1.0, "depth confidence invalid")
    require(-1.0 <= args.width_confidence <= 1.0, "width confidence invalid")

    request_path = pathlib.Path(args.request).resolve(strict=True)
    output_path = pathlib.Path(args.output)
    require(output_path.is_absolute(), "worker output path is not absolute")
    runtime_root = pathlib.Path(args.runtime_root).resolve(strict=True)
    require(runtime_root.is_dir(), "runtime root is invalid")
    images = read_request(request_path)
    source_root, _, _ = load_runtime_identity(runtime_root, args)

    sys.dont_write_bytecode = True
    sys.path.insert(0, str(source_root))
    import PIL  # pylint: disable=import-outside-toplevel
    import kornia  # pylint: disable=import-outside-toplevel
    import numpy as np  # pylint: disable=import-outside-toplevel
    import torch  # pylint: disable=import-outside-toplevel
    import torchvision  # pylint: disable=import-outside-toplevel
    from PIL import Image  # pylint: disable=import-outside-toplevel

    install_unavailable_cv2_stub()
    from lightglue import ALIKED, LightGlue  # pylint: disable=import-outside-toplevel

    require(torch.__version__ == EXPECTED_TORCH, "PyTorch version mismatch")
    require(torchvision.__version__ == EXPECTED_TORCHVISION, "torchvision mismatch")
    require(kornia.__version__ == EXPECTED_KORNIA, "Kornia version mismatch")
    require(PIL.__version__ == EXPECTED_PILLOW, "Pillow version mismatch")
    require(np.__version__ == "1.26.4", "NumPy version mismatch")

    torch.manual_seed(args.seed)
    np.random.seed(args.seed)
    torch.set_num_threads(args.cpu_threads)
    torch.set_num_interop_threads(1)
    torch.hub.set_dir(str(runtime_root / "torch/hub"))

    # The pinned PyTorch/LightGlue runtime is certified on CPU. MPS remains an
    # explicit diagnostic only because current Strecha measurements show a
    # matching-time and peak-memory regression; auto must therefore stay CPU.
    use_mps_matcher = args.device == "mps" and torch.backends.mps.is_available()
    if args.device == "mps":
        require(torch.backends.mps.is_built(), "PyTorch was not built with MPS")
        require(torch.backends.mps.is_available(), "MPS is unavailable")
    extractor_device = torch.device("cpu")
    matcher_device = torch.device("mps" if use_mps_matcher else "cpu")
    device_name = "cpu+mps" if use_mps_matcher else "cpu"

    extractor = ALIKED(
        model_name=EXTRACTOR_NAME,
        max_num_keypoints=args.max_keypoints,
        detection_threshold=args.detection_threshold,
        nms_radius=2,
    ).eval().to(extractor_device)
    matcher = LightGlue(
        features="aliked",
        filter_threshold=args.filter_threshold,
        depth_confidence=args.depth_confidence,
        width_confidence=args.width_confidence,
        flash=False,
        mp=False,
    ).eval().to(matcher_device)
    synchronize(torch, matcher_device)
    startup_end = time.perf_counter_ns()

    feature_tensors: list[dict[str, object]] = []
    output_keypoints: list[list[tuple[float, float, float]]] = []
    extraction_start = time.perf_counter_ns()
    with torch.inference_mode():
        for image_path in images:
            image_tensor = load_rgb_tensor(image_path, Image, np, torch).to(
                extractor_device
            )
            image_height, image_width = image_tensor.shape[-2:]
            features = extractor.extract(
                image_tensor, resize=args.resize_long_edge, side="long"
            )
            points = features["keypoints"][0].detach().cpu().float().numpy()
            scores = features["keypoint_scores"][0].detach().cpu().float().numpy()
            require(
                points.ndim == 2 and points.shape[1] == 2,
                "ALIKED returned invalid keypoint shape",
            )
            require(
                scores.ndim == 1 and len(scores) == len(points),
                "ALIKED returned invalid score shape",
            )
            require(len(points) <= args.max_keypoints, "ALIKED exceeded keypoint bound")
            serialized_points: list[tuple[float, float, float]] = []
            for point, score_value in zip(points, scores):
                x, y, score = float(point[0]), float(point[1]), float(score_value)
                require(
                    math.isfinite(x)
                    and math.isfinite(y)
                    and math.isfinite(score)
                    and -0.501 <= x <= image_width - 0.499
                    and -0.501 <= y <= image_height - 0.499
                    and 0.0 <= score <= 1.0001,
                    "ALIKED returned an invalid keypoint",
                )
                serialized_points.append((x, y, score))
            feature_tensors.append(move_feature_dict(torch, features, matcher_device))
            output_keypoints.append(serialized_points)
            del image_tensor
    synchronize(torch, extractor_device)
    extraction_end = time.perf_counter_ns()

    pair_graph = selected_pairs(len(images), args.pair_mode, args.pair_window)
    output_pairs: list[tuple[int, int, list[tuple[int, int, float]]]] = []
    matching_start = time.perf_counter_ns()
    with torch.inference_mode():
        for image_i, image_j in pair_graph:
            prediction = matcher(
                {
                    "image0": feature_tensors[image_i],
                    "image1": feature_tensors[image_j],
                }
            )
            synchronize(torch, matcher_device)
            compact_matches = prediction["matches"][0].detach().cpu().long().numpy()
            compact_scores = prediction["scores"][0].detach().cpu().float().numpy()
            require(
                compact_matches.ndim == 2 and compact_matches.shape[1] == 2,
                "LightGlue returned invalid compact match shape",
            )
            require(
                compact_scores.ndim == 1
                and len(compact_scores) == len(compact_matches),
                "LightGlue compact scores do not align with compact matches",
            )
            image_matches: list[tuple[int, int, float]] = []
            seen: set[tuple[int, int]] = set()
            for indices, score_value in zip(compact_matches, compact_scores):
                feature_i, feature_j = int(indices[0]), int(indices[1])
                score = float(score_value)
                require(
                    0 <= feature_i < len(output_keypoints[image_i])
                    and 0 <= feature_j < len(output_keypoints[image_j])
                    and math.isfinite(score)
                    and 0.0 <= score <= 1.0001,
                    "LightGlue returned an invalid compact match",
                )
                pair_key = (feature_i, feature_j)
                require(pair_key not in seen, "LightGlue returned a duplicate match")
                seen.add(pair_key)
                image_matches.append((feature_i, feature_j, score))
            image_matches.sort(key=lambda item: (item[0], item[1]))
            output_pairs.append((image_i, image_j, image_matches))
    synchronize(torch, matcher_device)
    matching_end = time.perf_counter_ns()

    require(len(output_pairs) == len(pair_graph), "pair graph was not fully evaluated")
    peak_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if platform.system() != "Darwin":
        peak_rss *= 1024
    worker_end = time.perf_counter_ns()
    metrics = {
        "startup_ns": startup_end - total_start,
        "extraction_ns": extraction_end - extraction_start,
        "matching_ns": matching_end - matching_start,
        "worker_total_ns": worker_end - total_start,
        "peak_rss_bytes": int(peak_rss),
    }
    identity = {
        "extractor": EXTRACTOR_NAME,
        "matcher": MATCHER_NAME,
        "requested_device": args.device,
        "device": device_name,
        "extractor_device": extractor_device.type,
        "matcher_device": matcher_device.type,
        "python_version": platform.python_version(),
        "torch_version": torch.__version__,
        "torchvision_version": torchvision.__version__,
        "kornia_version": kornia.__version__,
        "image_decoder": IMAGE_DECODER_IDENTITY,
        "lightglue_commit": args.expected_lightglue_commit,
        "lightglue_archive_sha256": args.expected_lightglue_archive_sha256,
        "lightglue_tree_sha256": args.expected_lightglue_tree_sha256,
        "aliked_commit": args.expected_aliked_commit,
        "aliked_model_sha256": args.expected_aliked_model_sha256,
        "lightglue_model_sha256": args.expected_lightglue_model_sha256,
    }
    atomic_write_result(output_path, metrics, identity, output_keypoints, output_pairs)
    print(
        json.dumps(
            {
                "status": "ok",
                "schema_version": PROTOCOL_VERSION,
                "identity": identity,
                "metrics": metrics,
                "images": len(output_keypoints),
                "pairs": len(output_pairs),
                "features": sum(len(points) for points in output_keypoints),
                "matches": sum(len(pair[2]) for pair in output_pairs),
            },
            sort_keys=True,
        ),
        flush=True,
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exception:  # noqa: BLE001 - terminal worker boundary
        print(
            json.dumps(
                {
                    "status": "error",
                    "error_type": type(exception).__name__,
                    "message": str(exception),
                },
                sort_keys=True,
            ),
            file=sys.stderr,
            flush=True,
        )
        traceback.print_exc(file=sys.stderr)
        raise SystemExit(70)
