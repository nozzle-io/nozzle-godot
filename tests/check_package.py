#!/usr/bin/env python3
import configparser
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PKG = ROOT / 'build/package/nozzle-godot'
required = [
    PKG / 'README.md',
    PKG / 'LICENSE',
    PKG / 'SConstruct',
    PKG / 'include/nozzle_godot/nozzle_diagnostics.hpp',
    PKG / 'src/nozzle_diagnostics.cpp',
    PKG / 'src/register_types.cpp',
    PKG / 'project/project.godot',
    PKG / 'project/nozzle_godot.gdextension',
    PKG / 'project/scenes/nozzle_diagnostics.tscn',
    PKG / 'project/scenes/nozzle_diagnostics.gd',
]
for path in required:
    if not path.exists():
        raise SystemExit(f'missing {path}')

scene = (PKG / 'project/scenes/nozzle_diagnostics.tscn').read_text(encoding='utf-8')
if '[node name="NozzleDiagnosticsExample" type="Node"]' not in scene:
    raise SystemExit('runtime scene root is not a plain Node smoke harness')
script = (PKG / 'project/scenes/nozzle_diagnostics.gd').read_text(encoding='utf-8')
if not script.startswith('extends Node'):
    raise SystemExit('runtime script does not use a parse-safe plain Node smoke harness')
if 'GDExtensionManager.load_extension(extension_path)' not in script:
    raise SystemExit('runtime script does not explicitly load the packaged GDExtension')
if 'ClassDB.instantiate("NozzleDiagnostics")' not in script:
    raise SystemExit('runtime script does not instantiate NozzleDiagnostics after extension load')
project = (PKG / 'project/project.godot').read_text(encoding='utf-8')
if 'run/main_scene="res://scenes/nozzle_diagnostics.tscn"' not in project:
    raise SystemExit('project main scene does not point to the runtime smoke scene')

bin_dir = PKG / 'project/bin'
libs = [p for p in bin_dir.rglob('*') if p.is_file()]
if not libs:
    raise SystemExit('missing built GDExtension library under project/bin')

parser = configparser.ConfigParser()
parser.read(PKG / 'project/nozzle_godot.gdextension')
if parser.get('configuration', 'entry_symbol') != '"nozzle_godot_library_init"':
    raise SystemExit('entry_symbol mismatch')
if parser.get('configuration', 'compatibility_minimum') != '"4.6"':
    raise SystemExit('compatibility_minimum mismatch')

readme = (PKG / 'README.md').read_text(encoding='utf-8')
for phrase in ['UNPROVEN_RUNTIME_TEXTURE_PATH', 'PUBLIC_API_SURFACE_COMPILES', 'UNSUPPORTED_UNPROVEN', '4.6.3-stable', '58d1de720b8ffe9f8ffcdfe3a85148582cfd2e74']:
    if phrase not in readme:
        raise SystemExit(f'README missing {phrase}')

zips = sorted((ROOT / 'build').glob('nozzle-godot-latest-*.zip'))
if len(zips) != 1:
    raise SystemExit(f'expected one zip, found {zips}')
with zipfile.ZipFile(zips[0]) as zf:
    names = zf.namelist()
    roots = {name.split('/', 1)[0] for name in names if name}
    if roots != {'nozzle-godot'}:
        raise SystemExit(f'wrong zip roots: {roots}')
    if any(name.startswith('nozzle-godot-latest-') for name in names):
        raise SystemExit('zip contains extra wrapper directory')
    if not any(name.startswith('nozzle-godot/project/bin/') for name in names):
        raise SystemExit('zip missing built GDExtension library')
print('nozzle-godot package shape ok')
