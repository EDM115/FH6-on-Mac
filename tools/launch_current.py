"""Launch the exact October 25 quiet online stack, without the legacy timing-selector write"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

import launch
from local_config import load_config
from prepare_wine_patches import check_files, read_series
from build_wine_components import TARGETS


def local_build_profile(cfg, recorded):
    build = cfg['state'] / 'wine-build'
    receipt = json.loads((build / 'components.json').read_text())
    expected_sources = read_series()['final_files']
    if receipt.get('source_files') != expected_sources:
        raise SystemExit('Local build receipt does not match the recorded patch sequence')
    check_files(Path(receipt['source_root']), expected_sources)
    if set(receipt['components']) != set(TARGETS.values()):
        raise SystemExit('Local build receipt has a different component set')
    components = dict(recorded['components'])
    for source, destination in TARGETS.items():
        artifact = receipt['components'][destination]
        if artifact['build_file'] != source or hashlib.sha256((build / source).read_bytes()).hexdigest() != artifact['sha256']:
            raise SystemExit('Local build artifact differs from its receipt: ' + destination)
        components[destination] = artifact['sha256']
    shim = cfg['state'] / 'build/tsshim'
    receipt = json.loads((shim / 'build.json').read_text())
    for name in ('d3d12.c', 'd3d12.def'):
        path = cfg['repo'] / 'third_party/highball-tsshim' / name
        if receipt['source_sha256'].get(name) != hashlib.sha256(path.read_bytes()).hexdigest():
            raise SystemExit('Timestamp/layout source changed after the recorded build')
    if hashlib.sha256((shim / 'd3d12.dll').read_bytes()).hexdigest() != receipt['dll_sha256']:
        raise SystemExit('Timestamp/layout binary differs from its build receipt')
    components['lib64/apple_gptk/wine/x86_64-windows/d3d12.dll'] = receipt['dll_sha256']
    return {**recorded, 'components': components}


def verify_current(cfg, recorded):
    for relative, expected in recorded['components'].items():
        path = cfg['runtime'] / relative
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise SystemExit('Component differs from the tested October 25 build: ' + relative)
    steam = cfg['bottle'] / 'drive_c/Program Files (x86)/Steam'
    manifest = (steam / 'steamapps/appmanifest_2483190.acf').read_text()
    build = recorded['game']['steam_build']
    if not re.search(r'"buildid"\s+"' + re.escape(build) + r'"', manifest):
        raise SystemExit('The game build changed, reassess the compatibility guards before launching')
    if not re.search(r'"StateFlags"\s+"4"', manifest):
        raise SystemExit('Steam has not marked the game installation ready')
    game = steam / 'steamapps/common/ForzaHorizon6/forzahorizon6.exe'
    with game.open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != recorded['game']['sha256']:
            raise SystemExit('The game executable differs from the inspected 461.691 build')


def current_arguments(cfg, excluded=()):
    backing = 'Z:' + str(cfg['runtime'] / 'lib64/apple_gptk/wine/x86_64-windows/apd12.dll').replace('/', '\\')
    variables = {
        'HB_D3D12_REAL': backing,
        'HB_TSSHIM_DEPTHRESOLVE': '1',
        'FH6SHIM_DIAG': '1',
        'HB_TSSHIM_APPEND_OFFSETS': '1',
        'WINEDLLPATH_PREPEND': str(cfg['runtime'] / 'lib64/apple_gptk/wine'),
        'WINEMSYNC': '1',
        'FH6_HTTP_DIAG': '0',
        'FH6_DUALSTACK_TTL255': '1',
    }
    arguments = ['--direct', '--steam-silent', '--tag', 'quiet-online-2026-10-25']
    for name, value in variables.items():
        if name not in excluded:
            arguments += ['--env', name + '=' + value]
    return arguments


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--rebuilt-components', action='store_true')
    parser.add_argument('--unset', action='append', default=[])
    own, remaining = parser.parse_known_args()
    unsets = [argument for name in own.unset for argument in ('--unset', name)]
    sys.argv = [sys.argv[0], *unsets, *remaining]
    if any(arg in ('-h', '--help') for arg in sys.argv[1:]):
        print('Current profile option: --rebuilt-components checks local source/build receipts for an experimental rebuild')
        launch.main()
        return
    cfg = load_config()
    recorded = json.loads((Path(__file__).resolve().parents[1] / 'provenance/tested-runtime-2026-10-25.json').read_text())
    if own.rebuilt_components:
        recorded = local_build_profile(cfg, recorded)
        print('Checking a local rebuild candidate, gameplay validation is still required')
    verify_current(cfg, recorded)
    sys.argv = [sys.argv[0], *current_arguments(cfg, excluded=own.unset), *sys.argv[1:]]
    launch.main()


if __name__ == '__main__':
    main()
