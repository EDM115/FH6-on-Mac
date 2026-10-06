"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Prepare one reversible popup-layout experiment; never install it here."""
from pathlib import Path
import hashlib
import json
import shutil
import xml.etree.ElementTree as ET
import zipfile

base = Path(__file__).resolve().parents[2]
out = Path(__file__).resolve().parent
target = base / 'work/bottles/FH6-GPTK4/drive_c/Program Files (x86)/Steam/steamapps/common/ForzaHorizon6/media/UI.zip'
assert target.resolve() == target and target.stat().st_nlink == 1
original = out / 'UI.original.zip'
if original.exists():
    assert target.read_bytes() == original.read_bytes(), 'Restore the original before preparing again'
else:
    shutil.copy2(target, original)
member = 'Scenes/Anthem/A_MessageBox.xaml'
replacement = out / 'UI.no-content-transition.zip'
old = b'Style="{StaticResource A_Style_FrameworkElement_DefaultTransition_0}"'
new = b'Opacity="1" local:ZPanel.ZOffset="0"'
with zipfile.ZipFile(original) as src, zipfile.ZipFile(replacement, 'w') as dst:
    dst.comment = src.comment
    for info in src.infolist():
        data = src.read(info)
        if info.filename == member:
            assert data.count(old) == 1
            assert b'x:Name="MainContent"' in data
            changed = data.replace(old, new)
            # Parse both and check that only the named container attributes change.
            before, after = ET.fromstring(data), ET.fromstring(changed)
            ns = '{http://schemas.microsoft.com/winfx/2006/xaml}'
            nodes = [n for n in before.iter() if n.get(ns+'Name') == 'MainContent']
            assert len(nodes) == 1 and nodes[0].tag.endswith('DockPanel')
            nodes[0].attrib.pop('Style')
            nodes[0].set('Opacity', '1')
            nodes[0].set('{clr-namespace:ForzaUI;assembly=}ZPanel.ZOffset', '0')
            assert ET.canonicalize(ET.tostring(before, encoding='unicode')) == ET.canonicalize(ET.tostring(after, encoding='unicode'))
            data = changed
        dst.writestr(info, data)
with zipfile.ZipFile(original) as src, zipfile.ZipFile(replacement) as dst:
    assert dst.testzip() is None
    assert src.namelist() == dst.namelist()
    changes = [n for n in src.namelist() if src.read(n) != dst.read(n)]
    assert changes == [member]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
manifest = dict(target=str(target), original=str(original), replacement=str(replacement),
                original_sha256=sha(original), replacement_sha256=sha(replacement),
                changed_members=changes, entries=len(dst.namelist()),
                change='Only MainContent: remove animated transition style; explicitly use opacity 1 and Z offset 0.',
                hypothesis='An opacity/depth transition can hide constructed popup contents. Removing it may reveal them.',
                status='Prepared and validated; not installed',
                rollback='Stop only FH6-GPTK4 bottle, restore UI.original.zip to target, verify original SHA-256, then relaunch.')
(out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(json.dumps(manifest, indent=2))
