#!/usr/bin/env python

VariantDir("build/nozzle", "deps/nozzle", duplicate=0)


def nozzle_source(path):
    return path.replace("deps/nozzle/", "build/nozzle/", 1)


def nozzle_sources_for_platform(platform):
    sources = [
        "deps/nozzle/src/common/ipc.cpp",
        "deps/nozzle/src/common/registry.cpp",
        "deps/nozzle/src/common/sender.cpp",
        "deps/nozzle/src/common/receiver.cpp",
        "deps/nozzle/src/common/frame.cpp",
        "deps/nozzle/src/common/texture.cpp",
        "deps/nozzle/src/common/device.cpp",
        "deps/nozzle/src/common/discovery.cpp",
        "deps/nozzle/src/common/metadata.cpp",
        "deps/nozzle/src/common/pixel_access.cpp",
        "deps/nozzle/src/common/channel_swizzle.cpp",
        "deps/nozzle/src/common/format_convert.cpp",
        "deps/nozzle/src/common/format_resolve.cpp",
        "deps/nozzle/src/common/backend_capabilities.cpp",
        "deps/nozzle/src/common/format_convert_sse2.cpp",
        "deps/nozzle/src/common/format_convert_f16c.cpp",
        "deps/nozzle/src/common/format_convert_neon.cpp",
        "deps/nozzle/src/c_api/nozzle_c.cpp",
    ]
    if platform == "macos":
        sources += [
            "deps/nozzle/src/backends/metal/metal_backend.mm",
            "deps/nozzle/src/backends/metal/metal_texture.mm",
            "deps/nozzle/src/backends/metal/metal_channel_swap.mm",
            "deps/nozzle/src/backends/metal/metal_sync.mm",
            "deps/nozzle/src/common/channel_swizzle_vimage.cpp",
            "deps/nozzle/src/common/format_convert_vimage.cpp",
        ]
    elif platform == "windows":
        sources += [
            "deps/nozzle/src/backends/d3d11/d3d11_backend.cpp",
            "deps/nozzle/src/backends/d3d11/d3d11_texture.cpp",
            "deps/nozzle/src/backends/d3d11/d3d11_sync.cpp",
        ]
    elif platform == "linux":
        sources += [
            "deps/nozzle/src/backends/linux/linux_texture.cpp",
        ]
    return [nozzle_source(source) for source in sources]


env = SConscript("deps/godot-cpp/SConstruct")
platform = env.get("platform")
env.Append(CPPPATH=["include", "src", "deps/nozzle/include", "deps/nozzle/src", "deps/nozzle/libs/plog/include"])
env.Append(CXXFLAGS=["-std=c++17"] if platform not in ["windows"] else ["/std:c++17"])

if platform == "macos":
    env.Append(CPPDEFINES=["NOZZLE_PLATFORM_MACOS=1", "NOZZLE_HAS_METAL=1"])
    env.Append(LINKFLAGS=["-framework", "Metal", "-framework", "IOSurface", "-framework", "Foundation", "-framework", "Accelerate"])
    env.Append(LIBS=["objc"])
elif platform == "windows":
    env.Append(CPPDEFINES=["NOZZLE_PLATFORM_WINDOWS=1", "NOZZLE_HAS_D3D11=1"])
    env.Append(LIBS=["d3d11", "dxgi", "bcrypt"])
elif platform == "linux":
    env.Append(CPPDEFINES=["NOZZLE_PLATFORM_LINUX=1", "NOZZLE_HAS_DMA_BUF=1"])
    env.ParseConfig("pkg-config --cflags --libs libdrm gbm egl")

sources = Glob("src/*.cpp") + nozzle_sources_for_platform(platform)

if platform == "macos":
    library = env.SharedLibrary(
        "project/bin/libnozzle_godot.{}.{}.framework/libnozzle_godot.{}.{}".format(
            platform, env["target"], platform, env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "project/bin/libnozzle_godot{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

env.NoCache(library)
Default(library)
