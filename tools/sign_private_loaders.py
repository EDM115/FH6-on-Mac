"""Inspect or ad hoc sign the two private Wine loaders for DYLD hooks."""
import argparse
import plistlib
import subprocess
from local_config import load_config

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true',help='Replace signatures on the configured private copies')
    args=parser.parse_args()
    cfg=load_config()
    child=(cfg['runtime']/'lib/wine/x86_64-unix/wine').resolve()
    if not child.is_relative_to(cfg['runtime']):
        raise SystemExit('Child loader points outside the private runtime')
    targets=[cfg['loader'],child]
    pending=[]
    for path in targets:
        if '/Applications/' in str(path) or any(p.endswith('.app') for p in path.parts):
            raise SystemExit('Refusing app-bundle/installed loader; prepare private copies outside the app')
        result=subprocess.run(['/usr/bin/codesign','-d','--entitlements',':-',str(path)],capture_output=True,check=True)
        existing=plistlib.loads(result.stdout)
        added=dict(existing);added['com.apple.security.cs.allow-dyld-environment-variables']=True
        pending.append((path,existing,added))
        print(f'{path}: preserve existing entitlements; allow DYLD environment variables')
    if not args.apply:
        print('Inspection only. --apply replaces these two private signatures; it does not change system security settings.')
        return
    out=cfg['state']/'signatures';out.mkdir(parents=True,exist_ok=True)
    for i,(path,existing,added) in enumerate(pending):
        original=out/f'{i}-original-entitlements.plist'
        if original.exists():
            raise SystemExit('Saved entitlements already exist. Inspect the previous signing attempt before retrying.')
    for i,(path,existing,added) in enumerate(pending):
        (out/f'{i}-original-entitlements.plist').write_bytes(plistlib.dumps(existing))
        entitlements=out/f'{i}-diagnostic-entitlements.plist'
        entitlements.write_bytes(plistlib.dumps(added))
        subprocess.run(['/usr/bin/codesign','--force','--sign','-','--options','runtime','--entitlements',str(entitlements),str(path)],check=True)
        subprocess.run(['/usr/bin/codesign','--verify','--strict',str(path)],check=True)
    print('Signed private loaders. Keep the original installed runtime to recreate unmodified copies.')

if __name__=='__main__':
    main()
