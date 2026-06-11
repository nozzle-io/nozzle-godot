#include "nozzle_godot/nozzle_diagnostics.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/core/version.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "nozzle/nozzle_c.h"

namespace godot {
namespace {

constexpr const char *k_nozzle_sha = "a8efca3c847c39b76057a8e77f94b34146cc9125";
constexpr const char *k_godot_target = "4.6.3-stable";
constexpr const char *k_godot_cpp_tag = "10.0.0-rc1";
constexpr const char *k_godot_cpp_sha = "58d1de720b8ffe9f8ffcdfe3a85148582cfd2e74";

struct sender_deleter {
    void operator()(NozzleSender *sender) const noexcept {
        if (sender != nullptr) {
            nozzle_sender_destroy(sender);
        }
    }
};

struct receiver_deleter {
    void operator()(NozzleReceiver *receiver) const noexcept {
        if (receiver != nullptr) {
            nozzle_receiver_destroy(receiver);
        }
    }
};

struct frame_deleter {
    void operator()(NozzleFrame *frame) const noexcept {
        if (frame != nullptr) {
            nozzle_frame_release(frame);
        }
    }
};

Dictionary fail_result(const char *step, NozzleErrorCode code) {
    Dictionary out;
    out["status"] = "FAIL";
    out["step"] = step;
    out["error_code"] = static_cast<int32_t>(code);
    out["copy_cost"] = "cpu-copy";
    out["godot_texture_to_nozzle"] = "FAIL";
    out["nozzle_receiver_oracle"] = "FAIL";
    return out;
}

Dictionary fail_result(const char *reason) {
    Dictionary out;
    out["status"] = "FAIL";
    out["reason"] = reason;
    out["copy_cost"] = "cpu-copy";
    out["godot_texture_to_nozzle"] = "FAIL";
    out["nozzle_receiver_oracle"] = "FAIL";
    return out;
}

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

bool bytes_equal(const PackedByteArray &left, const PackedByteArray &right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (int64_t i = 0; i < left.size(); ++i) {
        if (left[static_cast<int>(i)] != right[static_cast<int>(i)]) {
            return false;
        }
    }
    return true;
}

bool matches_pattern(const PackedByteArray &bytes, int32_t width, int32_t height) {
    const PackedByteArray expected = make_pattern(width, height);
    if (expected.is_empty() || bytes.size() != expected.size()) {
        return false;
    }
    const int64_t probes[] = {
        0,
        static_cast<int64_t>(width - 1),
        static_cast<int64_t>(height - 1) * width,
        static_cast<int64_t>(height) * width - 1,
        static_cast<int64_t>(height / 2) * width + (width / 2)
    };
    for (int64_t pixel : probes) {
        const int64_t base = pixel * 4;
        for (int64_t channel = 0; channel < 4; ++channel) {
            const int index = static_cast<int>(base + channel);
            if (bytes[index] != expected[index]) {
                return false;
            }
        }
    }
    return true;
}

PackedByteArray y_flipped(PackedByteArray bytes, int32_t width, int32_t height) {
    const int64_t row_bytes = static_cast<int64_t>(width) * 4;
    for (int32_t y = 0; y < height / 2; ++y) {
        const int64_t top_base = static_cast<int64_t>(y) * row_bytes;
        const int64_t bottom_base = static_cast<int64_t>(height - 1 - y) * row_bytes;
        for (int64_t offset = 0; offset < row_bytes; ++offset) {
            const int top_index = static_cast<int>(top_base + offset);
            const int bottom_index = static_cast<int>(bottom_base + offset);
            const uint8_t top = bytes[top_index];
            bytes.set(top_index, bytes[bottom_index]);
            bytes.set(bottom_index, top);
        }
    }
    return bytes;
}

PackedByteArray rb_swapped(PackedByteArray bytes) {
    for (int64_t i = 0; i + 3 < bytes.size(); i += 4) {
        const uint8_t red = bytes[static_cast<int>(i + 0)];
        bytes.set(static_cast<int>(i + 0), bytes[static_cast<int>(i + 2)]);
        bytes.set(static_cast<int>(i + 2), red);
    }
    return bytes;
}

PackedByteArray alpha_mutated(PackedByteArray bytes) {
    if (7 < bytes.size()) {
        bytes.set(3, static_cast<uint8_t>(bytes[3] ^ 0xff));
    }
    return bytes;
}

} // namespace

void NozzleDiagnostics::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_nozzle_sha"), &NozzleDiagnostics::get_nozzle_sha);
    ClassDB::bind_method(D_METHOD("get_godot_target"), &NozzleDiagnostics::get_godot_target);
    ClassDB::bind_method(D_METHOD("get_status_table"), &NozzleDiagnostics::get_status_table);
    ClassDB::bind_method(D_METHOD("get_public_texture_api_surface"), &NozzleDiagnostics::get_public_texture_api_surface);
    ClassDB::bind_method(D_METHOD("classify_texture_publish_path", "renderer_name"), &NozzleDiagnostics::classify_texture_publish_path);
    ClassDB::bind_method(D_METHOD("make_cpu_pattern_oracle", "width", "height"), &NozzleDiagnostics::make_cpu_pattern_oracle);
    ClassDB::bind_method(D_METHOD("make_cpu_pattern_bytes", "width", "height"), &NozzleDiagnostics::make_cpu_pattern_bytes);
    ClassDB::bind_method(D_METHOD("run_cpu_pattern_oracle", "width", "height"), &NozzleDiagnostics::run_cpu_pattern_oracle);
    ClassDB::bind_method(D_METHOD("run_godot_image_to_nozzle_oracle", "image", "width", "height"), &NozzleDiagnostics::run_godot_image_to_nozzle_oracle);
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
    out["godot_runtime_smoke"] = "PASS";
    out["godot_texture_to_nozzle"] = "CPU_COPY_RUNTIME_ORACLE_METHOD_AVAILABLE";
    out["nozzle_to_godot_texture"] = "MISSING_HOST_SMOKE";
    out["cpu_copy_pattern_oracle"] = "PASS_RUNTIME_ORACLE_AVAILABLE";
    out["zero_copy_gpu_interop"] = "UNSUPPORTED_UNPROVEN";
    return out;
}

Dictionary NozzleDiagnostics::get_public_texture_api_surface() const {
    // Compile-time surface probe: these member signatures exist in the pinned
    // godot-cpp 10.0.0-rc1 generated headers. The runtime smoke loads this
    // extension and runs CPU oracle checks, but it deliberately does not call
    // Godot texture transfer APIs.
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
    out["boundary"] = "Public native/RD texture API symbols compile, but the runtime transfer proof uses Godot ImageTexture CPU readback plus nozzle writable-frame CPU-copy. It does not prove native/RD handle transfer, GPU-copy, or zero-copy.";
    return out;
}

Dictionary NozzleDiagnostics::classify_texture_publish_path(const String &renderer_name) const {
    Dictionary out;
    out["renderer"] = renderer_name;
    out["status"] = "CPU_COPY_RUNTIME_ORACLE_METHOD_AVAILABLE";
    out["copy_cost"] = "cpu-copy-if-runtime-oracle-passes";
    out["reason"] = "This reports only that the CPU-copy runtime oracle method is available. Only run_godot_image_to_nozzle_oracle() results may claim PASS for ImageTexture CPU readback into a nozzle writable-frame sender and independent receiver copy-out. Native/RD handle, GPU-copy, and zero-copy remain unproven.";
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

PackedByteArray NozzleDiagnostics::make_cpu_pattern_bytes(int32_t width, int32_t height) const {
    return make_pattern(width, height);
}

Dictionary NozzleDiagnostics::run_cpu_pattern_oracle(int32_t width, int32_t height) const {
    Dictionary out;
    const PackedByteArray pattern = make_pattern(width, height);
    if (pattern.is_empty()) {
        out["status"] = "FAIL";
        out["reason"] = "invalid dimensions or bounded test payload exceeded";
        return out;
    }

    PackedByteArray truncated = pattern;
    truncated.resize(pattern.size() - 1);
    const bool positive = bytes_equal(pattern, make_pattern(width, height));
    const bool detects_y_flip = !matches_pattern(y_flipped(pattern, width, height), width, height);
    const bool detects_rb_swap = !matches_pattern(rb_swapped(pattern), width, height);
    const bool detects_alpha = !matches_pattern(alpha_mutated(pattern), width, height);
    const bool detects_byte_size = !matches_pattern(truncated, width, height);
    const bool ok = positive && detects_y_flip && detects_rb_swap && detects_alpha && detects_byte_size;

    out["status"] = ok ? "PASS" : "FAIL";
    out["width"] = width;
    out["height"] = height;
    out["no_y_flip"] = detects_y_flip ? "PASS" : "FAIL";
    out["no_r_b_swap"] = detects_rb_swap ? "PASS" : "FAIL";
    out["alpha"] = detects_alpha ? "PASS" : "FAIL";
    out["byte_size_mismatch"] = detects_byte_size ? "PASS" : "FAIL";
    out["checksum"] = checksum(pattern);
    return out;
}

Dictionary NozzleDiagnostics::run_godot_image_to_nozzle_oracle(const Ref<Image> &image, int32_t width, int32_t height) const {
    if (image.is_null()) {
        return fail_result("image is null");
    }
    if (width <= 0 || height <= 0) {
        return fail_result("invalid dimensions");
    }
    if (image->get_width() != width || image->get_height() != height) {
        return fail_result("image dimensions do not match expected size");
    }
    if (image->get_format() != Image::FORMAT_RGBA8) {
        return fail_result("image format is not RGBA8");
    }

    const PackedByteArray source = image->get_data();
    const PackedByteArray expected = make_pattern(width, height);
    if (expected.is_empty() || source.size() != expected.size()) {
        return fail_result("image byte size does not match expected RGBA8 payload");
    }
    if (!bytes_equal(source, expected)) {
        return fail_result("Godot ImageTexture readback does not match deterministic source pattern");
    }

    static std::atomic<uint64_t> sequence{0};
    const uint64_t current_sequence = sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    const std::string name = "nozzle-godot-texture-oracle-" + std::to_string(width) + "x" + std::to_string(height) + "-" + std::to_string(current_sequence);

    NozzleSender *raw_sender = nullptr;
    NozzleSenderDesc sender_desc{};
    sender_desc.name = name.c_str();
    sender_desc.application_name = "nozzle-godot";
    sender_desc.ring_buffer_size = 3;
    sender_desc.fallback_flags = NOZZLE_FALLBACK_STORAGE_COMPATIBLE;
    sender_desc.fallback_flags_valid = 1;
    NozzleErrorCode code = nozzle_sender_create(&sender_desc, &raw_sender);
    if (code != NOZZLE_OK) {
        return fail_result("nozzle_sender_create", code);
    }
    std::unique_ptr<NozzleSender, sender_deleter> sender(raw_sender);

    NozzleFrame *raw_writable_frame = nullptr;
    code = nozzle_sender_acquire_writable_frame(sender.get(), static_cast<uint32_t>(width), static_cast<uint32_t>(height), NOZZLE_FORMAT_RGBA8_UNORM, &raw_writable_frame);
    if (code != NOZZLE_OK) {
        return fail_result("nozzle_sender_acquire_writable_frame", code);
    }
    std::unique_ptr<NozzleFrame, frame_deleter> writable_frame(raw_writable_frame);

    NozzlePixelMapping *write_mapping = nullptr;
    NozzleMappedPixels writable_pixels{};
    code = nozzle_frame_lock_writable_pixels_mapping_with_origin(writable_frame.get(), NOZZLE_ORIGIN_TOP_LEFT, &write_mapping, &writable_pixels);
    if (code != NOZZLE_OK) {
        nozzle_sender_discard_frame(sender.get(), writable_frame.get());
        return fail_result("nozzle_frame_lock_writable_pixels_mapping_with_origin", code);
    }

    const int64_t expected_row_bytes = static_cast<int64_t>(width) * 4;
    if (writable_pixels.data == nullptr || writable_pixels.width != static_cast<uint32_t>(width) || writable_pixels.height != static_cast<uint32_t>(height) || writable_pixels.row_stride_bytes < expected_row_bytes) {
        nozzle_pixel_mapping_unlock(&write_mapping);
        nozzle_sender_discard_frame(sender.get(), writable_frame.get());
        return fail_result("unexpected writable mapping layout");
    }
    if (writable_pixels.format != NOZZLE_FORMAT_RGBA8_UNORM && writable_pixels.format != NOZZLE_FORMAT_BGRA8_UNORM) {
        nozzle_pixel_mapping_unlock(&write_mapping);
        nozzle_sender_discard_frame(sender.get(), writable_frame.get());
        return fail_result("unexpected writable mapping format");
    }

    const uint8_t *source_bytes = source.ptr();
    auto *destination_bytes = static_cast<uint8_t *>(writable_pixels.data);
    for (int32_t y = 0; y < height; ++y) {
        const uint8_t *source_row = source_bytes + static_cast<int64_t>(y) * expected_row_bytes;
        uint8_t *destination_row = destination_bytes + static_cast<int64_t>(y) * writable_pixels.row_stride_bytes;
        if (writable_pixels.format == NOZZLE_FORMAT_RGBA8_UNORM) {
            std::memcpy(destination_row, source_row, static_cast<size_t>(expected_row_bytes));
        } else {
            for (int32_t x = 0; x < width; ++x) {
                const int64_t source_index = static_cast<int64_t>(x) * 4;
                destination_row[source_index + 0] = source_row[source_index + 2];
                destination_row[source_index + 1] = source_row[source_index + 1];
                destination_row[source_index + 2] = source_row[source_index + 0];
                destination_row[source_index + 3] = source_row[source_index + 3];
            }
        }
    }

    code = nozzle_pixel_mapping_unlock_checked(&write_mapping);
    if (code != NOZZLE_OK) {
        nozzle_sender_discard_frame(sender.get(), writable_frame.get());
        return fail_result("nozzle_pixel_mapping_unlock_checked", code);
    }
    code = nozzle_sender_commit_frame(sender.get(), writable_frame.get());
    if (code != NOZZLE_OK) {
        return fail_result("nozzle_sender_commit_frame", code);
    }
    writable_frame.reset();

    NozzleReceiver *raw_receiver = nullptr;
    NozzleReceiverDesc receiver_desc{};
    receiver_desc.name = name.c_str();
    receiver_desc.application_name = "nozzle-godot";
    receiver_desc.receive_mode = NOZZLE_RECEIVE_LATEST_ONLY;
    code = nozzle_receiver_create(&receiver_desc, &raw_receiver);
    if (code != NOZZLE_OK) {
        return fail_result("nozzle_receiver_create", code);
    }
    std::unique_ptr<NozzleReceiver, receiver_deleter> receiver(raw_receiver);

    NozzleFrame *raw_received_frame = nullptr;
    NozzleAcquireDesc acquire_desc{};
    acquire_desc.timeout_ms = 1000;
    code = nozzle_receiver_acquire_frame(receiver.get(), &acquire_desc, &raw_received_frame);
    if (code != NOZZLE_OK) {
        return fail_result("nozzle_receiver_acquire_frame", code);
    }
    std::unique_ptr<NozzleFrame, frame_deleter> received_frame(raw_received_frame);

    std::vector<uint8_t> undersized_received(static_cast<size_t>(expected.size() - 1));
    NozzleMappedPixels undersized_pixels{};
    code = nozzle_frame_copy_pixels_with_origin(
        received_frame.get(),
        NOZZLE_ORIGIN_TOP_LEFT,
        undersized_received.data(),
        undersized_received.size(),
        &undersized_pixels);
    const bool detects_byte_size = code == NOZZLE_ERROR_INVALID_ARGUMENT &&
        undersized_pixels.data == nullptr &&
        undersized_pixels.row_stride_bytes == 0 &&
        undersized_pixels.width == 0 &&
        undersized_pixels.height == 0;
    if (!detects_byte_size) {
        return fail_result("short receiver copy buffer was not rejected deterministically");
    }

    std::vector<uint8_t> received(static_cast<size_t>(expected.size()));
    NozzleMappedPixels copied_pixels{};
    code = nozzle_frame_copy_pixels_with_origin(received_frame.get(), NOZZLE_ORIGIN_TOP_LEFT, received.data(), received.size(), &copied_pixels);
    if (code != NOZZLE_OK) {
        return fail_result("nozzle_frame_copy_pixels_with_origin", code);
    }
    if (copied_pixels.width != static_cast<uint32_t>(width) || copied_pixels.height != static_cast<uint32_t>(height) || copied_pixels.origin != NOZZLE_ORIGIN_TOP_LEFT || copied_pixels.row_stride_bytes != expected_row_bytes) {
        return fail_result("unexpected copied mapping layout");
    }

    PackedByteArray normalized;
    normalized.resize(expected.size());
    for (int64_t i = 0; i < expected.size(); ++i) {
        normalized.set(static_cast<int>(i), received[static_cast<size_t>(i)]);
    }
    if (copied_pixels.format == NOZZLE_FORMAT_BGRA8_UNORM) {
        normalized = rb_swapped(normalized);
    } else if (copied_pixels.format != NOZZLE_FORMAT_RGBA8_UNORM) {
        return fail_result("unexpected copied pixel format");
    }

    const bool transfer_ok = bytes_equal(normalized, expected);
    Dictionary out;
    out["status"] = transfer_ok ? "PASS" : "FAIL";
    out["width"] = width;
    out["height"] = height;
    out["godot_texture_to_nozzle"] = transfer_ok ? "PASS" : "FAIL";
    out["nozzle_receiver_oracle"] = transfer_ok ? "PASS" : "FAIL";
    out["renderer_backend_support"] = "CPU_READBACK_ONLY";
    out["copy_cost"] = "cpu-copy";
    out["zero_copy"] = "UNPROVEN";
    out["gpu_copy"] = "UNPROVEN";
    out["mapped_format"] = copied_pixels.format == NOZZLE_FORMAT_BGRA8_UNORM ? "BGRA8_UNORM" : "RGBA8_UNORM";
    out["no_y_flip"] = transfer_ok ? "PASS" : "FAIL";
    out["no_r_b_swap"] = transfer_ok ? "PASS" : "FAIL";
    out["alpha"] = transfer_ok ? "PASS" : "FAIL";
    out["byte_size_mismatch"] = detects_byte_size ? "PASS" : "FAIL";
    out["checksum"] = checksum(expected);
    return out;
}

} // namespace godot
