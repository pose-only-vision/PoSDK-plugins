"""Bound the installed LoFTR identity and exercise actual model inference."""
import argparse
import hashlib
from pathlib import Path
import platform

parser = argparse.ArgumentParser()
parser.add_argument('--weights', required=True)
parser.add_argument('--sha256', required=True)
args = parser.parse_args()
assert platform.machine() == 'arm64' and platform.python_version() == '3.11.11'
import cv2
import kornia
import numpy as np
import torch
assert torch.__version__ == '2.2.2' and kornia.__version__ == '0.7.4'
assert cv2.__version__ == '4.10.0' and np.__version__ == '1.26.4'
weights = Path(args.weights)
assert hashlib.sha256(weights.read_bytes()).hexdigest() == args.sha256
torch.set_num_threads(2)
torch.manual_seed(31)
from kornia.feature import LoFTR
model = LoFTR(pretrained=None).eval()
model.load_state_dict(torch.load(weights, map_location='cpu', weights_only=True)['state_dict'])
image = torch.rand(1, 1, 128, 160)
with torch.inference_mode():
    result = model({'image0': image, 'image1': torch.roll(image, 8, dims=3)})
assert len(result['confidence']) >= 8, 'real LoFTR inference produced insufficient test matches'
assert all(torch.isfinite(result[k]).all() for k in ['keypoints0', 'keypoints1', 'confidence'])
print('real LoFTR CPU inference passed:', len(result['confidence']), 'matches')
