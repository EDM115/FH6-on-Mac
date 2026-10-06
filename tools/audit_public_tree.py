"""Check source syntax, local Markdown links and common accidental data exports."""
import ast
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
IGNORED={'.git','.local','.venv','__pycache__'}
ALLOWED={'.md','.py','.c','.cpp','.m','.mm','.s','.ll','.json'}
NAMES={'LICENSE','.gitignore'}
PATTERNS={
    'absolute home path':re.compile(r'/(?:Users|home)/[A-Za-z0-9_.-]+/'),
    'private temporary path':re.compile(r'/(?:private/)?var/folders/[A-Za-z0-9_/.-]+'),
    'JWT-like value':re.compile(r'eyJ[A-Za-z0-9_-]{12,}\.[A-Za-z0-9_-]{12,}\.[A-Za-z0-9_-]{12,}'),
    'private key':re.compile(r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----'),
}

def audit(root=ROOT):
    errors=[];count=0
    for file in sorted(root.rglob('*')):
        rel=file.relative_to(root)
        if any(part in IGNORED for part in rel.parts):continue
        if file.is_symlink():
            errors.append(f'{rel}: symlink requires manual provenance review');continue
        if not file.is_file():continue
        count+=1
        if file.suffix not in ALLOWED and file.name not in NAMES:
            errors.append(f'{rel}: unexpected file type');continue
        try:text=file.read_text()
        except UnicodeDecodeError:
            errors.append(f'{rel}: binary/non-UTF-8 file');continue
        if '\0' in text:errors.append(f'{rel}: NUL byte')
        for label,pattern in PATTERNS.items():
            if pattern.search(text):errors.append(f'{rel}: {label}')
        if file.suffix=='.py':
            try:ast.parse(text,filename=str(rel))
            except SyntaxError as exc:errors.append(f'{rel}: {exc}')
        if file.suffix=='.md':
            for target in re.findall(r'\]\(([^)]+)\)',text):
                if target.startswith(('https://','http://','#','mailto:')):continue
                path=target.split('#',1)[0]
                if path and not (file.parent/path).exists():errors.append(f'{rel}: broken local link {target}')
    return count,errors

def main():
    count,errors=audit()
    for error in errors:print(error)
    print(f'Checked {count} public files; {len(errors)} findings. This scan does not replace manual content review.')
    raise SystemExit(bool(errors))

if __name__=='__main__':main()
