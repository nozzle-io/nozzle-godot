#!/usr/bin/env python3
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import urllib.request
import zipfile
import filecmp
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
MACOS_ZIP_NAME = 'Godot_v4.6.3-stable_macos.universal.zip'
MACOS_URL = f'https://github.com/godotengine/godot/releases/download/{GODOT_VERSION}/{MACOS_ZIP_NAME}'
MACOS_SHA256 = '30630f3e9b11e10b35c1f90ba8814185dcec43fae1a48345159be7552c64bfe8'

COMMON_REQUIRED_MARKERS = [
    'NOZZLE_GODOT_BINARY version=4.6.3-stable tag=7d41c59c457bd5a245092b4e7eb2d833e3b3f8c3 target=35e80b3a8822a9df9be390814b62f44c0a9c69e8 sha256=',
    'NOZZLE_GODOT_PROJECT path=',
    'NOZZLE_GODOT_EXTENSION_LOAD path=res://nozzle_godot.gdextension status=',
    'NOZZLE_GODOT_CLASSDB class=NozzleDiagnostics available=true',
    'NOZZLE_GODOT_EXTENSION_CLASS class=NozzleDiagnostics available=true',
    'NOZZLE_GODOT_VERSION=',
    'NOZZLE_GODOT_RUNTIME os=',
    'NOZZLE_GODOT_RENDERER_BACKEND method=',
    'NOZZLE_GODOT_SHA nozzle=',
    'NOZZLE_GODOT_STATUS=',
    'NOZZLE_GODOT_METHOD method=get_nozzle_sha available=true',
    'NOZZLE_GODOT_METHOD method=get_godot_target available=true',
    'NOZZLE_GODOT_METHOD method=get_status_table available=true',
    'NOZZLE_GODOT_METHOD method=get_public_texture_api_surface available=true',
    'NOZZLE_GODOT_METHOD method=classify_texture_publish_path available=true',
    'NOZZLE_GODOT_METHOD method=make_cpu_pattern_oracle available=true',
    'NOZZLE_GODOT_METHOD method=make_cpu_pattern_bytes available=true',
    'NOZZLE_GODOT_METHOD method=run_cpu_pattern_oracle available=true',
    'NOZZLE_GODOT_METHOD method=run_godot_image_to_nozzle_oracle available=true',
    'NOZZLE_GODOT_PUBLIC_TEXTURE_API=',
    'NOZZLE_GODOT_TEXTURE_PATH=',
    'NOZZLE_GODOT_CPU_PATTERN_ORACLE size=320x240',
    'NOZZLE_GODOT_CPU_ORACLE size=320x240 status=PASS',
    'NOZZLE_GODOT_CPU_ORACLE size=641x479 status=PASS',
    'NOZZLE_GODOT_RUNTIME_SMOKE PASS',
    'godot_runtime_smoke',
    'godot_texture_to_nozzle',
    'nozzle_to_godot_texture',
    'CPU_COPY_RUNTIME_ORACLE_METHOD_AVAILABLE',
    'MISSING_HOST_SMOKE',
    'no_y_flip',
    'no_r_b_swap',
    'alpha',
    'byte_size_mismatch',
    'copy_cost',
    'UNPROVEN',
]

TRANSFER_REQUIRED_MARKERS = [
    'NOZZLE_GODOT_TEXTURE_INTEROP size=320x240 godot_texture_to_nozzle=PASS nozzle_receiver_oracle=PASS renderer_backend_support=CPU_READBACK_ONLY copy_cost=cpu-copy zero_copy=UNPROVEN gpu_copy=UNPROVEN no_y_flip=PASS no_r_b_swap=PASS alpha=PASS byte_size_mismatch=PASS',
    'NOZZLE_GODOT_TEXTURE_INTEROP size=641x479 godot_texture_to_nozzle=PASS nozzle_receiver_oracle=PASS renderer_backend_support=CPU_READBACK_ONLY copy_cost=cpu-copy zero_copy=UNPROVEN gpu_copy=UNPROVEN no_y_flip=PASS no_r_b_swap=PASS alpha=PASS byte_size_mismatch=PASS',
    'NOZZLE_GODOT_TEXTURE_TRANSFER godot_texture_to_nozzle=PASS nozzle_receiver_oracle=PASS renderer_backend_support=CPU_READBACK_ONLY copy_cost=cpu-copy zero_copy=UNPROVEN gpu_copy=UNPROVEN',
    'PASS_CPU_COPY_RUNTIME_ORACLE',
]

MISSING_BACKEND_REQUIRED_MARKERS = [
    'NOZZLE_GODOT_TEXTURE_INTEROP_MISSING size=320x240 reason=runtime_backend_unavailable',
    'NOZZLE_GODOT_TEXTURE_INTEROP_MISSING size=641x479 reason=runtime_backend_unavailable',
    'NOZZLE_GODOT_TEXTURE_TRANSFER godot_texture_to_nozzle=MISSING_RUNTIME_BACKEND nozzle_receiver_oracle=MISSING_RUNTIME_BACKEND renderer_backend_support=MISSING_RUNTIME_BACKEND copy_cost=UNPROVEN zero_copy=UNPROVEN gpu_copy=UNPROVEN',
]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def run(cmd, cwd=None, env=None):
    print('+ ' + ' '.join(str(part) for part in cmd), flush=True)
    completed = subprocess.run(cmd, cwd=cwd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    print(completed.stdout, end='', flush=True)
    if completed.returncode != 0:
        raise SystemExit(f'command failed with exit code {completed.returncode}: {cmd}')
    return completed.stdout


def ensure_godot_archive(zip_name, url, expected_sha256, extract_name, executable_relative_path):
    TOOLS.mkdir(parents=True, exist_ok=True)
    zip_path = TOOLS / zip_name
    if not zip_path.exists() or sha256(zip_path) != expected_sha256:
        print(f'downloading {url}', flush=True)
        urllib.request.urlretrieve(url, zip_path)
    digest = sha256(zip_path)
    if digest != expected_sha256:
        raise SystemExit(f'Godot zip sha256 mismatch: {digest}')

    extract_dir = TOOLS / extract_name
    executable = extract_dir / executable_relative_path
    if not executable.exists():
        if extract_dir.exists():
            shutil.rmtree(extract_dir)
        extract_dir.mkdir(parents=True)
        with zipfile.ZipFile(zip_path) as archive:
            archive.extractall(extract_dir)
        if not executable.exists():
            raise SystemExit('Godot executable not found after extraction')
        executable.chmod(executable.stat().st_mode | 0o111)
    binary_marker = f'NOZZLE_GODOT_BINARY version={GODOT_VERSION} tag={GODOT_TAG_SHA} target={GODOT_TARGET_SHA} sha256={expected_sha256} path={executable}'
    print(binary_marker, flush=True)
    return executable, binary_marker


def ensure_godot_for_host():
    host = platform.system()
    if host == 'Linux':
        return ensure_godot_archive(
            LINUX_ZIP_NAME,
            LINUX_URL,
            LINUX_SHA256,
            'godot-4.6.3-linux-x86_64',
            Path('Godot_v4.6.3-stable_linux.x86_64'),
        ), False
    if host == 'Darwin':
        return ensure_godot_archive(
            MACOS_ZIP_NAME,
            MACOS_URL,
            MACOS_SHA256,
            'godot-4.6.3-macos-universal',
            Path('Godot.app/Contents/MacOS/Godot'),
        ), True
    raise SystemExit(f'run_godot_runtime_smoke.py does not support {host}')

def require_packaged_project_current(project_path):
    source_files = [
        'project/project.godot',
        'project/nozzle_godot.gdextension',
        'project/scenes/nozzle_diagnostics.tscn',
        'project/scenes/nozzle_diagnostics.gd',
    ]
    for relative in source_files:
        source = ROOT / relative
        packaged = project_path / Path(relative).relative_to('project')
        if not packaged.exists():
            raise SystemExit(f'packaged project is missing {packaged}')
        if not filecmp.cmp(source, packaged, shallow=False):
            raise SystemExit(f'packaged project is stale or differs from source: {relative}')

    libs = [path for path in (project_path / 'bin').rglob('*') if path.is_file()]
    if not libs:
        raise SystemExit(f'packaged project has no GDExtension library under {project_path / "bin"}')


def main():
    (executable, binary_marker), require_transfer = ensure_godot_for_host()
    project_path = BUILD / 'package' / 'nozzle-godot' / 'project'
    if not project_path.exists():
        raise SystemExit(f'packaged Godot project not found: {project_path}')
    require_packaged_project_current(project_path)
    project_marker = f'NOZZLE_GODOT_PROJECT path={project_path}'
    print(project_marker, flush=True)
    command = [
        str(executable),
        '--headless',
        '--path',
        str(project_path),
        '--quit-after',
        '20',
    ]
    env = os.environ.copy()
    env['NOZZLE_GODOT_REQUIRE_TEXTURE_TRANSFER'] = '1' if require_transfer else '0'
    output = binary_marker + '\n' + project_marker + '\n' + run(command, cwd=ROOT, env=env)
    required_markers = COMMON_REQUIRED_MARKERS + (TRANSFER_REQUIRED_MARKERS if require_transfer else MISSING_BACKEND_REQUIRED_MARKERS)
    missing = [marker for marker in required_markers if marker not in output]
    if missing:
        raise SystemExit('missing runtime markers: ' + ', '.join(missing))


if __name__ == '__main__':
    main()
