"""Restore the isolated Wine namespace after its game and Steam are stopped."""
from pathlib import Path
import hashlib,json,shutil
from local_config import load_config
root=load_config()['state']/'display'
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
