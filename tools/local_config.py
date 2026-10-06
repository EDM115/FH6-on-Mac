"""Shared local paths. Importing this module does not start Wine or read accounts."""
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / '.local'
METAL_SHA256 = 'f5b56df1b8fe8b364dd9530651a3769c8aed948bd343be3b4510604d503e2bad'

def load_config():
    if not __debug__:
        raise SystemExit('Python -O disables the experimental build/object guards. Run without it.')
    config = Path(os.environ.get('FH6_CONFIG', STATE / 'config.json')).expanduser().resolve()
    if not config.is_file():
        raise SystemExit('Create .local/config.json from config.example.json; see docs/setup.md.')
    values = json.loads(config.read_text())
    result = {'state': STATE, 'repo': ROOT}
    for name in ('runtime', 'bottle', 'loader'):
        path = Path(values[name]).expanduser()
        if not path.is_absolute():
            raise SystemExit(f'{name} must be an absolute local path')
        result[name] = path.resolve()
        if not result[name].exists():
            raise SystemExit(f'Missing {name}: {result[name]}')
    result['probe_runtime'] = Path(values.get('probe_runtime', result['runtime'])).expanduser().resolve()
    result['resources'] = result['runtime'] / 'lib64/apple_gptk/external/D3DMetal.framework/Versions/A/Resources'
    result['metal'] = result['resources'].parent / 'D3DMetal'
    result['steam_exe'] = values.get('steam_exe', r'C:\Program Files (x86)\Steam\steam.exe')
    STATE.mkdir(mode=0o700, exist_ok=True)
    return result
