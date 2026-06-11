# nozzle-godot

Initial Godot 4 GDExtension implementation spike for nozzle diagnostics and honest texture-transfer status reporting.

This is **not** a Godot GPU texture sharing proof and it is **not** a zero-copy claim. The current implementation builds a real GDExtension and exposes deterministic diagnostics, a CPU oracle helper, and a compile-time probe for the pinned Godot public texture API surface. It does not run a Godot renderer in CI and does not prove any runtime texture transfer path.

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

## Implemented

- GDExtension entry point `nozzle_godot_library_init`.
- `NozzleDiagnostics` RefCounted class registered at scene initialization level.
- Example Godot project with `.gdextension`, scene, and script.
- Deterministic CPU RGBA oracle metadata for fallback smoke scaffolding.
- Compile-time public texture API surface probe for the pinned godot-cpp headers:
  - `RenderingServer::texture_create_from_native_handle` (`deps/godot-cpp/gen/include/godot_cpp/classes/rendering_server.hpp:835`)
  - `RenderingServer::texture_get_rd_texture` (`deps/godot-cpp/gen/include/godot_cpp/classes/rendering_server.hpp:852`)
  - `RenderingServer::texture_get_native_handle` (`deps/godot-cpp/gen/include/godot_cpp/classes/rendering_server.hpp:853`)
  - `Texture2DRD::get_texture_rd_rid` (`deps/godot-cpp/gen/include/godot_cpp/classes/texture2drd.hpp:50`)
- Package-shape check and zip output with exactly one top-level `nozzle-godot/` folder.
- CI build for macOS, Linux, and Windows.

## Status legend

- `PASS_COMPILE_PACKAGE_ONLY`: built and packaged; not a Godot runtime execution proof.
- `PRESENT_UNEXECUTED`: callable code exists, but CI does not execute it inside Godot.
- `PUBLIC_API_SURFACE_COMPILES`: pinned godot-cpp public API symbols compile.
- `UNPROVEN_RUNTIME_TEXTURE_PATH`: runtime renderer/backend handle semantics and nozzle compatibility are not proven.
- `MISSING_RUNTIME_SMOKE`: no Godot editor/runtime smoke has executed for that renderer/path.

## Build/package status

| Area | Status | Evidence boundary |
| --- | --- | --- |
| GDExtension build | PASS_COMPILE_PACKAGE_ONLY | godot-cpp based shared library builds on macOS/Linux/Windows CI |
| Package shape | PASS_COMPILE_PACKAGE_ONLY | checker validates extension metadata, project files, library output, and zip root |
| Example project/scene | PASS_COMPILE_PACKAGE_ONLY | project and diagnostic scene are included; not loaded by Godot in CI |
| Public native/RD texture API surface | PUBLIC_API_SURFACE_COMPILES | C++ member signature probes compile against pinned godot-cpp headers |
| CPU fallback oracle | PRESENT_UNEXECUTED | deterministic bounded RGBA checksum helper exists; CI does not execute it inside Godot |
| Zero-copy GPU interop | UNSUPPORTED_UNPROVEN | no zero-copy runtime evidence; do not claim support |

## Renderer/platform/copy-cost matrix

| Direction | Renderer | Platform | Runtime smoke | Transfer status | Copy cost | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| Godot texture/render target -> nozzle | Forward+ | macOS | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |
| Godot texture/render target -> nozzle | Forward+ | Linux | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |
| Godot texture/render target -> nozzle | Forward+ | Windows | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |
| Godot texture/render target -> nozzle | Mobile | macOS/Linux/Windows | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |
| Godot texture/render target -> nozzle | Compatibility | macOS/Linux/Windows | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |
| nozzle -> Godot texture | Forward+ | macOS | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |
| nozzle -> Godot texture | Forward+ | Linux | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |
| nozzle -> Godot texture | Forward+ | Windows | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |
| nozzle -> Godot texture | Mobile | macOS/Linux/Windows | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |
| nozzle -> Godot texture | Compatibility | macOS/Linux/Windows | MISSING_RUNTIME_SMOKE | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no renderer run |

## Follow-up required before support claims

1. Run a real Godot binary in CI or a controlled host environment and load the packaged GDExtension.
2. Record renderer backend (`Forward+`, `Mobile`, `Compatibility`) and platform.
3. Attempt a specific texture transfer direction with runtime code/log evidence using the public texture API surface above.
4. Only after runtime proof, classify copy cost as zero-copy, GPU-copy, or CPU-copy. Until then, keep copy cost `UNPROVEN`.

## License

MIT
