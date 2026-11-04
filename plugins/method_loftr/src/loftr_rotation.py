"""Bounded quarter-turn retries for weak LoFTR views. Selection never uses poses/GT."""
from pathlib import Path
import json
import os
import numpy as np

POLICY = 'weak_view_quarter_turn_grid_aligned_v1'


def unrotate_points(points, quarter_turns, width, height):
    """Invert the grid-aligned transform in network pixel coordinates."""
    x, y = points[:, 0], points[:, 1]
    if quarter_turns == 0:
        return points.copy()
    if quarter_turns == 1:
        return np.column_stack((width - y, x))
    if quarter_turns == 2:
        return np.column_stack((width - x, height - y))
    if quarter_turns == 3:
        return np.column_stack((y, height - x))
    raise ValueError('Expected 0, 1, 2, or 3 quarter turns')


def rotate_image_grid_aligned(image, quarter_turns):
    """Keep LoFTR's stride-8 coarse lattice in the same source-image phase.

    A plain rot90 maps coarse coordinates 0 mod 8 to 7 mod 8. A one-pixel
    zero-padded shift maps them back to 0 mod 8, without cyclic wraparound.
    """
    import torch
    if quarter_turns == 0:
        return image
    rotated = torch.rot90(image, quarter_turns, (-2, -1)).contiguous()
    out = torch.zeros_like(rotated)
    if quarter_turns == 1:
        out[..., 1:, :] = rotated[..., :-1, :]
    elif quarter_turns == 2:
        out[..., 1:, 1:] = rotated[..., :-1, :-1]
    elif quarter_turns == 3:
        out[..., :, 1:] = rotated[..., :, :-1]
    else:
        raise ValueError('Expected 0, 1, 2, or 3 quarter turns')
    return out


def pair_support(pair, scales, seed):
    import cv2
    score = pair['confidence']
    count = len(score)
    empty = dict(matches=count, inliers=0, ratio=0., weight=0., high_confidence=0)
    if count < 15:
        return empty
    p0 = (pair['keypoints0'] + .5) / scales[0] - .5
    p1 = (pair['keypoints1'] + .5) / scales[1] - .5
    cv2.setRNGSeed(seed)
    F, mask = cv2.findFundamentalMat(p0.astype(np.float64), p1.astype(np.float64),
        cv2.USAC_MAGSAC, ransacReprojThreshold=1., confidence=.999, maxIters=10000)
    if F is None or F.shape != (3, 3) or mask is None or not np.isfinite(F).all():
        return empty
    mask = mask.ravel().astype(bool)
    if len(mask) != count:
        return empty
    return dict(matches=count, inliers=int(mask.sum()), ratio=float(mask.mean()),
                weight=float(score[mask].sum(dtype=np.float64)),
                high_confidence=int(np.count_nonzero((score >= .8) & mask)))


def reliable(support):
    return support['inliers'] >= 128 and support['ratio'] >= .5 and support['high_confidence'] >= 16


def prefer_retry(candidate, incumbent):
    return (candidate['inliers'] >= 64 and candidate['ratio'] >= .35
            and candidate['inliers'] >= incumbent['inliers'] + 32
            and candidate['weight'] > 1.5 * incumbent['weight'])


def build_rescue_cache(base_directory, output_directory, image_count, scales, predict):
    """Copy every baseline pair; retry only weak-view edges, preserving losers.

    predict(i, j, k) returns original-frame points with image j rotated by k
    grid-aligned quarter turns. The caller validates the complete base manifest.
    """
    base = Path(base_directory)
    output = Path(output_directory)
    output.mkdir(parents=True, exist_ok=True)
    supports = {}
    degree = np.zeros(image_count, dtype=np.int32)
    for i in range(image_count):
        for j in range(i + 1, image_count):
            with np.load(base / f'pair_{i}_{j}.npz', allow_pickle=False) as pair:
                support = pair_support(pair, (scales[i], scales[j]), 31 + i*1009 + j)
            supports[i, j] = support
            if reliable(support):
                degree[i] += 1
                degree[j] += 1
    weak = set(np.flatnonzero(degree < 2).tolist())
    rows = []
    passes = changed = 0
    for i in range(image_count):
        for j in range(i + 1, image_count):
            destination = output / f'pair_{i}_{j}.npz'
            with np.load(base / destination.name, allow_pickle=False) as original:
                winner = {key: original[key].copy() for key in ('keypoints0', 'keypoints1', 'confidence')}
            support = supports[i, j]
            angle = 0
            attempts = []
            # Reliable edges are retained byte for byte. Weak edges retain their
            # baseline hypothesis unless a larger verified consensus is found.
            if (i in weak or j in weak) and not reliable(support):
                for k in (1, 2, 3):
                    candidate = predict(i, j, k)
                    passes += 1
                    measured = pair_support(candidate, (scales[i], scales[j]), 31+i*1009+j)
                    attempts.append(dict(quarter_turns=k, **measured))
                    if prefer_retry(measured, support):
                        winner, support, angle = candidate, measured, k
            if angle:
                temporary = destination.with_suffix('.tmp')
                with temporary.open('wb') as stream:
                    np.savez_compressed(stream, **winner)
                os.replace(temporary, destination)
                changed += 1
            else:
                # No source mutation and no duplicate compressed payload.
                temporary = destination.with_suffix('.tmp')
                if temporary.exists():
                    temporary.unlink()
                try:
                    os.link(base / destination.name, temporary)
                except OSError:
                    import shutil
                    shutil.copyfile(base / destination.name, temporary)
                os.replace(temporary, destination)
            rows.append(dict(pair=[i, j],selected_quarter_turns=angle, baseline=supports[i,j],selected=support,attempts=attempts))
            if attempts:
                print(json.dumps(dict(event='loftr_rotation_retry', pair=[i,j], selected_quarter_turns=angle,
                                      baseline_inliers=supports[i,j]['inliers'], selected_inliers=support['inliers'])), flush=True)
    result = dict(policy=POLICY, weak_views=sorted(weak), reliable_degree=degree.tolist(),
                  model_passes=passes, changed_pairs=changed, pairs=rows)
    (output / 'rotation_provenance.json').write_text(json.dumps(result, indent=2)+'\n')
    return result
