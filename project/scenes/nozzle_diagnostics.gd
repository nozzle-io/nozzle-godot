extends Node

func _ready() -> void:
    var renderer_name := str(ProjectSettings.get_setting("rendering/renderer/rendering_method"))
    var class_available := ClassDB.class_exists("NozzleDiagnostics")
    print("NOZZLE_GODOT_EXTENSION_CLASS class=NozzleDiagnostics available=", class_available)
    if not class_available:
        push_error("NozzleDiagnostics GDExtension class is not registered")
        get_tree().quit(2)
        return

    var diagnostics := ClassDB.instantiate("NozzleDiagnostics")
    if diagnostics == null:
        push_error("NozzleDiagnostics instantiation returned null")
        get_tree().quit(3)
        return

    print("NOZZLE_GODOT_VERSION=", Engine.get_version_info())
    print("NOZZLE_GODOT_RUNTIME os=", OS.get_name(), " renderer=", renderer_name)
    print("NOZZLE_GODOT_STATUS=", diagnostics.get_status_table())
    print("NOZZLE_GODOT_PUBLIC_TEXTURE_API=", diagnostics.get_public_texture_api_surface())
    print("NOZZLE_GODOT_TEXTURE_PATH=", diagnostics.classify_texture_publish_path(renderer_name))
    print("NOZZLE_GODOT_CPU_ORACLE size=320x240 result=", diagnostics.run_cpu_pattern_oracle(320, 240))
    print("NOZZLE_GODOT_CPU_ORACLE size=641x479 result=", diagnostics.run_cpu_pattern_oracle(641, 479))
    print("NOZZLE_GODOT_RUNTIME_SMOKE PASS")
    get_tree().quit(0)
