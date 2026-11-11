import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import shlex
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import launch as launch_v2
import launch_current


class LaunchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.cfg = dict(runtime=root/'runtime', bottle=root/'bottle', state=root/'state', loader=root/'loader', probe_runtime=root/'probe', metal=root/'metal', steam_exe='steam.exe')
        required = [root/'metal', root/'loader', root/'runtime/bin/wine', root/'runtime/lib/wine/x86_64-unix/wine', root/'probe/bin/wine']
        required += [root/'state/build'/name for name in ('pipeline_log_capture.dylib', 'shader_trial.dylib', 'depth_source_trial.dylib')]
        for path in required:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'fixture')
        self.folder = root/'state/shaders/prepared-fallbacks'
        (self.folder/'replacements').mkdir(parents=True)
        self.digest = 'a'*64
        (self.folder/'replacements'/(self.digest+'.dxil')).write_bytes(b'replacement')
        self.manifest = [dict(success=True, original_sha256=self.digest, replacement_sha256=hashlib.sha256(b'replacement').hexdigest())]
        self.save_manifest()

    def save_manifest(self):
        (self.folder/'manifest.json').write_text(json.dumps(self.manifest))

    def run_launch(self, *args, inherited=None):
        output = io.StringIO()
        with patch.object(launch_v2, 'load_config', return_value=self.cfg), patch.object(launch_v2, 'METAL_SHA256', hashlib.sha256(b'fixture').hexdigest()), patch.object(sys, 'argv', ['launch_v2.py', *args]), patch.dict(os.environ, inherited or {}, clear=True), contextlib.redirect_stdout(output), patch.object(launch_v2.subprocess, 'Popen') as popen:
            popen.return_value.stdout = io.BytesIO(b'unrelated private log\nFH6_SHADER_TRIAL_LOADED\n')
            popen.return_value.wait.return_value = 0
            if '--check' in args:
                launch_v2.main()
            else:
                with self.assertRaises(SystemExit) as caught:
                    launch_v2.main()
                self.assertEqual(caught.exception.code, 0)
            return output.getvalue(), popen.call_args

    def test_fallback_validation_survives_disabled_depth(self):
        self.manifest[0]['replacement_sha256'] = 'b'*64
        self.save_manifest()
        with self.assertRaisesRegex(SystemExit, 'checksum mismatch'):
            self.run_launch('--check', '--no-depth-repair')

    def test_control_with_depth_does_not_require_replacements(self):
        (self.folder/'manifest.json').unlink()
        output, _ = self.run_launch('--check', '--shaders', 'control')
        self.assertIn('depth_source_trial.dylib', output)
        self.assertIn('FH6_SHADER_TRIAL=control', output)

    def test_effective_shader_override_is_validated(self):
        (self.folder/'manifest.json').unlink()
        with self.assertRaisesRegex(SystemExit, 'Generate local shader replacements'):
            self.run_launch('--check', '--shaders', 'control', '--env', 'FH6_SHADER_TRIAL=fallback')

    def test_inherited_switches_cannot_leak_and_explicit_overrides_win(self):
        inherited = dict(HB_TSSHIM_DEBUG='1', FH6SHIM_PREFILL='1', D3DM_MTL4='0', WINEDEBUG='+all', CX_ROOT='stale', DYLD_INSERT_LIBRARIES='stale', MTL_DEBUG_LAYER='1', FH6_MEMORY_MODE='stale', KEEP_ME='yes')
        _, call = self.run_launch('--env', 'FH6SHIM_DIAG=1', inherited=inherited)
        self.assertEqual(call.kwargs['env'], {'KEEP_ME': 'yes'})
        command = call.args[0]
        values = dict(item.split('=', 1) for item in shlex.split(command[command.index('--env')+1]))
        self.assertEqual(values['D3DM_MTL4'], '1')
        self.assertEqual(values['FH6SHIM_DIAG'], '1')
        self.assertNotIn('FH6SHIM_PREFILL', values)

    def test_unset_removes_inherited_and_constructed_variables(self):
        _, call = self.run_launch('--unset', 'OTHER_SETTING', '--unset', 'SteamDeck', inherited={'OTHER_SETTING': 'stale', 'SteamDeck': 'stale'})
        self.assertNotIn('OTHER_SETTING', call.kwargs['env'])
        self.assertNotIn('SteamDeck=', call.args[0][call.args[0].index('--env')+1])

    def test_raw_log_is_opt_in_and_graphics_filter_stays_active(self):
        self.run_launch()
        logs = next((self.cfg['state']/'logs').iterdir())
        self.assertFalse((logs/'raw.log').exists())
        self.assertNotIn(b'unrelated private', (logs/'graphics.log').read_bytes())

    def test_direct_launch_preserves_environment_and_passes_steam_to_native_loader(self):
        _, call = self.run_launch('--direct', '--steam-silent')
        command = call.args[0]
        self.assertNotIn('--cx-app', command)
        self.assertEqual(command[command.index('--ux-app'):], ['--ux-app', str(self.cfg['loader']), '--', 'steam.exe', '-silent', '-applaunch', '2483190'])
        self.assertIn('DYLD_INSERT_LIBRARIES=', command[command.index('--env')+1])

    def test_current_wrapper_unsets_profile_defaults_and_keeps_explicit_overrides(self):
        for extra, expected in [([], None), (['--env', 'FH6SHIM_DIAG=0'], '0')]:
            with patch.object(launch_current, 'load_config', return_value=self.cfg), patch.object(launch_current, 'verify_current'), patch.object(sys, 'argv', ['launch_current.py', '--unset', 'FH6SHIM_DIAG', '--unset', 'WINEMSYNC', *extra]), patch.object(launch_current.launch, 'main'):
                launch_current.main()
                forwarded = sys.argv[1:]
            _, call = self.run_launch(*forwarded)
            command = call.args[0]
            values = dict(item.split('=', 1) for item in shlex.split(command[command.index('--env')+1]))
            self.assertEqual(values.get('FH6SHIM_DIAG'), expected)
            self.assertNotIn('WINEMSYNC', values)


if __name__ == '__main__':
    unittest.main()
