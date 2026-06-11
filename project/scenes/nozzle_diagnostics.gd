extends Node

func _ready() -> void:
    var diagnostics := NozzleDiagnostics.new()
    print("NOZZLE_GODOT_STATUS=", diagnostics.get_status_table())
    print("NOZZLE_GODOT_PUBLIC_TEXTURE_API=", diagnostics.get_public_texture_api_surface())
    print("NOZZLE_GODOT_CPU_ORACLE=", diagnostics.make_cpu_pattern_oracle(320, 240))
    get_tree().quit(0)
