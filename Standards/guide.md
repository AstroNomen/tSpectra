# SpektraFilm Multiplatform Suite 开发工程规约 (SPEC-001)

## 1. 软件定性与目标
本项目基于开源项目 `spektrafilm` 的物理光谱与暗房模拟管线，构建原生全平台高性能胶片模拟套件：
- 桌面端 (Windows 10+ / macOS 15+)：专注于高质量单反/微单 RAW 文件（如 Nikon NEF）的解析、全精度物理模拟渲染、非破坏性调色与无损导出。
- 移动端 (iOS 18+ / Android 14+)：双轨制工作流，实现高帧率（60fps）取景器实时滤镜预览，以及全精度 RAW/照片的本地暗房冲洗修图。

## 2. 团队 Agent 分工与责任边界
本仓库为单一 Monorepo 架构，设立两名核心开发 Agent：
- Agent 1 (Windows 工作区)：
  - 负责 `core/` 的算法实现、LibRaw 桥接与 Vulkan 计算管线编写。
  - 负责 `desktop/windows/` (Qt 6 + Vulkan) 的开发与构建。
  - 负责 `mobile/android/` (Jetpack Compose + Vulkan NDK + CameraX) 的开发与构建。
- Agent 2 (macOS 工作区)：
  - 负责 `core/` 在 Apple Silicon (M4) 上的编译验证与 Metal 接口适配。
  - 负责 `desktop/macos/` (SwiftUI + Metal) 的开发与构建。
  - 负责 `mobile/ios/` (SwiftUI + Metal + AVFoundation) 的开发与构建。

### 【红线约束】跨 Agent 协作协议
1. 绝对禁止单方面修改 `core/include/spektra/spektra_api.h` 中的公共 C-ABI 接口。若需修改函数签名，必须先在 PR 中提出接口变更。
2. 算法核心、数据资产统一存放在 `core/`，各平台前端目录内禁止出现重复的图像数学解算逻辑。
3. 着色器代码遵循对称设计：`core/shaders/vulkan/` (GLSL -> SPIR-V) 与 `core/shaders/metal/` (MSL) 必须在功能与计算参数上严格 1:1 对齐。

---

## 3. 仓库物理目录结构

```text
spektrafilm/
├── .gitignore
├── CMakeLists.txt                    # 顶层 CMake，构建核心静态库及单元测试
├── core/                             # 核心引擎 (平台无关，现代 C++20)
│   ├── CMakeLists.txt
│   ├── include/spektra/
│   │   ├── spektra_api.h             # 纯 C-ABI 跨平台导出头文件
│   │   ├── types.h                   # 图像缓冲、色彩空间、胶片配方定义
│   │   └── raw_loader.h              # RAW 解码适配接口
│   ├── src/
│   │   ├── raw/                      # 基于 LibRaw 的解算与色彩矩阵映射
│   │   ├── physics/                  # 光谱重建、乳胶响应、特征曲线 (H&D)
│   │   ├── darkroom/                 # 防光晕 (Halation)、化学颗粒、相纸反转
│   │   └── lut/                      # 移动端 3D-LUT 烘焙引擎
│   └── shaders/
│       ├── vulkan/                   # GLSL 1.3 计算着色器 (.comp)
│       └── metal/                    # Metal Shading Language (.metal)
├── desktop/
│   ├── windows/                      # Agent 1 领地：Qt 6.8 + Vulkan 视口
│   └── macos/                        # Agent 2 领地：SwiftUI + MetalKit
├── mobile/
│   ├── android/                      # Agent 1 领地：Compose + CameraX + NDK
│   └── ios/                          # Agent 2 领地：SwiftUI + AVFoundation + Metal
└── assets/
    ├── film_profiles/                # 胶片配方 JSON 数据 (Kodak, Fuji, Ilford 等)
    └── spectral_data/                # 传感器光谱灵敏度矩阵与乳胶光谱响应表
```

## 4. 底层模拟物理管线 (Pipeline Architecture)

无论是 RAW 离线渲染还是移动端抓拍，底层的 Spektrafilm 物理阶段必须严格遵循以下阶段：

```
[原始输入] (Bayer RAW / 10-bit YUV)
   │
   ▼
1. 线性空间反马赛克与去黑电平 (LibRaw / Camera Sensor Linearization)
   │ -> 输出: 32-bit Float 线性 RGB 矩阵 (Camera RGB)
   ▼
2. 光谱反向重建 (Spectral Reconstruction)
   │ -> 基于相机传感器灵敏度模型，将 RGB 还原为多波长连续光谱响应向量
   ▼
3. 胶片乳胶曝光层积分 (Emulsion Layer Exposure)
   │ -> 红/绿/蓝三层感光乳胶的光子吸收模拟，应用 Hurter & Driffield 曲线
   ▼
4. 成色剂扩散与防光晕漫反射 (Halation & Dye Diffusion)
   │ -> GPU Compute Shader: 双高斯核空间卷积漫反射，高光边缘呈现特有红橙色光晕
   ▼
5. 随机物理颗粒合成 (Grain Synthesis)
   │ -> 依据局部曝光密度 (Density) 动态决定颗粒粗细与聚集度 (Clumping)
   ▼
6. 印相纸曝光与放大反转 (Paper Exposure & Tone Mapping)
   │ -> 模拟黑白/彩色相纸的反转伽马与肩部/趾部滚降
   ▼
[最终输出] (Display P3 / Rec.2020 / Ultra HDR 增益图输出)
```

## 5. 各平台核心组件与技术栈

### 5.1 共享核心库 (`core/`)

- **语言标准**：C++20（利用 `std::span`、模块化支持与强类型特性）。
- **外部依赖**：
  - `LibRaw` (0.21.3+)：用于 RAW 格式解析。
  - `glm`：数学向量与色彩矩阵计算。
  - `nlohmann_json`：解析胶片物理配置文件。
- **构建输出**：
  - Windows: `spektra_core.lib`
  - macOS: `libspektra_core.a`
  - Android: `libspektra_core.so`
  - iOS: `SpektraCore.xcframework`

### 5.2 Windows 桌面端 (`desktop/windows/`)

- **UI 框架**：Qt 6.8+ (C++)，采用深色暗房主题，主视口采用 `QVulkanWindow` 或 `QWindow` 绑定显卡交换链。
- **图形后端**：Vulkan 1.3（启用 `VK_KHR_dynamic_rendering`）。
- **独占特性**：针对 4K 屏幕开启 `QApplication::setHighDpiScaleFactorRoundingPolicy(PassThrough)`，支持拖拽加载 5000 万像素以上 Nikon NEF 文件，实时直方图与高动态渲染。

### 5.3 macOS 桌面端 (`desktop/macos/`)

- **UI 框架**：SwiftUI + AppKit 深度集成。
- **图形后端**：Metal 3 (`MTKView` 承载画布)。
- **独占特性**：基于 M4 芯片统一内存（Unified Memory），RAW 图像解码后由 CPU 传至 `MTLBuffer` 走 `StorageModeShared` 零拷贝管道，瞬时响应。
- **芯片架构:** 需要注意仅支持M系列芯片 从M1系列开始都可以适配使用

### 5.4 Android 移动端 (`mobile/android/`)

- **最低支持**：Android 14 (API 34)，目标版本 Android 15 (API 35)。
- **UI 与硬件**：Jetpack Compose 界面 + CameraX (`ImageAnalysis` + `ImageCapture`)。
- **双轨架构**：
  - 预览轨：`SurfaceView` + Vulkan，直接采样 3D LUT (65×65) + 蓝噪点颗粒片元 Shader，锁定 60fps。
  - 抓拍与冲洗轨：捕获 `ImageFormat.RAW_SENSOR`，交由 NDK 调用 `core` 进行后台高精度物理计算，并生成包含 ISO 21496-1 Gainmap 的 Ultra HDR JPEG。

### 5.5 iOS 移动端 (`mobile/ios/`)

- **最低支持**：iOS 18.0。
- **UI 与硬件**：SwiftUI + `AVFoundation`。
- **双轨架构**：
  - 预览轨：`AVCaptureVideoDataOutput` -> `CAMetalLayer` 跑 3D-LUT 采样 Metal 着色器。
  - 抓拍与冲洗轨：`AVCapturePhotoOutput` 捕获 Apple ProRAW/DNG -> Metal Compute Shader 执行全尺寸光晕与胶片曲线冲洗。

## 6. 四阶段实施路线图 (Milestones)

### Phase 0: 基础设施与骨架搭建 (Week 1)

- [ ] 建立 Monorepo 目录结构，配置根目录 `CMakeLists.txt`。
- [ ] 剥离 Spektrafilm 的 Python 逻辑，提取核心物理参数与数学公式，在 `core/` 下建立纯 C++ 基础模型。
- [ ] 编写简单的命令行测试程序，验证单张 RAW/TIFF 读取并应用单款胶片配方（如 Kodak Portra 400）的数学正确性。

### Phase 1: 桌面端双平台渲染引擎 (Week 2-3)

- [ ] Agent 1：在 Windows 上搭建 Qt 6 骨架，完成 Vulkan Compute Pipeline，跑通 GPU 加速的防光晕卷积与颗粒着色器。
- [ ] Agent 2：在 macOS 上搭建 SwiftUI 窗口，通过 Swift-C++ Interop 连接核心库，实现 Metal 版本的计算管线。
- [ ] 交付标准：两端均可导入尼康 NEF 格式 RAW 文件，2 秒内完成 4500 万像素图像的物理模拟渲染并导出 TIFF。

### Phase 2: 移动端双轨架构与取景器构建 (Week 4-5)

- [ ] 在 `core/` 中实现 3D-LUT 烘焙函数：将当前胶片配方离线生成 65×65 规格的 3D-LUT 数据。
- [ ] Agent 1：实现 Android CameraX 取景器，Vulkan SurfaceView 60fps 实时映射，捕获 RAW 帧。
- [ ] Agent 2：实现 iOS AVFoundation 捕获流，Metal 实时取景器，实现快门抓拍。
- [ ] 交付标准：真机实测（S25 与目标 iPhone）相机取景流畅无卡顿，按下快门能够捕获原始数据。

### Phase 3: 移动端相册冲洗、HDR 与细节打磨 (Week 6)

- [ ] 接入 Android MediaStore 与 iOS PhotoKit，支持从系统相册读取已有照片进行后期冲洗。
- [ ] S25 端支持 Ultra HDR 增益图输出，让高光呈现胶片特有通透感。
- [ ] iPhone 端适配 ProRAW 与广色域 Display P3 完整导出。

### Phase 4: 跨平台一致性对齐与发布构建 (Week 7)

- [ ] 使用标准 24 色色卡进行跨平台一致性测试，确保 Windows、macOS、Android、iOS 导出图片的色差 Delta-E < 1.5。
- [ ] 整理 README，打包自用分发文件。