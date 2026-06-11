#include "nozzle_godot/nozzle_diagnostics.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/core/version.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "nozzle/nozzle_c.h"

namespace godot {
namespace {

constexpr const char *k_nozzle_sha = "a8efca3c847c39b76057a8e77f94b34146cc9125";
constexpr const char *k_godot_target = "4.6.3-stable";
constexpr const char *k_godot_cpp_tag = "10.0.0-rc1";
constexpr const char *k_godot_cpp_sha = "58d1de720b8ffe9f8ffcdfe3a85148582cfd2e74";

PackedByteArray make_pattern(int32_t width, int32_t height) {
    PackedByteArray out;
    if (width <= 0 || height <= 0) {
        return out;
    }
    const int64_t byte_count = static_cast<int64_t>(width) * static_cast<int64_t>(height) * 4;
    if (byte_count <= 0 || byte_count > 64 * 1024 * 1024) {
        return out;
    }
    out.resize(static_cast<int>(byte_count));
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            const int64_t base = (static_cast<int64_t>(y) * width + x) * 4;
            out.set(static_cast<int>(base + 0), static_cast<uint8_t>(x == 0 ? 255 : (x == width - 1 ? 32 : ((x * 251 + y * 17) & 0xff))));
            out.set(static_cast<int>(base + 1), static_cast<uint8_t>(y == 0 ? 64 : (y == height - 1 ? 255 : ((x * 19 + y * 241) & 0xff))));
            out.set(static_cast<int>(base + 2), static_cast<uint8_t>((x == width / 2 && y == height / 2) ? 255 : ((x * 73 + y * 37) & 0xff)));
            const int alpha_case = (x + y) % 3;
            out.set(static_cast<int>(base + 3), static_cast<uint8_t>(alpha_case == 0 ? 0 : (alpha_case == 1 ? 128 : 255)));
        }
    }
    return out;
}

int64_t checksum(const PackedByteArray &bytes) {
    int64_t value = 1469598103934665603ULL;
    for (int64_t i = 0; i < bytes.size(); ++i) {
        value ^= static_cast<uint8_t>(bytes[static_cast<int>(i)]);
        value *= 1099511628211ULL;
    }
    return value;
}

} // namespace

void NozzleDiagnostics::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_nozzle_sha"), &NozzleDiagnostics::get_nozzle_sha);
    ClassDB::bind_method(D_METHOD("get_godot_target"), &NozzleDiagnostics::get_godot_target);
    ClassDB::bind_method(D_METHOD("get_status_table"), &NozzleDiagnostics::get_status_table);
    ClassDB::bind_method(D_METHOD("get_public_texture_api_surface"), &NozzleDiagnostics::get_public_texture_api_surface);
    ClassDB::bind_method(D_METHOD("classify_texture_publish_path", "renderer_name"), &NozzleDiagnostics::classify_texture_publish_path);
    ClassDB::bind_method(D_METHOD("make_cpu_pattern_oracle", "width", "height"), &NozzleDiagnostics::make_cpu_pattern_oracle);
}

String NozzleDiagnostics::get_nozzle_sha() const {
    return k_nozzle_sha;
}

String NozzleDiagnostics::get_godot_target() const {
    return k_godot_target;
}

Dictionary NozzleDiagnostics::get_status_table() const {
    static_assert(NOZZLE_OK == 0, "unexpected nozzle C ABI NOZZLE_OK value");
    Dictionary out;
    out["godot_target"] = k_godot_target;
    out["godot_cpp_tag"] = k_godot_cpp_tag;
    out["godot_cpp_sha"] = k_godot_cpp_sha;
    out["nozzle_sha"] = k_nozzle_sha;
    out["gdextension_build"] = "PASS";
    out["godot_runtime_smoke"] = "MISSING_RUNTIME_SMOKE";
    out["godot_texture_to_nozzle"] = "UNPROVEN_RUNTIME_TEXTURE_PATH";
    out["nozzle_to_godot_texture"] = "UNPROVEN_RUNTIME_TEXTURE_PATH";
    out["cpu_copy_pattern_oracle"] = "PRESENT_UNEXECUTED";
    out["zero_copy_gpu_interop"] = "UNSUPPORTED_UNPROVEN";
    return out;
}

Dictionary NozzleDiagnostics::get_public_texture_api_surface() const {
    // Compile-time surface probe: these member signatures exist in the pinned
    // godot-cpp 10.0.0-rc1 generated headers. This deliberately does not call
    // them because no Godot renderer/runtime smoke is running in CI.
    using texture_get_native_handle_signature = uint64_t (RenderingServer::*)(const RID &, bool) const;
    using texture_get_rd_texture_signature = RID (RenderingServer::*)(const RID &, bool) const;
    using texture_create_from_native_handle_signature = RID (RenderingServer::*)(RenderingServer::TextureType, Image::Format, uint64_t, int32_t, int32_t, int32_t, int32_t, RenderingServer::TextureLayeredType);
    using texture2drd_get_texture_rd_rid_signature = RID (Texture2DRD::*)() const;

    [[maybe_unused]] texture_get_native_handle_signature texture_get_native_handle_probe = &RenderingServer::texture_get_native_handle;
    [[maybe_unused]] texture_get_rd_texture_signature texture_get_rd_texture_probe = &RenderingServer::texture_get_rd_texture;
    [[maybe_unused]] texture_create_from_native_handle_signature texture_create_from_native_handle_probe = &RenderingServer::texture_create_from_native_handle;
    [[maybe_unused]] texture2drd_get_texture_rd_rid_signature texture2drd_get_texture_rd_rid_probe = &Texture2DRD::get_texture_rd_rid;

    Dictionary out;
    out["status"] = "PUBLIC_API_SURFACE_COMPILES";
    out["runtime_transfer_status"] = "UNPROVEN_RUNTIME_TEXTURE_PATH";
    out["copy_cost"] = "UNPROVEN";
    out["rendering_server_texture_get_native_handle"] = "deps/godot-cpp/gen/include/godot_cpp/classes/rendering_server.hpp:853";
    out["rendering_server_texture_create_from_native_handle"] = "deps/godot-cpp/gen/include/godot_cpp/classes/rendering_server.hpp:835";
    out["rendering_server_texture_get_rd_texture"] = "deps/godot-cpp/gen/include/godot_cpp/classes/rendering_server.hpp:852";
    out["texture2drd_get_texture_rd_rid"] = "deps/godot-cpp/gen/include/godot_cpp/classes/texture2drd.hpp:50";
    out["boundary"] = "Public native/RD texture API symbols compile, but this spike has not run Godot or proven that a backend handle can be mapped to a nozzle backend texture on any platform.";
    return out;
}

Dictionary NozzleDiagnostics::classify_texture_publish_path(const String &renderer_name) const {
    Dictionary out;
    out["renderer"] = renderer_name;
    out["status"] = "UNPROVEN_RUNTIME_TEXTURE_PATH";
    out["copy_cost"] = "UNPROVEN";
    out["reason"] = "Pinned godot-cpp exposes native/RD texture API symbols, but no runtime renderer smoke has proven that this renderer returns a backend handle usable by nozzle or accepts a nozzle-owned handle.";
    return out;
}

Dictionary NozzleDiagnostics::make_cpu_pattern_oracle(int32_t width, int32_t height) const {
    Dictionary out;
    const PackedByteArray pattern = make_pattern(width, height);
    if (pattern.is_empty()) {
        out["status"] = "FAIL";
        out["reason"] = "invalid dimensions or bounded test payload exceeded";
        return out;
    }
    out["status"] = "PASS";
    out["width"] = width;
    out["height"] = height;
    out["byte_count"] = pattern.size();
    out["checksum"] = checksum(pattern);
    out["detects"] = "deterministic asymmetric RGBA pattern for CPU-copy fallback smoke; not texture interop evidence";
    return out;
}

} // namespace godot
