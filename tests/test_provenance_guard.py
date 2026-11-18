import unittest

from check_provenance_rebuild import require_no_core_actions


class ProvenanceGuardTest(unittest.TestCase):
    def test_rejects_windows_cache_entry_from_ci(self):
        line = r"Retrieved `build\nozzle\src\common\ipc.windows.template_debug.x86_64.obj' from cache"
        with self.assertRaises(SystemExit, msg=line):
            require_no_core_actions(line, 'windows')

    def test_rejects_core_actions(self):
        for platform, line in [
            ('linux', 'Compiling shared deps/nozzle/src/backends/linux/linux_texture.cpp ...'),
            ('linux', 'c++ -c -o build/nozzle/src/common/registry.os deps/nozzle/src/common/registry.cpp'),
            ('macos', 'Compiling build/nozzle/src/backends/metal/metal_backend.mm ...'),
            ('windows', r'Compiling shared deps\nozzle\src\backends\d3d11\d3d11_sync.cpp ...'),
            ('windows', r'cl /c "deps\nozzle\src\common\frame.cpp"'),
            ('linux', "Retrieved `build/nozzle/src/common/registry.os' from cache"),
            ('linux', "Retrieved `build/nozzle/src/c_api/nozzle_c.o' from cache"),
        ]:
            with self.subTest(line=line), self.assertRaises(SystemExit):
                require_no_core_actions(line, platform)

    def test_allows_diagnostics_and_relink(self):
        require_no_core_actions('\n'.join([
            'Compiling shared src/nozzle_diagnostics.cpp ...',
            "Retrieved `src/nozzle_diagnostics.os' from cache",
            'clang++ -shared -o libnozzle_godot.so build/nozzle/src/common/ipc.os',
            r'link /DLL build\nozzle\src\common\ipc.windows.template_debug.x86_64.obj',
        ]), 'linux')


if __name__ == '__main__':
    unittest.main(verbosity=2)
