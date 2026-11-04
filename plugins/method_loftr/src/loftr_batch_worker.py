#!/usr/bin/env python3
"""Batch LoFTR inference with lossless pair caching and collection-wide association."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import time


def resized_shape(width, height, maximum):
    scale = min(1.0, maximum / max(width, height))
    # Round before divisor alignment: int(w * (maximum/w)) can spuriously lose 8 pixels.
    return max(8, int(round(width * scale)) // 8 * 8), max(8, int(round(height * scale)) // 8 * 8)


def original_coordinates(points, scale):
    return (points + .5) * scale - .5


def prepare_image(filename, maximum):
    import cv2
    import numpy as np
    import torch
    from torchvision.transforms.functional import resize
    image = cv2.imread(filename, cv2.IMREAD_GRAYSCALE | cv2.IMREAD_IGNORE_ORIENTATION)
    if image is None or min(image.shape) < 8:
        raise ValueError(f'Cannot read image: {filename}')
    height, width = image.shape
    # Match HLOC dense SfM: float area resize followed by tensor divisor alignment.
    image = image.astype(np.float32)
    ratio = min(1.0, maximum / max(width, height))
    intermediate = (int(round(width * ratio)), int(round(height * ratio)))
    if ratio < 1.0:
        image = cv2.resize(image, intermediate, interpolation=cv2.INTER_AREA)
    rw, rh = resized_shape(width, height, maximum)
    image = resize(torch.from_numpy(image[None] / 255.0), [rh, rw], antialias=True)[None]
    return image, np.array([width / rw, height / rh], np.float32), dict(
        path=filename, original_size=[width,height], intermediate_size=list(intermediate),
        network_size=[rw,rh], original_per_network_pixel=[width/rw,height/rh])


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True)
    parser.add_argument('--output-dir', required=True)
    parser.add_argument('--model-type', choices=('indoor','outdoor'), default='outdoor')
    parser.add_argument('--max-keypoints', type=int, default=0, help='Per-pair output limit; 0 retains all matches')
    parser.add_argument('--max-keypoints-per-image', type=int, default=0)
    parser.add_argument('--keypoint-merge-radius', type=float, default=1.0)
    parser.add_argument('--keypoint-cell-size', type=float, default=0.0)
    parser.add_argument('--confidence-threshold', type=float, default=.2)
    parser.add_argument('--resize-max', type=int, default=1024)
    parser.add_argument('--refine-endpoint', choices=('first', 'second'), default='second',
                        help='Kornia refines its second input; first matches the HLOC wrapper')
    parser.add_argument('--device', choices=('auto','cpu','cuda','mps'), default='auto')
    parser.add_argument('--checkpoint', default='')
    parser.add_argument('--pair-cache-dir', default='')
    parser.add_argument('--rotation-rescue', action='store_true')
    parser.add_argument('--base-pair-cache-dir', default='',
                        help='Optional validated zero-rotation cache for rotation retries')
    parser.add_argument('--pair-geometry', choices=('none','magsac','magsac_seed','magsac_post'), default='none')
    parser.add_argument('--geometry-threshold', type=float, default=1.0)
    parser.add_argument('--cpu-threads', type=int, default=6)
    args = parser.parse_args()
    if not (args.max_keypoints >= 0 and args.max_keypoints_per_image >= 0
            and 0 <= args.confidence_threshold <= 1 and 8 <= args.resize_max <= 4096
            and 0 <= args.keypoint_cell_size <= 32
            and .1 <= args.geometry_threshold <= 4 and .1 <= args.keypoint_merge_radius <= 8 and 1 <= args.cpu_threads <= 64):
        parser.error('Invalid matching parameters')
    if args.pair_cache_dir and not args.checkpoint:
        parser.error('Persistent inference cache requires an explicit pinned checkpoint')
    if args.base_pair_cache_dir and not args.rotation_rescue:
        parser.error('A base cache requires --rotation-rescue')
    if args.rotation_rescue and not args.checkpoint:
        parser.error('Rotation rescue requires an explicit pinned checkpoint')
    started = time.perf_counter()
    import cv2
    import numpy as np
    import torch
    import kornia
    from kornia.feature import LoFTR
    from kornia.feature.loftr.loftr import default_cfg
    from loftr_aggregation import aggregate
    cv2.setNumThreads(1)
    torch.set_num_threads(args.cpu_threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(0)
    available = {'cpu': True, 'cuda': torch.cuda.is_available(), 'mps': torch.backends.mps.is_available()}
    device = args.device
    if device == 'auto':
        device = next(d for d in ('cuda','mps','cpu') if available[d])
    if not available[device]:
        raise RuntimeError(f'Requested device unavailable: {device}')
    lines = Path(args.input).read_text(encoding='utf-8').splitlines()
    count = int(lines[0])
    if not 2 <= count <= 1000 or len(lines) != count+1:
        raise ValueError('Invalid image-list count')
    output = Path(args.output_dir); output.mkdir(parents=True, exist_ok=True)
    cache = Path(args.pair_cache_dir) if args.pair_cache_dir else output
    cache.mkdir(parents=True, exist_ok=True)
    images=[];image_metadata=[]
    for filename in lines[1:]:
        tensor, scale, metadata = prepare_image(filename, args.resize_max)
        metadata['sha256'] = sha256(filename)
        images.append((tensor,scale));image_metadata.append(metadata)
    checkpoint_sha256 = sha256(args.checkpoint) if args.checkpoint else None
    contract = dict(schema='posdk.loftr.inference.v1', preprocessing='hloc_float_area_tensor_df8_half_pixel_v1',
                    device=device, torch=torch.__version__, kornia=kornia.__version__,
                    checkpoint_sha256=checkpoint_sha256, model_type=args.model_type,
                    coarse_confidence_threshold=args.confidence_threshold,
                    refinement=args.refine_endpoint + '_endpoint', images=image_metadata)
    base_contract = copy.deepcopy(contract)
    if args.rotation_rescue:
        from loftr_rotation import POLICY
        base_cache = Path(args.base_pair_cache_dir) if args.base_pair_cache_dir else cache / 'baseline'
        if base_cache.resolve() == cache.resolve():
            raise ValueError('The baseline and rescued caches must be separate directories')
        base_cache.mkdir(parents=True, exist_ok=True)
        base_manifest = base_cache / 'inference.json'
        if base_manifest.exists():
            if json.loads(base_manifest.read_text()) != base_contract:
                raise ValueError('Baseline cache does not match the inference contract')
        else:
            if any(base_cache.glob('pair_*.npz')):
                raise ValueError('Baseline cache has pairs without a valid manifest')
            base_manifest.write_text(json.dumps(base_contract, indent=2)+'\n')
        contract['rotation_search'] = POLICY
        contract['base_manifest_sha256'] = sha256(base_manifest)
    manifest=cache/'inference.json'
    if manifest.exists():
        if json.loads(manifest.read_text()) != contract:
            raise ValueError('Inference cache does not match images/model/preprocessing/device; use another cache directory')
    else:
        if any(cache.glob('pair_*.npz')):
            raise ValueError('Inference cache has pair files without a valid manifest')
        manifest.write_text(json.dumps(contract,indent=2)+'\n')
    total=count*(count-1)//2
    missing=[(i,j) for i in range(count) for j in range(i+1,count) if not (cache/f'pair_{i}_{j}.npz').exists()]
    model=None
    network_passes=0
    inferred_pair_ids=set()
    base_cache_hits=0
    inference_started=time.perf_counter()
    def load_model():
        nonlocal model
        if model is not None:
            return
        config=copy.deepcopy(default_cfg)
        config['match_coarse']['thr']=args.confidence_threshold
        if args.checkpoint:
            weights=torch.load(args.checkpoint,map_location='cpu',weights_only=True)
            model=LoFTR(pretrained=None,config=config)
            model.load_state_dict(weights['state_dict'])
        else:
            from kornia.feature.loftr.loftr import urls
            for key,url in urls.items():urls[key]=url.replace('http://','https://',1)
            model=LoFTR(pretrained=args.model_type,config=config)
        model=model.to(device).eval()
    def predict(i, j, quarter_turns=0):
        nonlocal network_passes
        from loftr_rotation import rotate_image_grid_aligned, unrotate_points
        load_model()
        image_i = images[i][0]
        image_j = rotate_image_grid_aligned(images[j][0], quarter_turns)
        first, second = (image_j, image_i) if args.refine_endpoint == 'first' else (image_i, image_j)
        prediction = model(dict(image0=first.to(device), image1=second.to(device)))
        if args.refine_endpoint == 'first':
            prediction['keypoints0'], prediction['keypoints1'] = (
                prediction['keypoints1'], prediction['keypoints0'])
        confidence = prediction['confidence'].cpu().numpy()
        p0 = prediction['keypoints0'].cpu().numpy()
        p1 = prediction['keypoints1'].cpu().numpy()
        if quarter_turns:
            height, width = images[j][0].shape[-2:]
            p1 = unrotate_points(p1, quarter_turns, width, height)
        p0 = original_coordinates(p0, images[i][1])
        p1 = original_coordinates(p1, images[j][1])
        if not np.isfinite(np.column_stack((p0,p1,confidence))).all():
            raise RuntimeError(f'Non-finite model output in pair {i},{j}')
        network_passes += 1
        inferred_pair_ids.add((i, j))
        return dict(keypoints0=p0, keypoints1=p1, confidence=confidence)
    def write_pair(destination, pair):
        temporary = destination.with_suffix('.tmp')
        with temporary.open('wb') as stream:
            np.savez_compressed(stream, **pair)
        os.replace(temporary, destination)
    if args.rotation_rescue and (missing or not (cache/'rotation_provenance.json').exists()):
        from loftr_rotation import build_rescue_cache
        with torch.inference_mode():
            for i in range(count):
                for j in range(i+1, count):
                    destination = base_cache/f'pair_{i}_{j}.npz'
                    if not destination.exists():
                        write_pair(destination, predict(i,j))
                    else:
                        base_cache_hits += 1
            rescue = build_rescue_cache(base_cache, cache, count, [x[1] for x in images], predict)
        print(json.dumps(dict(event='loftr_rotation_finished', weak_views=rescue['weak_views'],
                              changed_pairs=rescue['changed_pairs'], model_passes=network_passes)), flush=True)
    model_loaded = model is not None
    print(json.dumps(dict(event='loftr_ready',device=device,images=count,pairs=total,cache_hits=total-len(missing),
                          model_loads=int(model_loaded or (bool(missing) and not args.rotation_rescue)),checkpoint_sha256=checkpoint_sha256)),flush=True)
    done=zero=network_matches=0
    with torch.inference_mode():
        for i in range(count):
            for j in range(i+1,count):
                destination=cache/f'pair_{i}_{j}.npz'
                if not destination.exists():
                    write_pair(destination, predict(i,j))
                with np.load(destination,allow_pickle=False) as pair:
                    confidence=pair['confidence'];n=len(confidence)
                    if pair['keypoints0'].shape!=(n,2) or pair['keypoints1'].shape!=(n,2):
                        raise ValueError('Malformed cached pair')
                    if not np.isfinite(confidence).all() or np.any((confidence<0)|(confidence>1)):
                        raise ValueError('Invalid cached confidence')
                done+=1;network_matches+=n;zero+=int(n==0)
                if done==1 or done%max(1,total//100)==0 or done==total:
                    print(json.dumps(dict(event='loftr_progress',completed=done,pairs=total,matches=network_matches,
                                          inference_seconds=time.perf_counter()-inference_started)),flush=True)
    model_loaded = model is not None
    del model
    if device=='mps':torch.mps.empty_cache()
    inference_seconds=time.perf_counter()-inference_started
    association_started=time.perf_counter()
    association=aggregate(cache,count,args.confidence_threshold,args.max_keypoints,args.keypoint_merge_radius,
                          args.max_keypoints_per_image,output_directory=output,
                          pair_geometry=args.pair_geometry,geometry_threshold=args.geometry_threshold,
                          cell_size=args.keypoint_cell_size)
    (output/'image_geometry.json').write_text(json.dumps(image_metadata,indent=2)+'\n')
    print(json.dumps(dict(event='loftr_finished',device=device,model_loads=int(model_loaded),pairs=done,
                          zero_match_pairs=zero,matches=association['associated_matches'],network_matches=network_matches,
                          inferred_pairs=len(inferred_pair_ids),cache_hits=total-len(missing),seconds=time.perf_counter()-started,
                          created_cache_pairs=len(missing),base_cache_hits=base_cache_hits,
                          model_passes=network_passes, rotation_rescue=args.rotation_rescue,
                          inference_seconds=inference_seconds,association_seconds=time.perf_counter()-association_started,
                          torch=torch.__version__,kornia=kornia.__version__,numpy=np.__version__,opencv=cv2.__version__,
                          checkpoint_sha256=checkpoint_sha256)),flush=True)


if __name__=='__main__':main()
