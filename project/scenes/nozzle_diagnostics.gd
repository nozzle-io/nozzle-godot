extends Node

func _ready() -> void:
    var diagnostics := NozzleDiagnostics.new()
    var renderer_name := str(ProjectSettings.get_setting("rendering/renderer/rendering_method"))
    print("NOZZLE_GODOT_VERSION=", Engine.get_version_info())
    print("NOZZLE_GODOT_RUNTIME os=", OS.get_name(), " renderer=", renderer_name)
    print("NOZZLE_GODOT_STATUS=", diagnostics.get_status_table())
    print("NOZZLE_GODOT_PUBLIC_TEXTURE_API=", diagnostics.get_public_texture_api_surface())
    print("NOZZLE_GODOT_TEXTURE_PATH=", diagnostics.classify_texture_publish_path(renderer_name))
    print("NOZZLE_GODOT_CPU_ORACLE size=320x240 result=", diagnostics.run_cpu_pattern_oracle(320, 240))
    print("NOZZLE_GODOT_CPU_ORACLE size=641x479 result=", diagnostics.run_cpu_pattern_oracle(641, 479))
    print("NOZZLE_GODOT_RUNTIME_SMOKE PASS")
    get_tree().quit(0)
