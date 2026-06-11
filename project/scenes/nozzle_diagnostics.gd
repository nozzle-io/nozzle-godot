extends NozzleDiagnostics

func _fail(message: String, exit_code: int) -> void:
    push_error(message)
    get_tree().quit(exit_code)

func _require_method(method_name: String) -> bool:
    var available := has_method(method_name)
    print("NOZZLE_GODOT_METHOD method=", method_name, " available=", available)
    return available

func _require_oracle(width: int, height: int, exit_code: int) -> bool:
    var result: Dictionary = run_cpu_pattern_oracle(width, height)
    print("NOZZLE_GODOT_CPU_ORACLE size=", width, "x", height, " status=", result.get("status", "MISSING"), " result=", result)
    var ok := result.get("status", "") == "PASS"
    ok = ok and result.get("no_y_flip", "") == "PASS"
    ok = ok and result.get("no_r_b_swap", "") == "PASS"
    ok = ok and result.get("alpha", "") == "PASS"
    ok = ok and result.get("byte_size_mismatch", "") == "PASS"
    if not ok:
        _fail("NozzleDiagnostics CPU oracle failed for %dx%d" % [width, height], exit_code)
        return false
    return true

func _ready() -> void:
    var project_renderer_name := str(ProjectSettings.get_setting("rendering/renderer/rendering_method"))
    var runtime_renderer_name := RenderingServer.get_current_rendering_method()
    var adapter_name := RenderingServer.get_video_adapter_name()
    var adapter_vendor := RenderingServer.get_video_adapter_vendor()
    var adapter_api_version := RenderingServer.get_video_adapter_api_version()
    var display_server_name := DisplayServer.get_name()
    var class_available := true
    for method_name in ["get_nozzle_sha", "get_godot_target", "get_status_table", "get_public_texture_api_surface", "classify_texture_publish_path", "make_cpu_pattern_oracle", "run_cpu_pattern_oracle"]:
        class_available = _require_method(method_name) and class_available
    print("NOZZLE_GODOT_EXTENSION_CLASS class=NozzleDiagnostics available=", class_available, " node_class=", get_class())
    if not class_available:
        _fail("NozzleDiagnostics GDExtension methods are not registered", 2)
        return

    print("NOZZLE_GODOT_VERSION=", Engine.get_version_info())
    print("NOZZLE_GODOT_RUNTIME os=", OS.get_name(), " project_renderer=", project_renderer_name, " runtime_renderer=", runtime_renderer_name)
    print("NOZZLE_GODOT_RENDERER_BACKEND method=", runtime_renderer_name, " adapter_name=", adapter_name, " adapter_vendor=", adapter_vendor, " adapter_api_version=", adapter_api_version, " display_server=", display_server_name)
    print("NOZZLE_GODOT_SHA nozzle=", get_nozzle_sha(), " godot_target=", get_godot_target())
    print("NOZZLE_GODOT_STATUS=", get_status_table())
    print("NOZZLE_GODOT_CPU_PATTERN_ORACLE size=320x240 result=", make_cpu_pattern_oracle(320, 240))
    print("NOZZLE_GODOT_PUBLIC_TEXTURE_API=", get_public_texture_api_surface())
    print("NOZZLE_GODOT_TEXTURE_PATH=", classify_texture_publish_path(runtime_renderer_name))
    if not _require_oracle(320, 240, 3):
        return
    if not _require_oracle(641, 479, 4):
        return
    print("NOZZLE_GODOT_RUNTIME_SMOKE PASS")
    get_tree().quit(0)
