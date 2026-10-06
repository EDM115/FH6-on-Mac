"""Small, asset-free tests for export guards and shader transformations."""
import ast
import hashlib
import importlib.util
import json
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_public_tree import audit

# Load only the pure rewrite function; don't run the script's configured I/O.
tree=ast.parse((ROOT/'tools/prepare_fallbacks.py').read_text())
function=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='rewrite')
namespace={'re':re}
exec(compile(ast.Module(body=[function],type_ignores=[]),'rewrite-fixture','exec'),namespace)
rewrite=namespace['rewrite']

FIXTURE='''define void @main() {
  %rate = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  ret void
}
declare i32 @dx.op.loadInput.i32(i32, i32, i32, i8, i32)
!dx.shaderModel = !{!0}
!dx.entryPoints = !{!1}
!0 = !{!"ps", i32 6, i32 6}
!1 = !{void ()* @main, !"main", !2, null, !5}
!2 = !{!3, null, null}
!3 = !{!4}
!4 = !{i32 0, !"SV_ShadingRate", i8 5, i8 29, !6, i8 1, i32 1, i8 1, i32 0, i8 0, null}
!5 = !{i32 0, i64 16777219}
!6 = !{i32 0}
'''

class ToolsTests(unittest.TestCase):
    def test_public_tree(self):
        _,errors=audit();self.assertEqual(errors,[])
    def test_rewrite_keeps_other_flags_and_removes_unused_input(self):
        result,edits=rewrite(FIXTURE)
        self.assertEqual(edits['new_flags'],3)
        self.assertIn('%rate = add i32 0, 0',result)
        self.assertIn('!3 = !{}',result)
        self.assertNotIn('declare i32 @dx.op.loadInput.i32',result)
    def test_unexpected_input_shape_rejected(self):
        with self.assertRaises(AssertionError):rewrite(FIXTURE.replace('i8 29','i8 28'))
    def test_missing_feature_flag_rejected(self):
        with self.assertRaises(AssertionError):rewrite(FIXTURE.replace('16777219','3'))
    def test_audit_rejects_binary_and_home_path(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'bad.exe').write_bytes(b'MZ\x00')
            # Construct a synthetic path without embedding a personal path in this repo.
            (root/'note.md').write_text('/'+'Users'+'/example/private-file')
            _,errors=audit(root);self.assertEqual(len(errors),2)
    def test_cache_extractor_bounds_and_candidate_selection(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'cache';output=root/'extracted'
            parts=[b'DXIL'+struct.pack('<I',4)+struct.pack('<I',0),b'ISG1'+struct.pack('<I',15)+b'SV_ShadingRate\0\0']
            header=bytearray(40);header[:4]=b'DXBC'
            struct.pack_into('<IIII',header,24,40+sum(map(len,parts)),2,40,40+len(parts[0]))
            blob=bytes(header)+b''.join(parts)
            source.write_bytes(b'DXBCbad'+blob+b'DXBCbad')
            result=subprocess.run([sys.executable,str(ROOT/'tools/extract_cache.py'),str(source),str(output)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            index=json.loads((output/'index.json').read_text())
            self.assertEqual(index['containers'],1)
            self.assertEqual(index['invalid_candidates'],2)
            self.assertEqual((output/(hashlib.sha256(blob).hexdigest()+'.dxil')).read_bytes(),blob)

if __name__=='__main__':unittest.main()
