#!/usr/bin/env python3
import shutil
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
PKG = BUILD / 'package' / 'nozzle-godot'

if PKG.exists():
    shutil.rmtree(PKG)
for rel in ['include', 'src', 'project']:
    shutil.copytree(ROOT / rel, PKG / rel)
for rel in ['README.md', 'LICENSE', 'SConstruct']:
    shutil.copy2(ROOT / rel, PKG / rel)
short = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], cwd=ROOT, text=True).strip()
for old_zip in BUILD.glob('nozzle-godot-latest-*.zip'):
    old_zip.unlink()
zip_path = BUILD / f'nozzle-godot-latest-{short}.zip'
with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zf:
    for path in sorted(PKG.rglob('*')):
        if path.is_file():
            zf.write(path, path.relative_to(BUILD / 'package'))
print(f'wrote {zip_path}')
