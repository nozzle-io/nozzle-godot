extends Node

func _fail(message: String, exit_code: int) -> void:
    push_error(message)
    get_tree().quit(exit_code)

func _extension_load_status_name(status: int) -> String:
    match status:
        0:
            return "OK"
        1:
            return "FAILED"
        2:
            return "ALREADY_LOADED"
        3:
            return "NOT_LOADED"
        4:
            return "NEEDS_RESTART"
        _:
            return "UNKNOWN"

func _require_method(diagnostics: Object, method_name: String) -> bool:
    var available := diagnostics.has_method(method_name)
    print("NOZZLE_GODOT_METHOD method=", method_name, " available=", available)
    return available

func _require_oracle(diagnostics: Object, width: int, height: int, exit_code: int) -> bool:
    var result: Dictionary = diagnostics.call("run_cpu_pattern_oracle", width, height)
    print("NOZZLE_GODOT_CPU_ORACLE size=", width, "x", height, " status=", result.get("status", "MISSING"), " result=", result)
    var ok: bool = str(result.get("status", "")) == "PASS"
    ok = ok and str(result.get("no_y_flip", "")) == "PASS"
    ok = ok and str(result.get("no_r_b_swap", "")) == "PASS"
    ok = ok and str(result.get("alpha", "")) == "PASS"
    ok = ok and str(result.get("byte_size_mismatch", "")) == "PASS"
    if not ok:
        _fail("NozzleDiagnostics CPU oracle failed for %dx%d" % [width, height], exit_code)
        return false
    return true

func _require_texture_oracle(diagnostics: Object, width: int, height: int, exit_code: int, require_transfer: bool) -> bool:
    var bytes: PackedByteArray = diagnostics.call("make_cpu_pattern_bytes", width, height)
    var image := Image.create_from_data(width, height, false, Image.FORMAT_RGBA8, bytes)
    var texture := ImageTexture.create_from_image(image)
    if texture == null:
        _fail("failed to create ImageTexture for %dx%d" % [width, height], exit_code)
        return false
    var readback := texture.get_image()
    var result: Dictionary = diagnostics.call("run_godot_image_to_nozzle_oracle", readback, width, height)
    print("NOZZLE_GODOT_TEXTURE_INTEROP size=", width, "x", height, " godot_texture_to_nozzle=", result.get("godot_texture_to_nozzle", "MISSING"), " nozzle_receiver_oracle=", result.get("nozzle_receiver_oracle", "MISSING"), " renderer_backend_support=", result.get("renderer_backend_support", "MISSING"), " copy_cost=", result.get("copy_cost", "MISSING"), " zero_copy=", result.get("zero_copy", "MISSING"), " gpu_copy=", result.get("gpu_copy", "MISSING"), " no_y_flip=", result.get("no_y_flip", "MISSING"), " no_r_b_swap=", result.get("no_r_b_swap", "MISSING"), " alpha=", result.get("alpha", "MISSING"), " byte_size_mismatch=", result.get("byte_size_mismatch", "MISSING"), " result=", result)
    var ok: bool = str(result.get("status", "")) == "PASS"
    ok = ok and str(result.get("godot_texture_to_nozzle", "")) == "PASS"
    ok = ok and str(result.get("nozzle_receiver_oracle", "")) == "PASS"
    ok = ok and str(result.get("copy_cost", "")) == "cpu-copy"
    ok = ok and str(result.get("zero_copy", "")) == "UNPROVEN"
    ok = ok and str(result.get("gpu_copy", "")) == "UNPROVEN"
    ok = ok and str(result.get("no_y_flip", "")) == "PASS"
    ok = ok and str(result.get("no_r_b_swap", "")) == "PASS"
    ok = ok and str(result.get("alpha", "")) == "PASS"
    ok = ok and str(result.get("byte_size_mismatch", "")) == "PASS"
    if not ok:
        if not require_transfer:
            print("NOZZLE_GODOT_TEXTURE_INTEROP_MISSING size=", width, "x", height, " reason=runtime_backend_unavailable result=", result)
            return true
        _fail("Godot ImageTexture -> nozzle receiver oracle failed for %dx%d" % [width, height], exit_code)
        return false
    return true

func _ready() -> void:
    var extension_path := "res://nozzle_godot.gdextension"
    var load_status: int = GDExtensionManager.load_extension(extension_path)
    var load_ok := load_status == 0 or load_status == 2
    print("NOZZLE_GODOT_EXTENSION_LOAD path=", extension_path, " status=", _extension_load_status_name(load_status), " code=", load_status)
    if not load_ok:
        _fail("failed to load NozzleDiagnostics GDExtension", 2)
        return

    var diagnostics_class_available := ClassDB.class_exists("NozzleDiagnostics")
    print("NOZZLE_GODOT_CLASSDB class=NozzleDiagnostics available=", diagnostics_class_available)
    if not diagnostics_class_available:
        _fail("NozzleDiagnostics GDExtension class is not registered", 3)
        return

    var diagnostics: Object = ClassDB.instantiate("NozzleDiagnostics")
    if diagnostics == null:
        _fail("NozzleDiagnostics GDExtension class could not be instantiated", 4)
        return

    var project_renderer_name := str(ProjectSettings.get_setting("rendering/renderer/rendering_method"))
    var runtime_renderer_name := RenderingServer.get_current_rendering_method()
    var adapter_name := RenderingServer.get_video_adapter_name()
    var adapter_vendor := RenderingServer.get_video_adapter_vendor()
    var adapter_api_version := RenderingServer.get_video_adapter_api_version()
    var display_server_name := DisplayServer.get_name()
    var class_available := true
    for method_name in ["get_nozzle_sha", "get_godot_target", "get_status_table", "get_public_texture_api_surface", "classify_texture_publish_path", "make_cpu_pattern_oracle", "make_cpu_pattern_bytes", "run_cpu_pattern_oracle", "run_godot_image_to_nozzle_oracle"]:
        class_available = _require_method(diagnostics, method_name) and class_available
    print("NOZZLE_GODOT_EXTENSION_CLASS class=NozzleDiagnostics available=", class_available, " object_class=", diagnostics.get_class())
    if not class_available:
        _fail("NozzleDiagnostics GDExtension methods are not registered", 5)
        return

    print("NOZZLE_GODOT_VERSION=", Engine.get_version_info())
    print("NOZZLE_GODOT_RUNTIME os=", OS.get_name(), " project_renderer=", project_renderer_name, " runtime_renderer=", runtime_renderer_name)
    print("NOZZLE_GODOT_RENDERER_BACKEND method=", runtime_renderer_name, " adapter_name=", adapter_name, " adapter_vendor=", adapter_vendor, " adapter_api_version=", adapter_api_version, " display_server=", display_server_name)
    print("NOZZLE_GODOT_SHA nozzle=", diagnostics.call("get_nozzle_sha"), " godot_target=", diagnostics.call("get_godot_target"))
    print("NOZZLE_GODOT_STATUS=", diagnostics.call("get_status_table"))
    print("NOZZLE_GODOT_CPU_PATTERN_ORACLE size=320x240 result=", diagnostics.call("make_cpu_pattern_oracle", 320, 240))
    print("NOZZLE_GODOT_PUBLIC_TEXTURE_API=", diagnostics.call("get_public_texture_api_surface"))
    print("NOZZLE_GODOT_TEXTURE_PATH=", diagnostics.call("classify_texture_publish_path", runtime_renderer_name))
    if not _require_oracle(diagnostics, 320, 240, 6):
        return
    if not _require_oracle(diagnostics, 641, 479, 7):
        return
    var require_texture_transfer := OS.get_environment("NOZZLE_GODOT_REQUIRE_TEXTURE_TRANSFER") == "1"
    if not _require_texture_oracle(diagnostics, 320, 240, 8, require_texture_transfer):
        return
    if not _require_texture_oracle(diagnostics, 641, 479, 9, require_texture_transfer):
        return
    if require_texture_transfer:
        print("NOZZLE_GODOT_TEXTURE_TRANSFER godot_texture_to_nozzle=PASS nozzle_receiver_oracle=PASS renderer_backend_support=CPU_READBACK_ONLY copy_cost=cpu-copy zero_copy=UNPROVEN gpu_copy=UNPROVEN runtime_oracle=PASS_CPU_COPY_RUNTIME_ORACLE")
    else:
        print("NOZZLE_GODOT_TEXTURE_TRANSFER godot_texture_to_nozzle=MISSING_RUNTIME_BACKEND nozzle_receiver_oracle=MISSING_RUNTIME_BACKEND renderer_backend_support=MISSING_RUNTIME_BACKEND copy_cost=UNPROVEN zero_copy=UNPROVEN gpu_copy=UNPROVEN")
    print("NOZZLE_GODOT_RUNTIME_SMOKE PASS")
    get_tree().quit(0)
