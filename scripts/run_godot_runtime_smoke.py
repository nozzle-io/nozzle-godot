#!/usr/bin/env python3
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
TOOLS = BUILD / 'tools'
GODOT_VERSION = '4.6.3-stable'
GODOT_TAG_SHA = '7d41c59c457bd5a245092b4e7eb2d833e3b3f8c3'
GODOT_TARGET_SHA = '35e80b3a8822a9df9be390814b62f44c0a9c69e8'
LINUX_ZIP_NAME = 'Godot_v4.6.3-stable_linux.x86_64.zip'
LINUX_URL = f'https://github.com/godotengine/godot/releases/download/{GODOT_VERSION}/{LINUX_ZIP_NAME}'
LINUX_SHA256 = 'd0bc2113065e481c9c2c2b2c37daa4e8be3fe9e27f0ab9ab0b6096e9a37907f3'

REQUIRED_MARKERS = [
    'NOZZLE_GODOT_EXTENSION_CLASS class=NozzleDiagnostics available=',
    'NOZZLE_GODOT_VERSION=',
    'NOZZLE_GODOT_RUNTIME os=',
    'NOZZLE_GODOT_STATUS=',
    'NOZZLE_GODOT_PUBLIC_TEXTURE_API=',
    'NOZZLE_GODOT_TEXTURE_PATH=',
    'NOZZLE_GODOT_CPU_ORACLE size=320x240',
    'NOZZLE_GODOT_CPU_ORACLE size=641x479',
    'NOZZLE_GODOT_RUNTIME_SMOKE PASS',
    'godot_runtime_smoke',
    'godot_texture_to_nozzle',
    'nozzle_to_godot_texture',
    'MISSING_HOST_SMOKE',
    'no_y_flip',
    'no_r_b_swap',
    'alpha',
    'byte_size_mismatch',
    'copy_cost',
    'UNPROVEN',
]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def run(cmd, cwd=None):
    print('+ ' + ' '.join(str(part) for part in cmd), flush=True)
    completed = subprocess.run(cmd, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    print(completed.stdout, end='', flush=True)
    if completed.returncode != 0:
        raise SystemExit(f'command failed with exit code {completed.returncode}: {cmd}')
    return completed.stdout


def ensure_godot_linux():
    TOOLS.mkdir(parents=True, exist_ok=True)
    zip_path = TOOLS / LINUX_ZIP_NAME
    if not zip_path.exists() or sha256(zip_path) != LINUX_SHA256:
        print(f'downloading {LINUX_URL}', flush=True)
        urllib.request.urlretrieve(LINUX_URL, zip_path)
    digest = sha256(zip_path)
    if digest != LINUX_SHA256:
        raise SystemExit(f'Godot zip sha256 mismatch: {digest}')

    extract_dir = TOOLS / 'godot-4.6.3-linux-x86_64'
    executable = extract_dir / 'Godot_v4.6.3-stable_linux.x86_64'
    if not executable.exists():
        if extract_dir.exists():
            shutil.rmtree(extract_dir)
        extract_dir.mkdir(parents=True)
        with zipfile.ZipFile(zip_path) as archive:
            archive.extractall(extract_dir)
        if not executable.exists():
            raise SystemExit('Godot executable not found after extraction')
        executable.chmod(executable.stat().st_mode | 0o111)
    print(f'NOZZLE_GODOT_BINARY version={GODOT_VERSION} tag={GODOT_TAG_SHA} target={GODOT_TARGET_SHA} path={executable}', flush=True)
    return executable


def main():
    if platform.system() != 'Linux':
        raise SystemExit('run_godot_runtime_smoke.py currently requires Linux x86_64 CI')
    executable = ensure_godot_linux()
    command = [
        str(executable),
        '--headless',
        '--path',
        str(ROOT / 'project'),
        '--quit-after',
        '20',
    ]
    output = run(command, cwd=ROOT)
    missing = [marker for marker in REQUIRED_MARKERS if marker not in output]
    if missing:
        raise SystemExit('missing runtime markers: ' + ', '.join(missing))


if __name__ == '__main__':
    main()
