#ifndef TSPECTRA_API_H
#define TSPECTRA_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
    #define TSPECTRA_EXPORT __declspec(dllexport)
#else
    #define TSPECTRA_EXPORT __attribute__((visibility("default")))
#endif

typedef enum {
    TSPECTRA_OK = 0,
    TSPECTRA_ERR_INVALID_PARAM = -1,
    TSPECTRA_ERR_FILE_NOT_FOUND = -2,
    TSPECTRA_ERR_DECODE_FAILED = -3,
    TSPECTRA_ERR_GPU_BACKEND = -4,
    TSPECTRA_ERR_OUT_OF_MEMORY = -5
} TSStatus;

typedef enum {
    TSPECTRA_FMT_RGB_F32 = 0,      // 32-bit 浮点线性通道 (用于高精度冲洗与离线计算)
    TSPECTRA_FMT_RGBA_U8 = 1,      // 8-bit sRGB / Display P3 预览缓冲
    TSPECTRA_FMT_YUV420_10BIT = 2  // 移动端取景器 10-bit YUV 流
} TSPixelFormat;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t stride_bytes;
    TSPixelFormat format;
    void* plane0;                  // 颜色平面或 Y 平面
    void* plane1;                  // UV 平面 (仅限 YUV 格式)
} TSImageBuffer;

typedef struct {
    float exposure_ev;             // 曝光补偿 (-3.0EV ~ +3.0EV)
    float halation_intensity;      // 防光晕散射强度 (0.0 ~ 1.0)
    float grain_scale;             // 胶片颗粒聚集尺度 (0.0 ~ 1.0)
    float paper_contrast;          // 相纸放大反差系数 (0.5 ~ 2.0)
} TSAdjustments;

// 不透明引擎上下文句柄
typedef struct TSEngineContext* TSContextHandle;

// 上下文生命周期
TSPECTRA_EXPORT TSStatus tspectra_context_create(TSContextHandle* out_ctx);
TSPECTRA_EXPORT void tspectra_context_destroy(TSContextHandle ctx);

// RAW 文件解析 (输出去马赛克后的线性 Camera RGB 32-bit Float 图像)
TSPECTRA_EXPORT TSStatus tspectra_raw_load_file(
    TSContextHandle ctx,
    const char* utf8_file_path,
    TSImageBuffer* out_linear_image
);

// 释放由核心引擎内部分配的 TSImageBuffer 数据
TSPECTRA_EXPORT void tspectra_buffer_free(TSImageBuffer* buffer);

// 3D-LUT 参数烘焙 (将指定胶片配方离线计算为 3D-LUT 数据，用于取景器与缩略图)
TSPECTRA_EXPORT TSStatus tspectra_lut3d_bake(
    TSContextHandle ctx,
    const char* profile_id,
    const TSAdjustments* adjustments,
    uint32_t lut_size,              // 常用 33 或 65
    float* out_rgb_buffer           // 内存大小需为: lut_size^3 * 3 * sizeof(float)
);

// 全精度离线暗房冲洗 (调用 Compute 着色器执行漫反射与乳胶积分)
TSPECTRA_EXPORT TSStatus tspectra_render_pipeline(
    TSContextHandle ctx,
    const TSImageBuffer* src_image,
    const char* profile_id,
    const TSAdjustments* adjustments,
    TSImageBuffer* dst_image
);

#ifdef __cplusplus
}
#endif

#endif // TSPECTRA_API_H
