"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Restore the exact original cloned FH6 executable after the test is stopped."""
from pathlib import Path
import hashlib, json, shutil
root=Path(__file__).resolve().parent
record=json.loads((root/'import-change.json').read_text())
exe=Path(record['executable']); backup=Path(record['backup'])
assert hashlib.sha256(backup.read_bytes()).hexdigest()==record['original_sha256'], 'Backup has changed'
assert hashlib.sha256(exe.read_bytes()).hexdigest() in (record['original_sha256'],record['patched_sha256']), 'Executable has changed since this test; refusing restoration'
shutil.copy2(backup,exe)
assert hashlib.sha256(exe.read_bytes()).hexdigest()==record['original_sha256']
dll=exe.parent/'fh6-display-size.dll'
if dll.exists():
 assert hashlib.sha256(dll.read_bytes()).digest()==hashlib.sha256((root/'fh6-display-size.dll').read_bytes()).digest(), 'DLL changed; refusing deletion'
 dll.unlink()
print('Restored cloned FH6 executable and removed test DLL')
