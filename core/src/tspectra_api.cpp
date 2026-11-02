#include "tspectra/tspectra_api.h"
#include <cstdlib>
#include <new>

struct TSEngineContext {
    int placeholder{0};
};

TSStatus tspectra_context_create(TSContextHandle* out_ctx) {
    if (!out_ctx) {
        return TSPECTRA_ERR_INVALID_PARAM;
    }
    *out_ctx = new (std::nothrow) TSEngineContext();
    if (!(*out_ctx)) {
        return TSPECTRA_ERR_OUT_OF_MEMORY;
    }
    return TSPECTRA_OK;
}

void tspectra_context_destroy(TSContextHandle ctx) {
    delete ctx;
}

TSStatus tspectra_raw_load_file(
    TSContextHandle ctx,
    const char* utf8_file_path,
    TSImageBuffer* out_linear_image
) {
    if (!ctx || !utf8_file_path || !out_linear_image) {
        return TSPECTRA_ERR_INVALID_PARAM;
    }
    // Stub implementation for Step 0.1
    return TSPECTRA_OK;
}

void tspectra_buffer_free(TSImageBuffer* buffer) {
    if (!buffer) {
        return;
    }
    if (buffer->plane0) {
        std::free(buffer->plane0);
        buffer->plane0 = nullptr;
    }
    if (buffer->plane1) {
        std::free(buffer->plane1);
        buffer->plane1 = nullptr;
    }
    buffer->width = 0;
    buffer->height = 0;
    buffer->stride_bytes = 0;
}

TSStatus tspectra_lut3d_bake(
    TSContextHandle ctx,
    const char* profile_id,
    const TSAdjustments* adjustments,
    uint32_t lut_size,
    float* out_rgb_buffer
) {
    if (!ctx || !profile_id || !adjustments || !out_rgb_buffer || lut_size == 0) {
        return TSPECTRA_ERR_INVALID_PARAM;
    }
    // Stub implementation for Step 0.1
    return TSPECTRA_OK;
}

TSStatus tspectra_render_pipeline(
    TSContextHandle ctx,
    const TSImageBuffer* src_image,
    const char* profile_id,
    const TSAdjustments* adjustments,
    TSImageBuffer* dst_image
) {
    if (!ctx || !src_image || !profile_id || !adjustments || !dst_image) {
        return TSPECTRA_ERR_INVALID_PARAM;
    }
    // Stub implementation for Step 0.1
    return TSPECTRA_OK;
}
