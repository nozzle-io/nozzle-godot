# nozzle-godot

Initial Godot 4 GDExtension implementation spike for nozzle diagnostics and honest texture-transfer status reporting.

This is **not** a Godot GPU texture sharing proof and it is **not** a zero-copy claim. The current implementation builds a real GDExtension, runs a Linux Godot headless runtime smoke that loads the built extension from the packaged example project, exposes deterministic diagnostics, runs CPU oracle checks inside Godot, and keeps texture transfer paths explicitly unproven.

## Baseline

- Godot target: `4.6.3-stable`.
- Godot tag object: `7d41c59c457bd5a245092b4e7eb2d833e3b3f8c3`.
- Godot tag target commit: `35e80b3a8822a9df9be390814b62f44c0a9c69e8`.
- godot-cpp baseline: `10.0.0-rc1`, commit `58d1de720b8ffe9f8ffcdfe3a85148582cfd2e74`, synced with upstream 4.6-stable.
- nozzle submodule: `a8efca3c847c39b76057a8e77f94b34146cc9125`.

## Build

```bash
git submodule update --init --recursive
python3 -m pip install scons
scons platform=macos target=template_debug -j2
python3 scripts/package.py
python3 tests/check_package.py
```

Use `platform=linux` or `platform=windows` on those runners.

Linux-only Godot runtime smoke:

```bash
python3 scripts/run_godot_runtime_smoke.py
```

The runtime smoke downloads the official `Godot_v4.6.3-stable_linux.x86_64.zip`
binary, verifies SHA-256
`d0bc2113065e481c9c2c2b2c37daa4e8be3fe9e27f0ab9ab0b6096e9a37907f3`,
then runs the packaged example project with `--headless --path build/package/nozzle-godot/project`.

## Implemented

- GDExtension entry point `nozzle_godot_library_init`.
- `NozzleDiagnostics` Node class registered at scene initialization level.
- Example Godot project with `.gdextension`, scene, and script. The smoke scene uses a plain `Node` harness so the script can parse before native class registration, explicitly loads `res://nozzle_godot.gdextension` through `GDExtensionManager.load_extension`, then instantiates `NozzleDiagnostics` through `ClassDB` and verifies required native methods before printing PASS. This proves packaged GDExtension loading without depending on parse-time native base-class availability.
- Deterministic CPU RGBA oracle metadata for fallback smoke scaffolding.
- Compile-time public texture API surface probe for the pinned godot-cpp headers:
  - `RenderingServer::texture_create_from_native_handle` (`deps/godot-cpp/gen/include/godot_cpp/classes/rendering_server.hpp:835`)
  - `RenderingServer::texture_get_rd_texture` (`deps/godot-cpp/gen/include/godot_cpp/classes/rendering_server.hpp:852`)
  - `RenderingServer::texture_get_native_handle` (`deps/godot-cpp/gen/include/godot_cpp/classes/rendering_server.hpp:853`)
  - `Texture2DRD::get_texture_rd_rid` (`deps/godot-cpp/gen/include/godot_cpp/classes/texture2drd.hpp:50`)
- Package-shape check and zip output with exactly one top-level `nozzle-godot/` folder.
- CI build for macOS, Linux, and Windows.
- Linux CI runtime smoke using the official Godot 4.6.3 x86_64 binary. The smoke loads the built GDExtension from the packaged example project, prints Godot version/platform/renderer/backend details, verifies required `NozzleDiagnostics` methods are registered, prints `NozzleDiagnostics.get_status_table()`, runs deterministic CPU oracle checks for `320x240` and `641x479` including positive, y-flip, R/B swap, alpha, and byte-size probes, and exits deterministically.

## Status legend

- `PASS_COMPILE_PACKAGE_ONLY`: built and packaged; not a Godot runtime execution proof.
- `PRESENT_UNEXECUTED`: callable code exists, but CI does not execute it inside Godot.
- `PUBLIC_API_SURFACE_COMPILES`: pinned godot-cpp public API symbols compile.
- `UNPROVEN_RUNTIME_TEXTURE_PATH`: runtime renderer/backend handle semantics and nozzle compatibility are not proven.
- `MISSING_RUNTIME_RUN`: no Godot editor/runtime smoke has executed for that platform.
- `MISSING_HOST_SMOKE`: Godot runtime loaded the extension, but no actual texture/frame oracle was executed for that path.

## Build/package status

| Area | Status | Evidence boundary |
| --- | --- | --- |
| GDExtension build | PASS_COMPILE_PACKAGE_ONLY | godot-cpp based shared library builds on macOS/Linux/Windows CI |
| Package shape | PASS_COMPILE_PACKAGE_ONLY | checker validates extension metadata, project files, library output, and zip root |
| Example project/scene | PASS on Linux CI | official Godot 4.6.3 Linux binary loads the packaged project/GDExtension in headless mode |
| Public native/RD texture API surface | PUBLIC_API_SURFACE_COMPILES | C++ member signature probes compile against pinned godot-cpp headers |
| CPU fallback oracle | PASS on Linux CI | deterministic positive plus y-flip, R/B swap, alpha mutation, and byte-size negative probes run inside Godot |
| Zero-copy GPU interop | UNSUPPORTED_UNPROVEN | no zero-copy runtime evidence; do not claim support |

## Renderer/platform/copy-cost matrix

| Direction | Renderer | Platform | Runtime smoke | Transfer status | Copy cost | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| Godot texture/render target -> nozzle | Compatibility/headless runtime report | Linux | PASS extension load | MISSING_HOST_SMOKE | UNPROVEN | runtime smoke loads packaged extension and prints `NOZZLE_GODOT_RENDERER_BACKEND`, but no texture/frame oracle is executed |
| Godot texture/render target -> nozzle | Forward+ | macOS/Linux/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no Forward+ runtime run |
| Godot texture/render target -> nozzle | Mobile | macOS/Linux/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no Mobile runtime run |
| Godot texture/render target -> nozzle | Compatibility | macOS/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | no macOS/Windows Godot runtime run |
| nozzle -> Godot texture | Compatibility/headless runtime report | Linux | PASS extension load | MISSING_HOST_SMOKE | UNPROVEN | runtime smoke loads packaged extension and prints `NOZZLE_GODOT_RENDERER_BACKEND`, but no texture/frame oracle is executed |
| nozzle -> Godot texture | Forward+ | macOS/Linux/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no Forward+ runtime run |
| nozzle -> Godot texture | Mobile | macOS/Linux/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no Mobile runtime run |
| nozzle -> Godot texture | Compatibility | macOS/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | no macOS/Windows Godot runtime run |

## Follow-up required before support claims

1. Attempt a specific texture transfer direction with runtime code/log evidence using the public texture API surface above.
2. Add renderer-specific runtime runs for Forward+ and Mobile before claiming those paths.
3. Add macOS/Windows runtime runs before claiming host runtime support there.
4. Only after runtime proof, classify copy cost as zero-copy, GPU-copy, or CPU-copy. Until then, keep copy cost `UNPROVEN`.

## License

MIT
