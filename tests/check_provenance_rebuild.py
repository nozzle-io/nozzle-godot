#!/usr/bin/env python3
"""Prove a dependency update reaches the built library without clearing caches.

Makes an empty commit in deps/nozzle, so HEAD moves and the sources stay the same,
then rebuilds in the same SCons state. Restores the original commit before exiting,
so later packaging sees the true revision.

usage: check_provenance_rebuild.py PLATFORM
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / 'deps' / 'nozzle'
GODOT_CPP = ROOT / 'deps' / 'godot-cpp'
HEADER = ROOT / 'build' / 'generated' / 'nozzle_godot' / 'build_provenance.hpp'
LIBRARY_PATTERNS = {
    'macos': 'libnozzle_godot.macos.template_debug.framework/libnozzle_godot.macos.template_debug',
    'linux': 'libnozzle_godot.linux.template_debug.*.so',
    'windows': 'libnozzle_godot.windows.template_debug.*.dll',
}


def git(*args, cwd=CORE):
    return subprocess.run(['git', '-C', str(cwd), *args], check=True, text=True, stdout=subprocess.PIPE).stdout.strip()


def require_clean_detached_core():
    # The probe commit would absorb staged edits, and a branch would stay advanced.
    if git('status', '--porcelain'):
        raise SystemExit(f'{CORE} has local changes')
    if subprocess.run(['git', '-C', str(CORE), 'symbolic-ref', '-q', 'HEAD'], stdout=subprocess.DEVNULL).returncode == 0:
        raise SystemExit(f'{CORE} is on a branch; this check needs a detached submodule checkout')


def scons(platform):
    executable = shutil.which('scons')
    if executable is None:
        raise SystemExit('scons not found')
    completed = subprocess.run(
        [executable, f'platform={platform}', 'target=template_debug', '-j2'],
        cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    )
    print(completed.stdout, flush=True)
    if completed.returncode != 0:
        raise SystemExit(f'scons failed with {completed.returncode}')
    return completed.stdout


def library(platform):
    found = list((ROOT / 'project' / 'bin').glob(LIBRARY_PATTERNS[platform]))
    if len(found) != 1:
        raise SystemExit(f'expected one library for {platform}, found {found}')
    return found[0]


def require_embedded(platform, present, absent):
    data = library(platform).read_bytes()
    if present.encode() not in data:
        raise SystemExit(f'library does not contain {present}')
    if absent.encode() in data:
        raise SystemExit(f'library still contains {absent}')
    if present not in HEADER.read_text():
        raise SystemExit(f'{HEADER} does not contain {present}')


def require_header_matches_checkouts():
    # Windows has no runtime smoke, so check every field here too.
    values = dict(re.findall(r'k_(\w+) = "([^"]*)"', HEADER.read_text()))
    expected = {
        'nozzle_sha': git('rev-parse', 'HEAD'),
        'nozzle_dirty': 'false',
        'godot_cpp_sha': git('rev-parse', 'HEAD', cwd=GODOT_CPP),
        'godot_cpp_dirty': 'false',
    }
    wrong = {key: values.get(key) for key, value in expected.items() if values.get(key) != value}
    if wrong:
        raise SystemExit(f'{HEADER} does not match the checkouts: {wrong}')
    if values.get('godot_cpp_tag', 'unknown') == 'unknown':
        raise SystemExit('godot-cpp is not the baseline commit in SConstruct; update the baseline')


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in LIBRARY_PATTERNS:
        raise SystemExit(__doc__)
    platform = sys.argv[1]
    if not (CORE / '.git').exists():
        raise SystemExit(f'{CORE} is not its own git checkout')

    require_clean_detached_core()
    require_header_matches_checkouts()

    original = git('rev-parse', 'HEAD')
    git('-c', 'user.name=nozzle-ci', '-c', 'user.email=nozzle-ci@invalid',
        'commit', '-q', '--allow-empty', '-m', 'provenance probe')
    try:
        probe = git('rev-parse', 'HEAD')
        output = scons(platform)
        require_embedded(platform, probe, original)
        # The sources did not change, so core must not recompile or come from the cache.
        if re.search(r'ipc\.(cpp|o|os|obj)\b', output):
            raise SystemExit('a revision change recompiled core sources')

        stamps = (HEADER.stat().st_mtime_ns, library(platform).stat().st_mtime_ns)
        scons(platform)
        if stamps != (HEADER.stat().st_mtime_ns, library(platform).stat().st_mtime_ns):
            raise SystemExit('a no-op build changed the header or library')
    finally:
        git('checkout', '-q', '--detach', original)

    scons(platform)
    require_embedded(platform, original, probe)
    require_header_matches_checkouts()
    print(f'NOZZLE_GODOT_PROVENANCE_REBUILD status=PASS original={original} probe={probe}', flush=True)


if __name__ == '__main__':
    main()
