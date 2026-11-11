import argparse
import collections
import ctypes as C
import datetime
import json
from pathlib import Path
import subprocess
import tempfile
import time
import sys
import xml.etree.ElementTree as ET


class ThreadInfo(C.Structure):
    # Layout from the installed macOS SDK's sys/proc_info.h
    _fields_ = [('user_ns', C.c_uint64), ('system_ns', C.c_uint64)] + [(name, C.c_int32) for name in ('cpu', 'policy', 'state', 'flags', 'sleep', 'curpri', 'priority', 'maxpriority')] + [('name', C.c_char * 64)]


def finish_capture(process, pid, setup, enable_attempted):
    try:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
    finally:
        if enable_attempted:
            result = subprocess.run(['/usr/bin/metalperftrace', 'setup', '--disable', 'per-frame-metrics', '--pid', str(pid)], capture_output=True, text=True, timeout=12)
            setup['disable_exit'] = result.returncode


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from local_config import load_config

parser = argparse.ArgumentParser(description='Read FH6 thread counters alongside four process-scoped frame signposts without suspending threads')
parser.add_argument('--pid', type=int, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--expected-frame-rate', type=int, choices=(30, 60, 120), default=60)
args = parser.parse_args()
cfg = load_config()
args.output = args.output.expanduser().resolve()
if not args.output.is_relative_to(cfg['state'].resolve()):
    raise SystemExit('Capture output must be inside this checkout\'s private .local folder')
args.output.parent.mkdir(parents=True, exist_ok=True)
if args.output.exists():
    raise SystemExit('Refusing to overwrite an earlier capture')

config = cfg['bottle'] / 'drive_c/users/crossover/AppData/Local/ForzaHorizon6/LocalStorage_Shared/ForzaUserConfigSelections/UserConfigSelections'
tree = ET.fromstring(config.read_bytes())
settings = {element.attrib['id']: element.attrib['value'] for element in tree.iter('option') if element.attrib.get('id') in ('FrameRate', 'VSync')}
assert settings == {'FrameRate': {30: '0', 60: '2', 120: '3'}[args.expected_frame_rate], 'VSync': '1'}, settings
identity = subprocess.check_output(['/bin/ps', '-p', str(args.pid), '-o', 'comm='], text=True).strip()
assert identity.lower().endswith('forzahorizon6.exe'), identity
expected_game = (cfg['bottle'] / 'drive_c/Program Files (x86)/Steam/steamapps/common/ForzaHorizon6/forzahorizon6.exe').resolve()
mapped = subprocess.check_output(['/usr/sbin/lsof', '-a', '-p', str(args.pid), '-Fn'], text=True)
assert str(expected_game) in {str(Path(line[1:]).resolve()) for line in mapped.splitlines() if line.startswith('n/')}, 'PID does not map the configured game executable'

libproc = C.CDLL('/usr/lib/libproc.dylib', use_errno=True)
proc_pidinfo = libproc.proc_pidinfo
proc_pidinfo.argtypes = [C.c_int, C.c_int, C.c_uint64, C.c_void_p, C.c_int]
proc_pidinfo.restype = C.c_int
system = C.CDLL('/usr/lib/libSystem.B.dylib')
system.mach_continuous_time.restype = C.c_uint64


class Timebase(C.Structure):
    _fields_ = [('numer', C.c_uint32), ('denom', C.c_uint32)]


timebase = Timebase()
assert system.mach_timebase_info(C.byref(timebase)) == 0
thread_buffer = (C.c_uint64 * 2048)()
names = {}
samples = []
records = []
setup = {}
errors = collections.Counter()
enable_attempted = False
process = None
started = datetime.datetime.now(datetime.timezone.utc).isoformat()
cpu_start = time.process_time_ns()
frame_names = {'DrawableLifetime', 'ClientDrawable', 'ImageQueueInsert', 'OnGlassDrawableInterval'}
predicate = 'type == "signpostEvent" AND (' + ' OR '.join(f'signpostName == "{name}"' for name in sorted(frame_names)) + ')'
print(json.dumps({'pid': args.pid, 'settings': settings, 'interval_ms': 20, 'scope': 'thread CPU counters and four graphics timing events only'}), flush=True)

try:
    enable_attempted = True
    result = subprocess.run(['/usr/bin/metalperftrace', 'setup', '--enable', 'per-frame-metrics', '--pid', str(args.pid)], capture_output=True, text=True, timeout=12)
    setup['enable_exit'] = result.returncode
    assert result.returncode == 0
    with tempfile.TemporaryFile(mode='w+t') as stream, tempfile.TemporaryFile(mode='w+t') as stderr:
        process = subprocess.Popen(['/usr/bin/log', 'stream', '--process', str(args.pid), '--signpost', '--metric', '--level', 'debug', '--predicate', predicate, '--style', 'ndjson', '--timeout', '20', '--no-backtrace'], stdout=stream, stderr=stderr, text=True)
        deadline = time.monotonic() + 26
        next_sample = time.monotonic()
        next_list = 0
        thread_addresses = []
        while process.poll() is None and time.monotonic() < deadline:
            begin = system.mach_continuous_time()
            if time.monotonic() >= next_list:
                size = proc_pidinfo(args.pid, 6, 0, thread_buffer, C.sizeof(thread_buffer))
                assert size > 0 and size < C.sizeof(thread_buffer), (size, C.get_errno())
                thread_addresses = list(dict.fromkeys(thread_buffer[:size // 8]))
                next_list = time.monotonic() + 1
            rows = []
            for address in thread_addresses:
                info = ThreadInfo()
                size = proc_pidinfo(args.pid, 5, address, C.byref(info), C.sizeof(info))
                if size != C.sizeof(info):
                    errors[str(C.get_errno())] += 1
                    continue
                names[str(address)] = bytes(info.name).decode(errors='replace')
                rows.append([address, info.user_ns, info.system_ns, info.state])
            samples.append([begin, system.mach_continuous_time(), rows])
            next_sample += 0.02
            time.sleep(max(0, next_sample - time.monotonic()))
        if process.poll() is None:
            process.terminate()
        setup['stream_exit'] = process.wait(timeout=5)
        stream.seek(0)
        for line in stream:
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if row.get('signpostName') in frame_names:
                assert row.get('processID') == args.pid
                records.append(row)
        stderr.seek(0)
        setup['stream_stderr'] = stderr.read()
finally:
    finish_capture(process, args.pid, setup, enable_attempted)

result = {'started_utc': started, 'finished_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'host_pid': args.pid, 'settings': settings, 'setup': setup, 'timebase': {'numer': timebase.numer, 'denom': timebase.denom}, 'sample_clock': 'mach_continuous_time', 'observer_cpu_ms': (time.process_time_ns() - cpu_start) / 1e6, 'thread_counter_source': 'proc_pidinfo PROC_PIDLISTTHREADS and PROC_PIDTHREADINFO, cthread addresses identify this live process only', 'counter_units': 'nanoseconds, microsecond precision per XNU fill_taskthreadinfo', 'thread_names': names, 'sample_columns': ['begin_mach_ticks', 'end_mach_ticks', 'threads'], 'thread_columns': ['cthread_address', 'user_ns', 'system_ns', 'run_state'], 'samples': samples, 'read_errors': dict(errors), 'records': records}
with args.output.open('x') as destination:
    json.dump(result, destination)
    destination.write('\n')
print(json.dumps({'samples': len(samples), 'threads': len(names), 'events': len(records), 'observer_cpu_ms': result['observer_cpu_ms'], 'setup': setup, 'read_errors': dict(errors)}, indent=2), flush=True)
assert setup.get('disable_exit') == 0 and setup.get('stream_exit') == 0
