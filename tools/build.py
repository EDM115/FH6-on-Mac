"""Build authored helpers locally. Does not install files, sign Wine, or launch FH6."""
import argparse
import subprocess
import sys
from local_config import load_config

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tests', action='store_true', help='Also compile standalone native probes')
    args = parser.parse_args()
    cfg = load_config()
    root, state, resources = cfg['repo'], cfg['state'], cfg['resources']
    out = state / 'build'
    out.mkdir(parents=True, exist_ok=True)
    shaders = state / 'shaders'
    for sub in ('control-cache', 'fallback-cache'):
        (shaders / sub).mkdir(parents=True, exist_ok=True)
    includes = state / 'third-party/include'
    if not (includes / 'dxc/dxcapi.h').is_file():
        raise SystemExit('Run tools/fetch_dxc_headers.py first')
    if not (resources / 'libmetalirconverter.dylib').is_file():
        raise SystemExit('Configured runtime lacks the GPTK shader converter')
    def compile(source, destination, extra=(), objc=False, c=False):
        command = ['/usr/bin/clang' if c else '/usr/bin/clang++', '-arch', 'x86_64',
                   '-std=c11' if c else '-std=c++17', '-O2', '-g', '-fno-omit-frame-pointer', '-Wall', '-Wextra']
        if objc:
            command += ['-framework', 'Foundation', '-framework', 'Metal']
        command += [str(root / source), '-o', str(out / destination), *extra]
        print(f'Building {destination}', flush=True)
        subprocess.run(command, check=True)
    compile('src/diagnostics/pipeline_log_capture.c', 'pipeline_log_capture.dylib', ['-dynamiclib'], c=True)
    compile('src/depth/depth_source_trial.mm', 'depth_source_trial.dylib', ['-dynamiclib'], objc=True)
    compile('src/shaders/shader_trial.cpp', 'shader_trial.dylib', ['-dynamiclib',
        '-DFH6_TRIAL_ROOT="'+str(shaders)+'"', '-L'+str(resources), '-lmetalirconverter', '-Wl,-rpath,'+str(resources)])
    compile('src/shaders/dxil_tool.cpp', 'dxil_tool', ['-I'+str(includes)])
    subprocess.run([sys.executable, str(root/'tools/build_window_inventory.py')], check=True)
    if args.tests:
        for name in ('depth_fragment_probe', 'depth_read_probe'):
            compile('tests/native/'+name+'.mm', name, objc=True)
        compile('tests/native/shading_rate_repro.mm', 'shading_rate_repro', ['-I'+str(includes)], objc=True)
        compile('tests/native/pipeline_log_probe.c', 'pipeline_log_probe', c=True)
    print('Build complete. Outputs are private under .local/.')

if __name__ == '__main__':
    main()
