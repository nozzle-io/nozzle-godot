# nozzle-godot

Initial Godot 4 GDExtension implementation spike for nozzle diagnostics and honest texture-transfer status reporting.

This is **not** a Godot GPU texture sharing proof and it is **not** a zero-copy claim. The current implementation builds a real GDExtension, runs Godot headless runtime smoke from packaged example projects, exposes deterministic diagnostics, runs CPU oracle checks inside Godot, proves a Godot `ImageTexture` CPU-readback -> nozzle writable-frame sender -> independent nozzle receiver copy-out path on macOS CI, and keeps native/RD handle, GPU-copy, and zero-copy texture paths explicitly unproven.

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

Godot runtime smoke (Linux reports runtime backend availability; macOS requires the CPU-copy transfer oracle):

```bash
python3 scripts/run_godot_runtime_smoke.py
```

The runtime smoke downloads the official Godot 4.6.3 host binary, verifies SHA-256, then runs the packaged example project with `--headless --path build/package/nozzle-godot/project`. Linux hosted runners may report `MISSING_RUNTIME_BACKEND` for the DMA-BUF sender path because `/dev/dri/renderD128` is not available; macOS CI requires the actual CPU-copy transfer oracle to pass.

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
- CI build workflow for macOS, Linux, and Windows.
- Linux/macOS CI runtime smoke workflow using official Godot 4.6.3 binaries. The smoke loads the built GDExtension from the packaged example project, prints Godot version/platform/renderer/backend details, verifies required `NozzleDiagnostics` methods are registered, prints `NozzleDiagnostics.get_status_table()` with method-availability status, runs deterministic CPU oracle checks for `320x240` and `641x479`, creates real Godot `ImageTexture` objects, reads them back as `Image`, and on macOS requires publishing the CPU pixels through a nozzle writable-frame sender plus independent receiver copy-out oracle; on Linux hosted runners it records deterministic `MISSING_RUNTIME_BACKEND` when the DMA-BUF backend device is unavailable.

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
| GDExtension build | PASS_COMPILE_PACKAGE_ONLY after #176 CI | godot-cpp based shared library is built on macOS/Linux/Windows CI |
| Package shape | PASS_COMPILE_PACKAGE_ONLY | checker validates extension metadata, project files, library output, and zip root |
| Example project/scene | PASS on Linux/macOS CI after #176 CI | official Godot 4.6.3 binaries load the packaged project/GDExtension in headless mode |
| Public native/RD texture API surface | PUBLIC_API_SURFACE_COMPILES | C++ member signature probes compile against pinned godot-cpp headers |
| CPU fallback oracle | PASS on Linux/macOS CI after #176 CI | deterministic positive plus y-flip, R/B swap, alpha mutation, and byte-size negative probes run inside Godot |
| Zero-copy GPU interop | UNSUPPORTED_UNPROVEN | no zero-copy runtime evidence; do not claim support |

## Renderer/platform/copy-cost matrix

| Direction | Renderer | Platform | Runtime smoke | Transfer status | Copy cost | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| Godot `ImageTexture` CPU readback -> nozzle writable frame -> independent receiver | headless runtime report | macOS | PASS Godot runtime after #176 CI | PASS_CPU_COPY_RUNTIME_ORACLE after #176 CI | cpu-copy | runtime smoke creates real `ImageTexture` objects for `320x240` and `641x479`, reads them back as `Image`, publishes through nozzle writable frames, and validates independent receiver copy-out markers |
| Godot texture/render target -> nozzle | Forward+ | macOS/Linux/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no Forward+ runtime run |
| Godot texture/render target -> nozzle | Mobile | macOS/Linux/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no Mobile runtime run |
| Godot native/RD texture/render target -> nozzle | Compatibility | macOS/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | macOS runtime proof is CPU-readback only; no native/RD runtime run |
| nozzle -> Godot texture | Compatibility/headless runtime report | Linux | PASS extension load | MISSING_HOST_SMOKE | UNPROVEN | no independent nozzle sender -> Godot texture/Image oracle is executed |
| nozzle -> Godot texture | Forward+ | macOS/Linux/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no Forward+ runtime run |
| nozzle -> Godot texture | Mobile | macOS/Linux/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | API symbols compile; no Mobile runtime run |
| nozzle -> Godot texture | Compatibility | macOS/Windows | MISSING_RUNTIME_RUN | UNPROVEN_RUNTIME_TEXTURE_PATH | UNPROVEN | macOS runtime proof is opposite direction only |

## Follow-up required before support claims

1. Add a nozzle -> Godot texture/Image runtime oracle; current proof covers only Godot `ImageTexture` CPU readback -> nozzle writable frame -> receiver.
2. Add renderer-specific native/RD runtime runs for Forward+ and Mobile before claiming those paths.
3. Add Windows runtime runs before claiming host runtime support there; macOS currently covers only the CPU-readback direction.
4. Only after native/RD runtime proof, classify native texture paths as zero-copy or GPU-copy. The proven Godot `ImageTexture` readback path is CPU-copy.

## License

MIT
