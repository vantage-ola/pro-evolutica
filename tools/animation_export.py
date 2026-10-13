"""Export a validated rigid model/dense clip to the local native viewer format."""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

if __package__:
    from .anm_inspect import inspect_anm
    from .anm_pose import evaluate_pose, influences
    from .ktmdl_inspect import inspect_skeleton, read_model
    from .ktmdl_export import strip_triangles
else:
    from anm_inspect import inspect_anm
    from anm_pose import evaluate_pose, influences
    from ktmdl_inspect import inspect_skeleton, read_model
    from ktmdl_export import strip_triangles

MAGIC = b'PEVA0001'
MAX_BYTES = 32*1024*1024
MAX_VERTEX_SAMPLES = 2_000_000


def export_animation(model, clip, *, core_offset=0, sequence_offset=None, weight_order):
    metadata = inspect_skeleton(model)
    animation = inspect_anm(clip, core_offset=core_offset, sequence_offset=sequence_offset)
    bones = metadata['skeletal_metadata']['bones']
    count, fps = animation['sample_count'], animation['fps']
    if type(fps) is not int or not 1 <= fps <= 240:
        raise ValueError('playback requires a sequence with an integer FPS from 1 to 240')
    vertices = metadata['vertex_count']
    if not 1 <= vertices <= 65535 or not 1 <= count <= 512 or vertices*count > MAX_VERTEX_SAMPLES:
        raise ValueError('native animation geometry/sample limit exceeded')
    # The first evaluation validates matching hierarchies, complete rotations,
    # rigid inverse binds, influences and absence of unsupported morphs.
    first, _ = evaluate_pose(model, clip, core_offset=core_offset,
        sequence_offset=sequence_offset, weight_order=weight_order)
    rest, _ = evaluate_pose(model, weight_order=weight_order)
    low = [min(first['bounds_min'][c], rest['bounds_min'][c]) for c in range(3)]
    high = [max(first['bounds_max'][c], rest['bounds_max'][c]) for c in range(3)]
    for sample in range(1, count):
        report, _ = evaluate_pose(model, clip, sample=sample, core_offset=core_offset,
            sequence_offset=sequence_offset, weight_order=weight_order)
        low = [min(low[c], report['bounds_min'][c]) for c in range(3)]
        high = [max(high[c], report['bounds_max'][c]) for c in range(3)]
    centre = [(a+b)*.5 for a,b in zip(low,high)]
    if any(not math.isfinite(x) or abs(x)>1e6 for x in centre):
        raise ValueError('animation centre is outside native float bounds')
    if any(abs(x)>1e6 for bone in bones for key in ('matrix_a','matrix_b') for x in bone[key]):
        raise ValueError('bind matrix is outside native float bounds')
    # Bounding-box half diagonal encloses every sample; margin covers f32 storage.
    radius = math.dist(low,high)*.5*1.00001
    if not math.isfinite(radius) or not 1e-6 <= radius <= 1e6:
        raise ValueError('animation framing radius is outside supported bounds')
    bone_data = bytearray()
    for bone in bones:
        bone_data += struct.pack('<i32f',bone['parent'],*bone['matrix_a'],*bone['matrix_b'])
    vertex_data, indices = bytearray(), []
    base = 0
    for packet, skin in zip(metadata['packets'], metadata['skeletal_metadata']['packets'], strict=True):
        position = next(a for a in packet['attributes'] if a['semantic'] == 16)
        for i, raw in enumerate(skin['raw_influences']):
            xyz = struct.unpack_from('<3f',model,packet['vertex_data_offset']+i*packet['vertex_stride']+position['offset'])
            if any(abs(x)>1e6 for x in xyz):
                raise ValueError('vertex is outside native float bounds')
            binding = influences(raw,skin['palette'],weight_order)
            nodes = [b for b,_ in binding]+[0]*(4-len(binding))
            weights = [w for _,w in binding]+[0.]*(4-len(binding))
            vertex_data += struct.pack('<3f4B4d',*xyz,*nodes,*weights)
        strip = struct.unpack_from(f'<{packet["index_count"]}H',model,packet['index_data_offset'])
        indices.extend(base+i for face in strip_triangles(strip) for i in face)
        base += packet['vertex_count']
    if not indices or len(indices) > 600000:
        raise ValueError('native animation triangle limit exceeded')
    rotations = {t['node']:t for t in animation['tracks'] if t['curve_type'] == 0x1c}
    translations = {t['node']:t for t in animation['tracks'] if t['curve_type'] == 0x1d}
    samples = bytearray()
    for sample in range(count):
        for node in range(len(bones)):
            offset = rotations[node]['value_offset']+sample*6
            samples += clip[offset:offset+6]
            if node in translations:
                offset = translations[node]['value_offset']+sample*12
                if any(abs(x)>1e6 for x in struct.unpack_from('<3f',clip,offset)):
                    raise ValueError('translation is outside native float bounds')
                samples += b'\1'+clip[offset:offset+12]
            else:
                samples += bytes(13) # Preserve local rest translation in the runtime.
    output = (struct.pack('<8s5I4f',MAGIC,len(bones),vertices,len(indices),count,fps,*centre,radius)
              +bone_data+vertex_data+struct.pack(f'<{len(indices)}H',*indices)+samples)
    if len(output) > MAX_BYTES:
        raise ValueError('native animation file limit exceeded')
    report = {'schema_version':1,'model_sha256':metadata['sha256'],'animation_sha256':animation['sha256'],
              'package_sha256':hashlib.sha256(output).hexdigest(),'size_bytes':len(output),
              'bone_count':len(bones),'vertex_count':vertices,'triangle_count':len(indices)//3,
              'sample_count':count,'fps':fps,'duration_seconds':(count-1)/fps,
              'weight_order':weight_order,'centre':centre,'radius':radius,
              'limitations':'Dense integer samples, final sample held; geometry-only lighting. No interpolation, looping, events or game materials.'}
    return output, report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model',required=True,type=Path)
    parser.add_argument('--animation',required=True,type=Path)
    parser.add_argument('--core-offset',type=int,default=0)
    parser.add_argument('--sequence-offset',required=True,type=int)
    parser.add_argument('--weight-order',required=True,choices=('first','last'))
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--json',type=Path)
    args = parser.parse_args(argv)
    try:
        paths = [p for p in (args.output,args.json) if p is not None]
        if len({p.resolve() for p in paths}) != len(paths) or any(p.exists() or p.is_symlink() for p in paths):
            raise ValueError('outputs must be distinct new files')
        output,report = export_animation(read_model(args.model),read_model(args.animation),
            core_offset=args.core_offset,sequence_offset=args.sequence_offset,weight_order=args.weight_order)
        text = json.dumps(report,indent=2,allow_nan=False)+'\n'
        with args.output.open('xb') as f:
            f.write(output)
        if args.json:
            with args.json.open('x',encoding='utf-8') as f:
                f.write(text)
        else:
            print(text,end='')
        return 0
    except (OSError,ValueError) as error:
        print(f'animation_export: {error}',file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())
