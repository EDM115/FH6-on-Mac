"""Exercise capture failure cleanup without starting a profiler or inspecting any process"""
import ast
from pathlib import Path
import subprocess
from types import SimpleNamespace
import unittest
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[1]
TREE = ast.parse((ROOT / 'experiments/frame-pauses/capture_cpu_and_frames.py').read_text())
CLEANUP = next(node for node in TREE.body if isinstance(node, ast.FunctionDef) and node.name == 'finish_capture')
CAPTURE = next(node for node in TREE.body if isinstance(node, ast.Try))


class CaptureCleanupTests(unittest.TestCase):
    def namespace(self):
        api = SimpleNamespace(run=Mock(return_value=SimpleNamespace(returncode=0)), TimeoutExpired=subprocess.TimeoutExpired)
        namespace = {'subprocess': api}
        exec(compile(ast.Module(body=[CLEANUP], type_ignores=[]), 'capture-cleanup', 'exec'), namespace)
        return namespace, api

    def test_log_reaping_failure_does_not_skip_metrics_disable(self):
        namespace, api = self.namespace()
        process = Mock()
        process.poll.return_value = None
        process.wait.side_effect = subprocess.TimeoutExpired('log', 5)
        setup = {}
        with self.assertRaises(subprocess.TimeoutExpired):
            namespace['finish_capture'](process, 42, setup, True)
        process.kill.assert_called_once()
        self.assertIn('--disable', api.run.call_args.args[0])
        self.assertEqual(setup['disable_exit'], 0)

    def test_enable_timeout_still_attempts_disable(self):
        namespace, api = self.namespace()
        api.run.side_effect = [subprocess.TimeoutExpired('metalperftrace', 12), SimpleNamespace(returncode=0)]
        namespace.update(args=SimpleNamespace(pid=42), setup={}, process=None, enable_attempted=False)
        with self.assertRaises(subprocess.TimeoutExpired):
            exec(compile(ast.Module(body=[CAPTURE], type_ignores=[]), 'capture-enable', 'exec'), namespace)
        self.assertEqual(api.run.call_count, 2)
        self.assertIn('--enable', api.run.call_args_list[0].args[0])
        self.assertIn('--disable', api.run.call_args_list[1].args[0])
        self.assertEqual(namespace['setup']['disable_exit'], 0)


if __name__ == '__main__':
    unittest.main()
