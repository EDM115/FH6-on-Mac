import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import runpy
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from launch_current import current_arguments, verify_current


class CurrentLaunchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.cfg = {'runtime': root / 'runtime', 'bottle': root / 'bottle'}
        self.recorded = {'components': {'bin/wineserver': hashlib.sha256(b'server').hexdigest()}, 'game': {'steam_build': '25588841', 'sha256': hashlib.sha256(b'game').hexdigest()}}
        server = self.cfg['runtime'] / 'bin/wineserver'
        server.parent.mkdir(parents=True)
        server.write_bytes(b'server')
        self.steam = self.cfg['bottle'] / 'drive_c/Program Files (x86)/Steam/steamapps'
        game = self.steam / 'common/ForzaHorizon6/forzahorizon6.exe'
        game.parent.mkdir(parents=True)
        game.write_bytes(b'game')
        (self.steam / 'appmanifest_2483190.acf').write_text('"buildid" "25588841"\n"StateFlags" "4"\n')

    def test_checked_runtime_and_game_are_admitted(self):
        verify_current(self.cfg, self.recorded)

    def test_changed_component_is_rejected(self):
        (self.cfg['runtime'] / 'bin/wineserver').write_bytes(b'other')
        with self.assertRaisesRegex(SystemExit, 'Component differs'):
            verify_current(self.cfg, self.recorded)

    def test_changed_game_is_rejected(self):
        (self.steam / 'common/ForzaHorizon6/forzahorizon6.exe').write_bytes(b'new game')
        with self.assertRaisesRegex(SystemExit, 'executable differs'):
            verify_current(self.cfg, self.recorded)

    def test_incomplete_update_is_rejected(self):
        (self.steam / 'appmanifest_2483190.acf').write_text('"buildid" "25588841"\n"StateFlags" "6"\n')
        with self.assertRaisesRegex(SystemExit, 'not marked'):
            verify_current(self.cfg, self.recorded)

    def test_quiet_online_defaults_keep_depth_repair_and_exclude_relay(self):
        args = current_arguments(self.cfg)
        values = dict(args[index + 1].split('=', 1) for index, value in enumerate(args) if value == '--env')
        self.assertEqual(values['FH6_DUALSTACK_TTL255'], '1')
        self.assertEqual(values['FH6_HTTP_DIAG'], '0')
        self.assertEqual(values['WINEMSYNC'], '1')
        self.assertEqual(values['HB_TSSHIM_DEPTHRESOLVE'], '1')
        self.assertEqual(values['HB_TSSHIM_APPEND_OFFSETS'], '1')
        self.assertNotIn('--raw-log', args)
        self.assertFalse(any('relay' in value for value in args))

    def test_window_probe_passes_sync_mode_and_windows_path_to_native_loader(self):
        self.cfg.update(state=Path(self.temp.name) / 'private state', probe_runtime=self.cfg['runtime'], loader=Path(self.temp.name) / 'native loader')
        result = SimpleNamespace(returncode=0, stdout=b'', stderr=b'')
        with patch('local_config.load_config', return_value=self.cfg), patch('subprocess.run', return_value=result) as run, patch.object(sys, 'argv', ['window_snapshot.py', 'fixture', '--msync', '0']):
            runpy.run_path(str(ROOT / 'tools/window_snapshot.py'), run_name='__main__')
        command = run.call_args.args[0]
        self.assertEqual(command[command.index('--env') + 1], 'WINEMSYNC=0')
        self.assertEqual(command[command.index('--ux-app') + 1], str(self.cfg['loader']))
        self.assertEqual(command[-2], '--')
        self.assertTrue(command[-1].startswith('Z:'))
        self.assertIn('private state', command[-1])


if __name__ == '__main__':
    unittest.main()
