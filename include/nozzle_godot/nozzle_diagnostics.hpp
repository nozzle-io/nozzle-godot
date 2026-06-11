#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class NozzleDiagnostics : public RefCounted {
    GDCLASS(NozzleDiagnostics, RefCounted)

protected:
    static void _bind_methods();

public:
    String get_nozzle_sha() const;
    String get_godot_target() const;
    Dictionary get_status_table() const;
    Dictionary get_public_texture_api_surface() const;
    Dictionary classify_texture_publish_path(const String &renderer_name) const;
    Dictionary make_cpu_pattern_oracle(int32_t width, int32_t height) const;
    Dictionary run_cpu_pattern_oracle(int32_t width, int32_t height) const;
};

} // namespace godot
