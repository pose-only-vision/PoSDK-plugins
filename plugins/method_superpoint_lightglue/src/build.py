"""Build the published source snapshot against a matching public PoSDK Plugin SDK.

Only the build-directory staging copy is adjusted for the public repository's
src/ + deps/ layout. Original runtime source files remain unchanged.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--configure-only', action='store_true')
    parser.add_argument('--parallel', type=int, default=2)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent
    plugin = source.parent
    sdk = args.sdk_root.expanduser().resolve()
    build = args.build_dir.expanduser().resolve()
    if build == plugin or build.is_relative_to(source):
        parser.error('Choose a separate build directory outside src/.')
    config = sdk/'plugins/cmake/plugin-config.cmake'
    if not config.is_file():
        parser.error('sdk-root must contain plugins/cmake/plugin-config.cmake')
    manifest = json.loads((source/'source-manifest.json').read_text())
    stage = build/'staging'/manifest['plugin_id']
    stage.mkdir(parents=True, exist_ok=True)
    for item in manifest['files']:
        relative = Path(item['path'])
        if relative.is_absolute() or '..' in relative.parts:
            raise RuntimeError('Invalid source manifest path')
        original = source/relative
        if original.is_symlink():
            raise RuntimeError('Source symlinks are not accepted')
        if hashlib.sha256(original.read_bytes()).hexdigest() != item['sha256']:
            raise RuntimeError('Source checksum mismatch: '+str(relative))
        if relative.name in {'README.md', 'build.py'}:
            continue
        destination = stage/relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, destination)
    for name in ['manifest.json','LICENSE','LICENSE.md','README.md','THIRD_PARTY_NOTICES.md']:
        if (plugin/name).is_file():
            shutil.copy2(plugin/name, stage/name)
    # Dependency recipes already live in the published sibling deps/ directory.
    if (plugin/'deps').is_dir():
        shutil.copytree(plugin/'deps', stage/'deps', dirs_exist_ok=True)
    if (source/'licenses/runtime').is_dir():
        shutil.copytree(source/'licenses/runtime', stage/'deps/licenses', dirs_exist_ok=True)
    license_aliases = {'LIGHTGLUE-LICENSE.txt':'LightGlue-Apache-2.0.txt',
                       'SUPERPOINT-LICENSE.txt':'SuperPoint-LICENSE.txt',
                       'ALIKED-LICENSE.txt':'ALIKED-BSD-3-Clause.txt'}
    for target, original in license_aliases.items():
        if (source/'licenses'/original).is_file():
            (stage/'deps').mkdir(exist_ok=True)
            shutil.copy2(source/'licenses'/original, stage/'deps'/target)
    helper = source/'plugin_runtime_path.hpp'
    if helper.exists():
        shutil.copy2(helper, stage.parent/helper.name)
    if (stage/'romav2_aggregation.py').exists():
        shutil.copy2(stage/'romav2_aggregation.py', stage/'deps/dense_aggregation.py')
    cmake = (stage/'CMakeLists.txt').read_text()
    cmake = cmake.replace('include(../../cmake/plugin-config.cmake)',
                          'include("${POSDK_PLUGIN_SDK_ROOT}/plugins/cmake/plugin-config.cmake")')
    if not re.search(r'\bproject\s*\(', cmake, re.I):
        cmake = re.sub(r'(cmake_minimum_required\([^\n]+\))',
                       r'\1\nproject(published_posdk_plugin LANGUAGES CXX)', cmake, count=1)
    (stage/'CMakeLists.txt').write_text(cmake)
    native = build/'native'
    command = ['cmake','-S',str(stage),'-B',str(native),
               '-DPOSDK_PLUGIN_SDK_ROOT='+str(sdk),'-DCMAKE_BUILD_TYPE=Release',
               '-DCMAKE_CXX_STANDARD=17','-DPOSDK_BUILD_PLUGIN_TESTS=OFF',
               '-DBUILD_TESTS=OFF','-DBUILD_TESTING=OFF']
    subprocess.run(command, check=True)
    if not args.configure_only:
        subprocess.run(['cmake','--build',str(native),'--target',manifest['plugin_id'],
                        '--parallel',str(max(1,args.parallel))], check=True)
    print('Source build output: '+str(native))


if __name__ == '__main__':
    main()
