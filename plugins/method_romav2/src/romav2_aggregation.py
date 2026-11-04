"""Collection-wide, deterministic aggregation of pair-conditioned LoFTR observations.

Uses confidence accumulation in pixel cells followed by mutual-best assignment,
following the dense-SfM construction described by HLOC. No transitive merging.
"""
from pathlib import Path
import json
import numpy as np

_OFFSET = 1 << 20
_BASE = 1 << 21


def bin_keys(points, cell):
    bins = np.rint((points.astype(np.float64) + .5) / cell).astype(np.int64)
    if np.any(np.abs(bins) >= _OFFSET):
        raise ValueError('Keypoint outside supported coordinate range')
    return (bins[:, 0] + _OFFSET) * _BASE + bins[:, 1] + _OFFSET


def bin_points(keys, cell):
    return (np.column_stack((keys // _BASE - _OFFSET, keys % _BASE - _OFFSET)) * cell - .5).astype(np.float32)


def selected_rows(confidence, threshold, maximum):
    indices = np.flatnonzero(np.isfinite(confidence) & (confidence >= threshold))
    if maximum and len(indices) > maximum:
        indices = indices[np.argsort(-confidence[indices], kind='stable')[:maximum]]
    return indices


def mutual_best(ids0, ids1, scores):
    valid = np.flatnonzero((ids0 >= 0) & (ids1 >= 0))
    if not len(valid):
        return valid
    order = valid[np.argsort(-scores[valid], kind='stable')]
    # Keep a match only if it is the best at BOTH endpoints.
    first0 = np.unique(ids0[order], return_index=True)[1]
    first1 = np.unique(ids1[order], return_index=True)[1]
    return order[np.intersect1d(first0, first1)]


def assign(points, keys, representatives, radius, cell=None):
    result = np.full(len(points), -1, dtype=np.int64)
    if not len(keys) or not len(points):
        return result
    query = bin_keys(points, radius if cell is None else cell)
    best = np.full(len(points), radius * radius, dtype=np.float64)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            candidate = query + dx * _BASE + dy
            at = np.searchsorted(keys, candidate)
            safe = np.minimum(at, len(keys) - 1)
            found = (at < len(keys)) & (keys[safe] == candidate)
            distance = np.sum((points.astype(np.float64) - representatives[safe]) ** 2, axis=1)
            better = found & ((distance < best) | ((distance == best) & ((result < 0) | (at < result))))
            result[better] = at[better]
            best[better] = distance[better]
    return result


def geometry_mask(pair, scales, mode, threshold, seed):
    import cv2
    n = len(pair['confidence'])
    if mode == 'none':
        return np.ones(n, dtype=bool)
    if n < 15:
        return np.zeros(n, dtype=bool)
    p0 = (pair['keypoints0'] + .5) / scales[0] - .5
    p1 = (pair['keypoints1'] + .5) / scales[1] - .5
    cv2.setRNGSeed(seed)
    matrix, mask = cv2.findFundamentalMat(p0.astype(np.float64), p1.astype(np.float64),
        cv2.USAC_MAGSAC, ransacReprojThreshold=threshold, confidence=.999, maxIters=10000)
    if matrix is None or matrix.shape != (3,3) or mask is None or not np.isfinite(matrix).all():
        return np.zeros(n, dtype=bool)
    mask = mask.ravel().astype(bool)
    if len(mask) != n or mask.sum() < max(15, .15*n):
        return np.zeros(n, dtype=bool)
    return mask


def aggregate(directory, image_count, threshold, maximum=0, radius=1.0, maximum_per_image=0,
              output_directory=None, pair_geometry='none', geometry_threshold=1.0,
              cell_size=0.0):
    directory = Path(directory)
    output = Path(output_directory) if output_directory is not None else directory
    output.mkdir(parents=True, exist_ok=True)
    chunks = [[] for _ in range(image_count)]
    fine_chunks = [[] for _ in range(image_count)]
    weight_chunks = [[] for _ in range(image_count)]
    cell_size = max(radius, cell_size)
    raw_count = selected_count = capped_pairs = geometric_count = 0
    if pair_geometry != 'none':
        metadata = json.loads((directory / 'inference.json').read_text())['images']
        scales = [np.array(x['original_per_network_pixel'], np.float32) for x in metadata]
    else:
        scales = [np.ones(2, np.float32) for _ in range(image_count)]
    masks = {}
    for i in range(image_count):
        for j in range(i + 1, image_count):
            with np.load(directory / f'pair_{i}_{j}.npz', allow_pickle=False) as pair:
                score = pair['confidence']
                mask = geometry_mask(pair, (scales[i],scales[j]), pair_geometry, geometry_threshold, 31+i*1009+j)
                masks[(i,j)] = mask
                geometric_count += int(mask.sum())
                eligible = np.arange(len(score)) if pair_geometry in ('magsac_seed', 'magsac_post') else np.flatnonzero(mask)
                chosen = eligible[selected_rows(score[eligible], threshold, maximum)]
                raw_count += len(score)
                selected_count += len(chosen)
                capped_pairs += int(bool(maximum) and np.count_nonzero(score >= threshold) > maximum)
                for view, name in ((i, 'keypoints0'), (j, 'keypoints1')):
                    chunks[view].append(bin_keys(pair[name][chosen], cell_size))
                    fine_chunks[view].append(bin_keys(pair[name][chosen], radius))
                    weight_chunks[view].append(score[chosen].astype(np.float64))
    feature_keys = []
    representatives = []
    for view in range(image_count):
        observations = np.concatenate(chunks[view]) if chunks[view] else np.empty(0, np.int64)
        weights = np.concatenate(weight_chunks[view]) if weight_chunks[view] else np.empty(0)
        fine = np.concatenate(fine_chunks[view]) if fine_chunks[view] else np.empty(0, np.int64)
        keys, inverse = np.unique(observations, return_inverse=True)
        votes = np.bincount(inverse, weights=weights, minlength=len(keys))
        if len(keys):
            # Each coarse cell selects its most-supported fine location. This
            # permits stable identity without averaging distant observations.
            pairs, pair_inverse = np.unique(np.column_stack((observations, fine)), axis=0, return_inverse=True)
            support = np.bincount(pair_inverse, weights=weights, minlength=len(pairs))
            order = np.lexsort((pairs[:, 1], -support, pairs[:, 0]))
            first = np.unique(pairs[order, 0], return_index=True)[1]
            representative_keys = pairs[order[first], 1]
        else:
            representative_keys = keys
        if maximum_per_image and len(keys) > maximum_per_image:
            keep = np.argsort(-votes, kind='stable')[:maximum_per_image]
            keep.sort()
            keys = keys[keep]
            representative_keys = representative_keys[keep]
        feature_keys.append(keys)
        representatives.append(bin_points(representative_keys, radius))
        chunks[view].clear()
        fine_chunks[view].clear()
        weight_chunks[view].clear()
    pairs = matches = 0
    for i in range(image_count):
        for j in range(i + 1, image_count):
            with np.load(directory / f'pair_{i}_{j}.npz', allow_pickle=False) as pair:
                eligible = np.arange(len(pair['confidence'])) if pair_geometry in ('magsac_seed', 'magsac_post') else np.flatnonzero(masks[(i,j)])
                chosen = eligible[selected_rows(pair['confidence'][eligible], threshold, maximum)]
                scores = pair['confidence'][chosen]
                ids0 = assign(pair['keypoints0'][chosen], feature_keys[i], representatives[i], radius, cell_size)
                ids1 = assign(pair['keypoints1'][chosen], feature_keys[j], representatives[j], radius, cell_size)
                keep = mutual_best(ids0, ids1, scores)
                flags = masks[(i,j)][chosen][keep] if pair_geometry != 'none' else np.zeros(len(keep), bool)
                if pair_geometry == 'magsac_post':
                    keep = keep[flags]
                    flags = np.ones(len(keep), bool)
            destination = output / f'pair_{i}_{j}.txt'
            with destination.open('w', encoding='utf-8') as stream:
                stream.write(f'{len(keep)}\n')
                np.savetxt(stream, np.column_stack((ids0[keep], ids1[keep], scores[keep], flags)), fmt=['%d', '%d', '%.8g', '%d'])
            matches += len(keep)
            pairs += int(len(keep) > 0)
    for view, points in enumerate(representatives):
        with (output / f'features_{view}.txt').open('w', encoding='utf-8') as stream:
            stream.write(f'{len(points)}\n')
            np.savetxt(stream, points, fmt='%.8g')
    summary = dict(protocol='loftr_associations_v1', image_count=image_count,
                   network_matches=raw_count, geometrically_verified_matches=geometric_count,
                   pair_geometry=pair_geometry, geometry_threshold_network_px=geometry_threshold, selected_matches=selected_count,
                   cap_hit_pairs=capped_pairs, associated_matches=matches,
                   associated_pairs=pairs, features=sum(map(len, representatives)),
                   features_per_image=list(map(len, representatives)),
                   maximum_per_pair=maximum, maximum_per_image=maximum_per_image,
                   merge_radius_original_px=radius, cell_size_original_px=cell_size,
                   assignment='global_pixel_bins_confidence_mode_mutual_best')
    (output / 'association.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(dict(event='loftr_association_finished', **summary)), flush=True)
    return summary
