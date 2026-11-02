# t.Spectra Multiplatform Suite: Agent Architecture Specification & Phased Execution Manual (v2.0)

> **文档性质**：Agent 学习规约、架构准则与原子化执行手册。
> **适用对象**：
>
> - **Agent 1**：Windows 10 / Android 环境技术主管（负责 Vulkan、Qt6、Compose、NDK、Core 基线）。
> - **Agent 2**：macOS 15.8 (M4) / iOS 18 环境技术主管（负责 Metal 3、SwiftUI、AVFoundation、Core 适配）。

---

## 第一部分：全局工程红线与命名准则 (Global Inviolable Rules)

### 1.1 命名隔离准则（防止句点 `.` 导致的路径与链接灾难）
* **显示名称（User-Facing Display Name）**：仅在操作系统桌面图标、应用标题栏文字、App Store/应用清单的显示名称中使用 `t.Spectra`。
* **工程/代码标识（Engineering Identifiers）**：**绝对禁止在任何目录名、文件名、命名空间、类名、变量名、CMake 目标名、Bundle ID、包名中使用句点 `.`**。统一遵循以下规则：
  - 代码根目录：`tSpectra/`
  - 核心 C-ABI 库目标与文件名：`tspectra_core`，头文件为 `tspectra_api.h`
  - C++ 顶层命名空间：`namespace tspectra { ... }`
  - C-ABI 导出符号前缀：`tspectra_*`（例如 `tspectra_create_context`）
  - Android 包名：`com.tspectra.camera`
  - iOS / macOS Bundle ID：`com.tspectra.desktop` / `com.tspectra.camera`
  - 桌面可执行文件名称：`tSpectraDesktop` / `tspectra_desktop.exe`

### 1.2 双 Agent 责任边界与文件锁定

* **公共只读区**：`core/include/tspectra/tspectra_api.h`。任何 Agent **严禁**单方面修改接口签名与结构体定义。若需扩充，必须先输出《接口变更提案》并由用户确认。
* **Agent 1 独占区**：
  - `desktop/windows/`
  - `mobile/android/`
  - `core/shaders/vulkan/`
  - `core/src/raw/` 与 Vulkan 后端计算实现
* **Agent 2 独占区**：
  - `desktop/macos/`
  - `mobile/ios/`
  - `core/shaders/metal/`
  - macOS 统一内存与 Metal 后端接口适配
* **零污染原则**：`core/` 目录内部严禁包含任何 UI 框架头文件，亦不得直接依赖特定操作系统 API（如直接引入 `<windows.h>` 或 `<UIKit/UIKit.h>`）。所有交互必须通过 `tspectra_api.h` 的纯 C 数据指针或不透明句柄。

---

## 第二部分：工程物理拓扑结构

```text
tSpectra/
├── CMakeLists.txt                         # 顶层 CMake，驱动核心库与测试构建
├── core/                                  # 核心引擎 (现代 C++20，平台无关)
│   ├── CMakeLists.txt
│   ├── include/tspectra/
│   │   ├── tspectra_api.h                 # 纯 C-ABI 外部统一导出头文件
│   │   ├── tspectra_types.h               # 数据结构、缓冲、像素格式定义
│   │   └── tspectra_raw.h                 # RAW 解码接口定义
│   ├── src/
│   │   ├── physics/                       # Spektrafilm 光谱重建、乳胶响应、特征曲线
│   │   ├── raw/                           # LibRaw 原生封装与线性数据提取
│   │   ├── darkroom/                      # 防光晕卷积 (Halation)、胶片颗粒合成
│   │   └── lut/                           # 3D-LUT 实时参数烘焙模块
│   ├── shaders/
│   │   ├── vulkan/                        # GLSL 计算着色器 (.comp) 与编译脚本
│   │   └── metal/                         # Metal Shading Language 着色器 (.metal)
│   └── tests/                             # 自动化验证与合成色卡 Diff 脚本
├── desktop/
│   ├── windows/                           # [Agent 1] Qt 6.8 + Vulkan 桌面工程
│   └── macos/                             # [Agent 2] SwiftUI + MetalKit 桌面工程
├── mobile/
│   ├── android/                           # [Agent 1] Compose + CameraX + NDK 工程
│   └── ios/                               # [Agent 2] SwiftUI + AVFoundation + Metal 工程
└── assets/
    ├── film_profiles/                     # 胶片配方 JSON 数据 (Kodak, Fuji, Ilford 等)
    └── spectral_data/                     # 相机传感器与乳胶光谱敏感度曲线矩阵
```

---

## 第三部分：核心 C-ABI 契约规范 (`core/include/tspectra/tspectra_api.h`)

此文件为核心交互契约，双端 Agent 必须严格围绕以下定义进行实现与调用：

```c
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
```

---

## 第四部分：分阶段原子化开发路线图 (Phased Roadmap)

> **调度指引**：用户将按 `Stage X - Step Y` 向对应 Agent 下达指令。接收指令的 Agent 必须仅执行对应步骤的内容，达成**完成定义 (DoD)** 并输出验证结果后终止，严禁擅自跳步或预先生成后续功能。

```
[Stage 0: 基础底座与 C-ABI 框架]
       │
       ▼
[Stage 1: 算法核心与双端着色器] ──► [跨端数学一致性验证 (Python 脚本)]
       │
       ▼
[Stage 2: 桌面原生暗房 (Windows / macOS)]
       │
       ▼
[Stage 3: 移动端取景与抓拍 (Android / iOS)]
       │
       ▼
[Stage 4: 移动端相册修图与 UI 打磨]
       │
       ▼
[Stage 5: 全平台自用构建与部署]
```

### Stage 0: 基础设施与 C-ABI 框架搭建

#### Step 0.1: Monorepo 目录与 CMake 骨架初始化
* **执行主体**：Agent 1 主导，Agent 2 验证。
* **前置依赖**：无。
* **执行任务**：
  1. 创建顶层 `CMakeLists.txt`，支持构建静态库 `tspectra_core`。
  2. 建立 `core/include/tspectra/tspectra_api.h` 头文件，放置桩函数定义。
  3. 配置 `core/CMakeLists.txt`，确保可以在 Windows MSVC 与 macOS Apple Clang 下完成空工程编译。
* **验证标准 (DoD)**：
  - Windows: `cmake -B build -S . && cmake --build build --config Release` 生成 `tspectra_core.lib`。
  - macOS: `cmake -B build -S . -DCMAKE_OSX_ARCHITECTURES=arm64 && cmake --build build` 生成 `libtspectra_core.a`，无任何 Warning。

#### Step 0.2: 集成 LibRaw 原生库并实现 RAW 解码接口
* **执行主体**：Agent 1。
* **前置依赖**：Step 0.1。
* **执行任务**：
  1. 在 `core/src/raw/` 中通过 CMake 引入 LibRaw 原生 C++ 库。
  2. 实现 `tspectra_raw_load_file` 逻辑：读取原生相机 RAW（支持尼康 NEF），输出 32-bit Float 线性去马赛克数据（提取原始曝光与白平衡矩阵，不进行 sRGB 压缩）。
  3. 实现 `tspectra_buffer_free`，确保跨 DLL 堆内存释放安全。
* **验证标准 (DoD)**：
  - 编写命令行单元测试 `test_raw_loader`，加载一张测试 `.NEF` 文件，能够打印出正确的图像宽高、去黑电平后的像素均值，且内存无泄漏。

---

### Stage 1: 算法核心与计算着色器管线 (Core Physics & Shaders)

#### Step 1.1: 光谱重建与乳胶 H&D 曲线数学模型实现
* **执行主体**：Agent 1 主导，Agent 2 代码审查。
* **前置依赖**：Step 0.2。
* **执行任务**：
  1. 将 Spektrafilm 项目中提取的相机光谱灵敏度矩阵与胶片感光曲线导入 `assets/`。
  2. 在 `core/src/physics/` 中以 C++20 实现相机 RGB 到多波长光谱响应的重建计算。
  3. 实现 Hurter & Driffield (H&D) 特征曲线插值算法。
* **验证标准 (DoD)**：
  - 单元测试运行时间：纯 CPU 单像素光谱推算每百万像素耗时 $< 20\text{ms}$，数值收敛无 NaN。

#### Step 1.2: 3D-LUT 烘焙引擎实现
* **执行主体**：Agent 1。
* **前置依赖**：Step 1.1。
* **执行任务**：
  1. 实现 `tspectra_lut3d_bake` 函数。
  2. 遍历 $N \times N \times N$（$N=33$ 或 $65$）网格节点，将节点的 RGB 输入通过光谱响应与相纸反转模型，输出映射后的 RGB 数据。
* **验证标准 (DoD)**：
  - 生成 $33 \times 33 \times 33$ 规格 3D-LUT 烘焙耗时在 CPU 单线程下 $< 50\text{ms}$；导出数据可直接保存为通用 `.cube` 格式并在外部查看器中验证色彩连续性。

#### Step 1.3: Vulkan 1.3 Compute 着色器与离线调度管线
* **执行主体**：Agent 1。
* **前置依赖**：Step 1.1。
* **执行任务**：
  1. 在 `core/shaders/vulkan/` 编写计算着色器：
     - `halation.comp`：双高斯核多 pass 漫反射光晕算法。
     - `grain.comp`：基于局部密度的 PCG 物理颗粒算法。
     - `lut3d_apply.comp`：带三线性插值的 3D-LUT 映射着色器。
  2. 配置 glslangValidator 将其预编译为 `.spv` 二进制，在 `core/src/darkroom/` 中实现 Vulkan Compute Pipeline 调度。
* **验证标准 (DoD)**：
  - 在 RTX 5070 上运行 4500 万像素图像离线渲染，Vulkan 全通调调度耗时 $< 100\text{ms}$。

#### Step 1.4: Metal 3 计算着色器与 M 系列芯片零拷贝调度
* **执行主体**：Agent 2。
* **前置依赖**：Step 1.3（必须对齐 GLSL 算法）。
* **执行任务**：
  1. 在 `core/shaders/metal/` 编写与 Vulkan GLSL **完全 1:1 数学等价** 的 MSL 着色器：
     - `halation.metal`
     - `grain.metal`
     - `lut3d_apply.metal`
  2. 配置 Metal Pipeline State，使用 `MTLResourceStorageModeShared` 实现 CPU 解码数据到 GPU 的零拷贝读取。
* **验证标准 (DoD)**：
  - 在 MacBook Air M4 上运行相同尺寸图像处理，单帧耗时 $< 150\text{ms}$。

#### Step 1.5: 跨平台数字合成色卡差值测试 (Consistency Verification)
* **执行主体**：双 Agent 协同。
* **前置依赖**：Step 1.3 与 Step 1.4。
* **执行任务**：
  1. 在 `core/tests/` 中生成包含 24 标准色块的合成测试图 `synthetic_macbeth_f32.tiff`。
  2. Agent 1 在 Windows/Vulkan 下调用 `tspectra_render_pipeline` 生成 `output_win.tiff`。
  3. Agent 2 在 macOS/Metal 下调用 `tspectra_render_pipeline` 生成 `output_mac.tiff`。
  4. 运行 `test_consistency.py` 比对像素误差。
* **验证标准 (DoD)**：
  - 均方根误差 $\text{RMSE} < 0.005$，平均色差 $\Delta E_{00} < 0.1$，判定两端物理着色器数学对称。

---

### Stage 2: 桌面原生暗房应用开发 (Desktop Frontends)

#### Step 2.1: Windows 端 (Qt 6.8 + Vulkan) 4K 基础界面搭建
* **执行主体**：Agent 1。
* **前置依赖**：Step 1.3。
* **执行任务**：
  1. 初始化 `desktop/windows/` 工程，接入 Qt 6.8（MSVC 2022）。
  2. 在 `main.cpp` 中强制设置：
     ```cpp
     QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
     ```
  3. 实现三栏式暗房专业界面（基准背景色 `#181818`），控件设置足够大的触控热区与中性灰文字。
  4. 左侧预设面板使用 `QListView` 呈现预渲染的胶片缩略图；右侧放置曝光、光晕、颗粒与相纸反差的大尺寸 `QSlider`。
* **验证标准 (DoD)**：
  - 在 4K 150%/200% 缩放比例下启动程序，字体与控件边缘无锯齿与位图拉伸模糊，界面布局响应流畅。

#### Step 2.2: Windows 端 Vulkan 视口集成与 RAW 文件流水线贯通
* **执行主体**：Agent 1。
* **前置依赖**：Step 2.1 与 Step 0.2。
* **执行任务**：
  1. 在 Qt 视口区域继承 `QWindow` 绑定 Vulkan Swapchain。
  2. 支持拖拽尼康 NEF 文件直接载入，后台线程调用 `tspectra_raw_load_file`。
  3. 支持实时响应右侧滑块变动，利用 Vulkan 计算管线刷新中央主视口。
* **验证标准 (DoD)**：
  - 拖入 4500 万像素 NEF 文件后，1 秒内显示图像，滑动“曝光补偿”与“防光晕”滑块，视口渲染平滑无卡顿。

#### Step 2.3: macOS 端 (SwiftUI + Metal) 基础界面与 Swift-C++ 桥接
* **执行主体**：Agent 2。
* **前置依赖**：Step 1.4。
* **执行任务**：
  1. 初始化 `desktop/macos/` Xcode 工程，目标设置为 macOS 15.0+，架构锁定 `arm64`。
  2. 启用 Swift 5.9+ 的 `InteroperabilityMode = C++`，直接在 Swift 中调用 `tspectra_api.h` 符号。
  3. 使用 `NavigationSplitView` 构建符合 macOS 风格的暗房三栏界面。
* **验证标准 (DoD)**：
  - 项目在 MacBook Air M4 上一键编译通过，Swift 成功创建 `TSContextHandle` 并不崩溃。

#### Step 2.4: macOS 端 MetalKit 画布与统一内存交互
* **执行主体**：Agent 2。
* **前置依赖**：Step 2.3。
* **执行任务**：
  1. 在中央区域接入 `MTKView`，通过 Metal Pipeline 渲染图像。
  2. RAW 解码数据通过 `MTLBuffer` 共享内存直接传给 Metal Shader，视口支持滚轮缩放与平移操作。
* **验证标准 (DoD)**：
  - 导入大尺寸 RAW 照片，内存增长平稳，缩放与调节滑块时画布维持 60fps 刷新。

---

### Stage 3: 移动端取景器与相机抓拍系统 (Mobile Viewfinder & Capture)

#### Step 3.1: Android 端 (CameraX + Vulkan 3D-LUT) 实时取景器
* **执行主体**：Agent 1。
* **前置依赖**：Step 1.2 与 Step 1.3。
* **执行任务**：
  1. 在 `mobile/android/` 搭建 Compose 工程（`minSdk=34`, `targetSdk=35`）。
  2. 配置 CameraX `Preview` 流，绑定到 Vulkan 驱动的 `SurfaceView`。
  3. 导入烘焙好的 $33 \times 33 \times 33$ 3D-LUT 纹理，在片元着色器中对相机 YUV 帧执行实时色彩映射与蓝噪点叠加。
* **验证标准 (DoD)**：
  - 三星 S25 真机实测：取景器稳定 60fps 显示胶片色调，连续运行 5 分钟设备无异常发热。

#### Step 3.2: Android 端 RAW 抓拍与 Ultra HDR 增益图输出
* **执行主体**：Agent 1。
* **前置依赖**：Step 3.1。
* **执行任务**：
  1. 配置 CameraX `ImageCapture`，启用 `CAPTURE_MODE_MAXIMIZE_QUALITY` 捕获 `RAW_SENSOR`。
  2. 按下快门后将原始 RAW 字节传给 NDK 调用 `tspectra_render_pipeline` 进行离线冲洗。
  3. 将冲洗结果封装为带有 ISO 21496-1 Gainmap 的 Ultra HDR JPEG 格式写入系统相册。
* **验证标准 (DoD)**：
  - 三星 S25 实拍照片在系统相册中打开时，屏幕局部高光自动触发 HDR 亮度提升，呈现暗房光感。

#### Step 3.3: iOS 端 (AVFoundation + Metal 3D-LUT) 实时取景器
* **执行主体**：Agent 2。
* **前置依赖**：Step 1.2 与 Step 1.4。
* **执行任务**：
  1. 在 `mobile/ios/` 搭建 SwiftUI 工程（`IPHONEOS_DEPLOYMENT_TARGET = 18.0`）。
  2. 配置 `AVCaptureSession`，通过 `AVCaptureVideoDataOutput` 提取 10-bit YUV 帧。
  3. 使用 `CAMetalLayer` 挂载 3D-LUT 采样 Metal 片元着色器进行实时色彩呈现。
* **验证标准 (DoD)**：
  - 目标真机（iPhone 17 Pro Max / 测试机）取景器达到稳定高帧率渲染，滑动手势切换胶片轮盘无卡顿。

#### Step 3.4: iOS 端 ProRAW 抓拍与 Display P3 色彩管理
* **执行主体**：Agent 2。
* **前置依赖**：Step 3.3。
* **执行任务**：
  1. 配置 `AVCapturePhotoOutput` 捕获 Apple ProRAW (DNG)。
  2. 后台调用 `tspectra_render_pipeline` 处理全分辨率数据。
  3. 以 Display P3 色彩空间与 10-bit 深度保存至系统相册。
* **验证标准 (DoD)**：
  - 按下快门无明显 UI 冻结，相册成功保存全精度成片，EXIF 信息完整保留。

---

### Stage 4: 移动端暗房相册后期与原生 UI 打磨 (Mobile Darkroom & Polish)

#### Step 4.1: Android 端 Material 3 暗房相册与修图流
* **执行主体**：Agent 1。
* **前置依赖**：Step 3.2。
* **执行任务**：
  1. 接入 Android MediaStore API，支持在 App 内浏览本地已有照片。
  2. 使用 Material 3 规范构建修图面板（Dynamic Color 色调、大滑块触控、触觉反馈 `HapticFeedbackConstants.CONFIRM`）。
  3. 提供全套曝光、光晕、颗粒与胶片预设调节，支持二次冲洗覆盖或另存为。
* **验证标准 (DoD)**：
  - 导入相册内任意 4000 万像素图片，修图调节即时响应，保存导出功能稳定。

#### Step 4.2: iOS 端 SwiftUI 玻璃拟物材质与 PhotoKit 修图流
* **执行主体**：Agent 2。
* **前置依赖**：Step 3.4。
* **执行任务**：
  1. 接入 iOS PhotoKit，支持相册照片导入。
  2. 运用 iOS 18 风格的 `.ultraThinMaterial` 打造半透明毛玻璃底部胶片转轮与控制条。
  3. 接入 `UIImpactFeedbackGenerator(style: .rigid)`，为胶片切换与快门行程提供物理触感。
* **验证标准 (DoD)**：
  - UI 动效细腻，毛玻璃半透明背景与底层暗房画布无遮挡冲突，符合苹果原生设计体验。

---

### Stage 5: 全平台自用构建与部署验证 (Packaging & Verification)

#### Step 5.1: 自动化一致性差值比对与最终校准
* **执行主体**：双 Agent 协同。
* **前置依赖**：Stage 2 与 Stage 3 全部完成。
* **执行任务**：
  1. 全平台运行合成色卡冲洗测试。
  2. 比对 Windows、macOS、Android、iOS 四端导出的同配方图像色彩数值差异。
* **验证标准 (DoD)**：
  - 四端色差均落在 $\Delta E_{00} < 0.15$ 范围内，视觉无肉眼可辨偏差。

#### Step 5.2: 全平台产物打包
* **执行主体**：Agent 1 (Windows/Android), Agent 2 (macOS/iOS)。
* **前置依赖**：Step 5.1。
* **执行任务**：
  1. Agent 1：打包 Windows 绿色版免安装压缩包（包含 `tspectra_desktop.exe`、Qt 依赖库及 Vulkan 运行时）；打包 Android 签名 `.apk`。
  2. Agent 2：打包 macOS 原生 `tSpectra.app` 并封装为 `.dmg`；使用开发证书打包 iOS 自签名 `.ipa`。
* **验证标准 (DoD)**：
  - 用户在四台设备上分别运行打包产物，均可正常启动且标题均正确显示为 **t.Spectra**。

---

## 第五部分：Agent 执行协议与交互指令标准

当用户向你发送指令时，你必须严格遵循以下协议做出回应并执行：

### 5.1 用户指令格式示例
> **用户指令**：“Agent 1，请执行 Stage 0 中的 Step 0.2。”

### 5.2 Agent 响应与行动步骤规范
1. **解析阶段与步骤**：读取并复述当前步骤的【执行任务】与【验证标准】。
2. **环境与约束确认**：检查自身角色（Agent 1 还是 Agent 2），确认不越界修改受限文件，核实工程与标识符命名中绝无非法句点 `.`。
3. **输出代码与工程配置**：仅产出该步骤所必需的文件代码或修改建议。
4. **自检与汇报**：提供该步骤对应的本地编译或测试运行命令，明确告知用户如何确认完成定义 (DoD) 已达成。