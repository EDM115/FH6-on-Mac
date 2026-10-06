"""Fetch two pinned Microsoft headers into ignored .local/third-party/."""
import hashlib
import urllib.request
from local_config import STATE

COMMIT = '4781bc2115d6e64f8f344664de0f0b6ec1d93015'
FILES = {
    'dxcapi.h': 'a8d409642ded485ea4fd4c055f1199a13538f0393755c2d958ade281afed4ca0',
    'WinAdapter.h': 'f5688a1408a8de8c0c35176bc900f21d7679d492215da94da4ab643cb66867f4',
}

def main():
    target = STATE / 'third-party/include/dxc'
    target.mkdir(parents=True, exist_ok=True)
    for name, digest in FILES.items():
        dest = target / name
        data = dest.read_bytes() if dest.exists() else urllib.request.urlopen(
            f'https://raw.githubusercontent.com/microsoft/DirectXShaderCompiler/{COMMIT}/include/dxc/{name}', timeout=30).read()
        if hashlib.sha256(data).hexdigest() != digest:
            raise SystemExit(f'Checksum mismatch: {name}')
        dest.write_bytes(data)
        print(f'Verified {name}')

if __name__ == '__main__':
    main()
