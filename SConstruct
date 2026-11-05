#!/usr/bin/env python

VariantDir("build/nozzle", "deps/nozzle", duplicate=0)


def nozzle_source(path):
    return path.replace("deps/nozzle/", "build/nozzle/", 1)


def read_nozzle_sources(group):
    list_path = "deps/nozzle/sources/{}.txt".format(group)
    with open(list_path) as list_file:
        entries = [line.strip() for line in list_file if line.strip()]
    if not entries:
        raise SystemExit("{} is empty".format(list_path))
    return ["deps/nozzle/" + entry for entry in entries]


def nozzle_sources_for_platform(platform):
    sources = read_nozzle_sources("common")
    if platform in ["macos", "windows", "linux"]:
        sources += read_nozzle_sources(platform)
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
