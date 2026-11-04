#!/usr/bin/env python3
"""
LoFTR Detector-Free Feature Matching Script for PoSDK
LoFTR无检测器特征匹配脚本

Uses kornia.feature.LoFTR for end-to-end image pair matching.
Supports MPS (Apple Silicon), CUDA, and CPU backends.
"""

import argparse
import sys
import os
import numpy as np

def get_device(device_str):
    """Select compute device based on availability and user preference."""
    import torch

    if device_str == "auto":
        if torch.cuda.is_available():
            return torch.device("cuda")
        elif hasattr(torch.backends, "mps") and torch.backends.mps.is_available():
            return torch.device("mps")
        else:
            return torch.device("cpu")
    elif device_str == "mps":
        if hasattr(torch.backends, "mps") and torch.backends.mps.is_available():
            return torch.device("mps")
        print("Warning: MPS not available, falling back to CPU")
        return torch.device("cpu")
    elif device_str == "cuda":
        if torch.cuda.is_available():
            return torch.device("cuda")
        print("Warning: CUDA not available, falling back to CPU")
        return torch.device("cpu")
    else:
        return torch.device("cpu")


def load_image_as_tensor(img_path, resize_max, device):
    """Load image and convert to grayscale tensor for LoFTR."""
    import torch
    import cv2

    img = cv2.imread(img_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise ValueError(f"Failed to load image: {img_path}")

    h, w = img.shape[:2]
    scale = min(resize_max / max(h, w), 1.0)
    if scale < 1.0:
        new_w = int(w * scale)
        new_h = int(h * scale)
        img = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_AREA)

    img_tensor = torch.from_numpy(img).float() / 255.0
    img_tensor = img_tensor.unsqueeze(0).unsqueeze(0)  # [1, 1, H, W]
    return img_tensor.to(device), scale


def run_loftr_matching(args):
    """Run LoFTR matching on an image pair."""
    import torch
    import kornia.feature as KF

    device = get_device(args.device)
    print(f"Using device: {device}")

    # Load LoFTR model
    model_type = args.model_type.lower()
    if model_type == "indoor":
        matcher = KF.LoFTR(pretrained="indoor")
    else:
        matcher = KF.LoFTR(pretrained="outdoor")

    # MPS may not support all ops; fall back to CPU if needed
    try:
        matcher = matcher.to(device).eval()
    except Exception as e:
        print(f"Warning: Failed to move model to {device}, falling back to CPU: {e}")
        device = torch.device("cpu")
        matcher = matcher.to(device).eval()

    # Load images
    img0_tensor, scale0 = load_image_as_tensor(args.img1, args.resize_max, device)
    img1_tensor, scale1 = load_image_as_tensor(args.img2, args.resize_max, device)

    # Run LoFTR
    input_dict = {"image0": img0_tensor, "image1": img1_tensor}

    with torch.inference_mode():
        try:
            correspondences = matcher(input_dict)
        except Exception as e:
            # Some LoFTR ops may fail on MPS; retry on CPU
            if device.type != "cpu":
                print(f"Warning: LoFTR failed on {device}, retrying on CPU: {e}")
                device = torch.device("cpu")
                matcher = matcher.to(device)
                img0_tensor = img0_tensor.to(device)
                img1_tensor = img1_tensor.to(device)
                input_dict = {"image0": img0_tensor, "image1": img1_tensor}
                correspondences = matcher(input_dict)
            else:
                raise

    mkpts0 = correspondences["keypoints0"].cpu().numpy()
    mkpts1 = correspondences["keypoints1"].cpu().numpy()
    confidence = correspondences["confidence"].cpu().numpy()

    # Scale keypoints back to original image coordinates
    if scale0 < 1.0:
        mkpts0 = mkpts0 / scale0
    if scale1 < 1.0:
        mkpts1 = mkpts1 / scale1

    # Filter by confidence threshold
    mask = confidence >= args.confidence_threshold
    mkpts0 = mkpts0[mask]
    mkpts1 = mkpts1[mask]
    confidence = confidence[mask]

    # Limit max keypoints
    if args.max_keypoints > 0 and len(mkpts0) > args.max_keypoints:
        top_indices = np.argsort(-confidence)[:args.max_keypoints]
        mkpts0 = mkpts0[top_indices]
        mkpts1 = mkpts1[top_indices]
        confidence = confidence[top_indices]

    num_matches = len(mkpts0)
    print(f"LoFTR found {num_matches} matches (after filtering)")

    # Write output file
    # Format: first line = num_matches, then each line: x0 y0 x1 y1 confidence
    with open(args.output, "w") as f:
        f.write(f"{num_matches}\n")
        for i in range(num_matches):
            f.write(f"{mkpts0[i, 0]:.6f} {mkpts0[i, 1]:.6f} "
                    f"{mkpts1[i, 0]:.6f} {mkpts1[i, 1]:.6f} "
                    f"{confidence[i]:.6f}\n")

    print(f"Results written to: {args.output}")
    return 0


def main():
    parser = argparse.ArgumentParser(description="LoFTR Feature Matching for PoSDK")
    parser.add_argument("--img1", required=True, help="Path to first image")
    parser.add_argument("--img2", required=True, help="Path to second image")
    parser.add_argument("--output", required=True, help="Output file path")
    parser.add_argument("--model_type", default="outdoor",
                        choices=["indoor", "outdoor"],
                        help="LoFTR pretrained model type")
    parser.add_argument("--max_keypoints", type=int, default=2048,
                        help="Maximum number of matches to keep")
    parser.add_argument("--confidence_threshold", type=float, default=0.2,
                        help="Minimum confidence threshold for matches")
    parser.add_argument("--resize_max", type=int, default=840,
                        help="Maximum image dimension (longer side)")
    parser.add_argument("--device", default="auto",
                        choices=["auto", "mps", "cuda", "cpu"],
                        help="Compute device")

    args = parser.parse_args()

    if not os.path.exists(args.img1):
        print(f"Error: Image not found: {args.img1}")
        return 1
    if not os.path.exists(args.img2):
        print(f"Error: Image not found: {args.img2}")
        return 1

    try:
        return run_loftr_matching(args)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
