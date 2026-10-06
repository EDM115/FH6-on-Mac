"""Run authored offscreen probes; check results, not just process exit codes."""
import json
import os
import subprocess
from local_config import load_config

def main():
    cfg=load_config();root=cfg['repo'];build=cfg['state']/'build'
    env=dict(os.environ,MTL_DEBUG_LAYER='1')
    env.pop('DYLD_INSERT_LIBRARIES',None)
    def run(args,hook=None):
        local=dict(env)
        if hook:local['DYLD_INSERT_LIBRARIES']=str(build/hook)
        p=subprocess.run([str(build/args[0]),*map(str,args[1:])],env=local,capture_output=True,text=True,timeout=60)
        if p.returncode:
            raise RuntimeError(f'{args[0]} exited {p.returncode}: {p.stderr}. Exit 2 can mean no accessible Metal device.')
        return p
    for name,hook,args,marker in [
        ('compute',None,['depth_read_probe'],'DEPTH_FLOAT_READ_PASSED'),
        ('fragment',None,['depth_fragment_probe'],'DEPTH_FRAGMENT_RESOLVE_PASSED'),
        ('hooked fragment','depth_source_trial.dylib',['depth_fragment_probe','test-hook'],'DEPTH_FRAGMENT_RESOLVE_PASSED'),
    ]:
        p=run(args,hook)
        if marker not in p.stdout or 'pixels=64 depth=0.25' not in p.stdout:
            raise RuntimeError(f'{name}: expected readback was not reported')
        print(f'PASS {name}: 64 pixels, average 0.25')
    p=run(['pipeline_log_probe'],'pipeline_log_capture.dylib')
    custom=[s for s in p.stderr.splitlines() if s.startswith('FH6_PIPELINE_DIAGNOSTIC')]
    if len(custom)!=1 or 'synthetic diagnostic error' not in custom[0] or 'do-not-capture' in custom[0]:
        raise RuntimeError('Pipeline logger filtering failed')
    print('PASS pipeline logger filtering')
    for mode,extra in [('original',[]),('fallback',[root/'tests/native/shading_rate_fallback.ll'])]:
        out=cfg['state']/'tests'/('shading-'+mode);out.mkdir(parents=True,exist_ok=True)
        p=run(['shading_rate_repro',cfg['resources'],out,*extra])
        report=json.loads(p.stdout)
        pipelines={v['fragment']:v for v in report['pipelines']}
        control=pipelines['fragment_control']
        broken=pipelines['fragment_shading_rate']
        if not control['success'] or control.get('center_rgba')!=[255,0,0,255] or broken['success'] or 'sv_shadingrate0' not in broken.get('error',''):
            raise RuntimeError('Original converter behavior differs from the recorded control')
        if mode=='fallback':
            good=pipelines['fragment_fallback']
            if not good['success'] or good.get('center_rgba')!=[255,0,0,255]:
                raise RuntimeError('Fallback pixel readback failed')
        print(f'PASS shading {mode}: expected pipeline results')

if __name__=='__main__':main()
