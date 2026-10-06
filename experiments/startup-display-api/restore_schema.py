"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Restore the isolated Wine namespace after its game and Steam are stopped."""
from pathlib import Path
import hashlib,json,shutil
root=Path(__file__).resolve().parent
record=json.loads((root/'schema-change.json').read_text())
schema=Path(record['schema']); backup=Path(record['backup']); proxy=Path(record['proxy'])
assert hashlib.sha256(backup.read_bytes()).hexdigest()==record['original_sha256'],'Schema backup changed'
assert hashlib.sha256(schema.read_bytes()).hexdigest() in (record['original_sha256'],record['modified_sha256']),'Schema changed since this test; refusing restoration'
if proxy.exists():
 assert hashlib.sha256(proxy.read_bytes()).digest()==hashlib.sha256((root/'fh6disp.dll').read_bytes()).digest(),'Proxy changed; refusing deletion'
shutil.copy2(backup,schema)
assert hashlib.sha256(schema.read_bytes()).hexdigest()==record['original_sha256']
if proxy.exists():proxy.unlink()
print('Restored standalone cloned Wine schema and removed cloned system32 proxy')
