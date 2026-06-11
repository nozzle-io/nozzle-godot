#!/usr/bin/env python

import os

env = SConscript("deps/godot-cpp/SConstruct")
env.Append(CPPPATH=["include", "src", "deps/nozzle/include"])
env.Append(CXXFLAGS=["-std=c++17"] if env.get("platform") not in ["windows"] else ["/std:c++17"])

sources = Glob("src/*.cpp")

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "project/bin/libnozzle_godot.{}.{}.framework/libnozzle_godot.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
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
