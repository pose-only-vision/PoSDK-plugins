#!/usr/bin/env python3
"""Installer-only identity and real-inference smoke test."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import platform
import struct
import sys
import types


def install_unavailable_cv2_stub() -> None:
    if "cv2" in sys.modules:
        raise RuntimeError("OpenCV was imported before the compatibility gate")
    stub = types.ModuleType("cv2")
    stub.Feature2D = object

    def unavailable(name: str):
        raise AttributeError(
            f"OpenCV API {name!r} is unavailable in the certified Pillow runtime"
        )

    stub.__getattr__ = unavailable
    sys.modules["cv2"] = stub


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def tree_sha256(root: pathlib.Path) -> str:
    digest = hashlib.sha256()
    files = []
    for path in root.rglob("*"):
        relative = path.relative_to(root)
        if ".git" in relative.parts or "__pycache__" in relative.parts:
            continue
        if path.suffix == ".pyc":
            continue
        if path.is_symlink():
            raise RuntimeError(f"source symlink is forbidden: {relative}")
        if path.is_file():
            files.append(path)
    for path in sorted(files, key=lambda item: item.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix().encode("utf-8")
        digest.update(struct.pack("<I", len(relative)))
        digest.update(relative)
        digest.update(struct.pack("<Q", path.stat().st_size))
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime-root", required=True)
    parser.add_argument("--lightglue-commit", required=True)
    parser.add_argument("--tree-sha256", required=True)
    parser.add_argument("--aliked-sha256", required=True)
    parser.add_argument("--lightglue-sha256", required=True)
    args = parser.parse_args()

    assert platform.system() == "Darwin"
    assert platform.machine() == "arm64"
    assert platform.python_version() == "3.11.11"
    runtime_root = pathlib.Path(args.runtime_root).resolve(strict=True)
    source_root = (runtime_root / "LightGlue").resolve(strict=True)
    checkpoint_root = (runtime_root / "torch/hub/checkpoints").resolve(strict=True)
    aliked_path = (checkpoint_root / "aliked-n16.pth").resolve(strict=True)
    lightglue_path = (
        checkpoint_root / "aliked_lightglue_v0-1_arxiv.pth"
    ).resolve(strict=True)
    identity = json.loads((runtime_root / "identity.json").read_text(encoding="utf-8"))
    assert identity["schema_version"] == 1
    assert identity["lightglue_commit"] == args.lightglue_commit
    assert identity["lightglue_tree_sha256"] == args.tree_sha256
    assert identity["aliked_model_sha256"] == args.aliked_sha256
    assert identity["lightglue_model_sha256"] == args.lightglue_sha256
    assert tree_sha256(source_root) == args.tree_sha256
    assert sha256_file(aliked_path) == args.aliked_sha256
    assert sha256_file(lightglue_path) == args.lightglue_sha256

    sys.dont_write_bytecode = True
    sys.path.insert(0, str(source_root))
    import PIL
    import kornia
    import numpy
    import torch
    import torchvision

    install_unavailable_cv2_stub()
    from lightglue import ALIKED, LightGlue

    assert torch.__version__ == "2.2.2"
    assert torchvision.__version__ == "0.17.2"
    assert numpy.__version__ == "1.26.4"
    assert PIL.__version__ == "12.3.0"
    assert kornia.__version__ == "0.6.12"
    torch.set_num_threads(2)
    torch.set_num_interop_threads(1)
    torch.manual_seed(31)
    torch.hub.set_dir(str(runtime_root / "torch/hub"))

    extractor = ALIKED(
        model_name="aliked-n16",
        max_num_keypoints=64,
        detection_threshold=-1.0,
        nms_radius=2,
    ).eval()
    matcher = LightGlue(
        features="aliked",
        filter_threshold=0.1,
        depth_confidence=0.95,
        width_confidence=0.99,
        flash=False,
        mp=False,
    ).eval()

    axis = torch.arange(128)
    checker = (((axis[:, None] // 8) + (axis[None, :] // 8)) % 2).float()
    image = checker.unsqueeze(0).repeat(3, 1, 1)
    with torch.inference_mode():
        features0 = extractor.extract(image, resize=128)
        features1 = extractor.extract(image, resize=128)
        prediction = matcher({"image0": features0, "image1": features1})
    assert features0["keypoints"].shape == (1, 64, 2)
    assert features0["descriptors"].shape == (1, 64, 128)
    compact_matches = prediction["matches"][0]
    compact_scores = prediction["scores"][0]
    assert compact_matches.ndim == 2
    assert compact_matches.shape[1] == 2
    assert compact_scores.ndim == 1
    assert compact_scores.shape[0] == compact_matches.shape[0]
    assert compact_matches.shape[0] > 0
    print(
        json.dumps(
            {
                "status": "ok",
                "features": int(features0["keypoints"].shape[1]),
                "matches": int(compact_matches.shape[0]),
                "lightglue_commit": args.lightglue_commit,
                "tree_sha256": args.tree_sha256,
                "aliked_sha256": args.aliked_sha256,
                "lightglue_sha256": args.lightglue_sha256,
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
