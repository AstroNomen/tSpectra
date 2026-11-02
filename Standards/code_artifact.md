# SpektraFilm 全平台胶片模拟套件：开发指南与阶段追踪手册

> 本手册供项目所有者（用户）掌握全局架构、技术原理、软硬件准备以及各阶段验收标准。

---

## 一、 核心痛点与技术解决方案

### 1. 零成本无色卡测试方案 (Synthetic Verification)
无需购买昂贵的爱色丽实体色卡。我们使用纯数学生成的数字基准图像进行跨平台一致性检验：
```
[数字标准 24 色测试图谱 (Synthetic TIFF)]
           │
     ┌─────┴────────────────┐
     ▼                      ▼
Agent 1 (Windows/Vulkan)  Agent 2 (macOS/Metal)
     │                      │
     ▼                      ▼
[导出的 TIFF A]          [导出的 TIFF B]
     └─────┬────────────────┘
           ▼
[自动化差值比对脚本 (Python / C++)]
计算 RMSE (均方根误差) 与 ΔE_00 (CIEDE2000)
指标：ΔE_00 < 0.1 即判定两端着色器物理数学严格等价
```

### 2. 预设缩略图与移动端实时取景架构
* **原项目卡顿根源**：Spektrafilm 采用多波长光谱采样积分与多层漫反射卷积，CPU 离线计算一张 4000 万像素照片耗时漫长。
* **分级计算引擎 (Tiered Pipeline)**：
  1. **离线烘焙 (Bake)**：胶片配方中的光谱积分、乳胶响应曲线预先解算并离线烘焙为一个高精度 3D-LUT 纹理（$33 \times 33 \times 33$ 或 $65 \times 65 \times 65$）。
  2. **桌面端缩略图流式呈现**：RAW 文件导入后生成 $384 \times 384$ 线性代理图缓存。切换预设时，GPU 在 1 毫秒内完成 3D-LUT 映射，缩略图网格实现即时滚动。
  3. **移动端取景器**：相机预览流（1080p/4K 60fps）直接通过 Vulkan/Metal 片元着色器采样 3D-LUT，辅以简易蓝噪点颗粒片元 Shader，帧率稳跑 60~120fps。
  4. **全精度抓拍与导出**：按下快门捕获 RAW/DNG，切换至后台全功能物理计算管线（多 pass 光晕漫反射 + 物理胶片颗粒卷积）。

### 3. LibRaw 与 Spektrafilm 代码复用
* 原项目中的 `rawpy.RawPy(file)` 本质就是 `LibRaw` 的 Python 封装。
* C++ 引擎中直接引入 `LibRaw 0.21+` 原生库，复用以下已有成果：
  * 相机传感器的拜耳去马赛克（AHD / AMaZE）。
  * 提取传感器原始线性数据（去黑电平、白平衡增益）。
  * 直接复用 Spektrafilm 仓库中提取的相机光谱响应矩阵及各胶片型号的光谱吸收曲线（JSON/CSV 格式）。

---

## 二、 平台技术栈决策清单

| 平台 | UI 框架 | 渲染与计算后端 | 架构目标 | 关键特性 |
| :--- | :--- | :--- | :--- | :--- |
| **Windows** | Qt 6.8+ (C++) | Vulkan 1.3 | x86_64 | 4K DPI 自适应无级缩放，RTX 5070 硬件并发计算 |
| **macOS** | SwiftUI + AppKit | Metal 3 | **arm64 独占 (M 系列)** | 统一内存零拷贝传输，完全弃用 x86 代码 |
| **Android** | Jetpack Compose | Vulkan 1.3 (NDK) | arm64-v8a (API 34-35) | CameraX RAW 流抓拍，Ultra HDR 增益图输出 |
| **iOS** | SwiftUI | Metal 3 | arm64 (iOS 18.0+) | AVFoundation ProRAW 流，系统级触觉反馈 |

---

## 三、 用户操作与环境配置步骤

### 1. Windows PC 配置 (为 Agent 1 准备)
1. 安装 **Visual Studio 2022 Community**：
   - 勾选组件：「使用 C++ 的桌面开发」（包含 MSVC v143、C++ CMake 工具、Windows 11 SDK）。
2. 安装 **Vulkan SDK (1.3.x)**：
   - 从 LunarG 官网下载并运行安装包，在终端运行 `vulkaninfo` 确认 RTX 5070 被正确识别。
3. 安装 **Qt 6.8 (MSVC 2022 64-bit)**：
   - 勾选 `Qt 6.8.x for MSVC 2022`、`Qt Quick` 与 `Shader Tools`。
4. 安装 **Android Studio**：
   - 在 SDK Manager 中安装 **Android SDK Platform 35**、**NDK (r26 或更高版本)** 与 **CMake 3.22+**。

### 2. MacBook Air M4 配置 (为 Agent 2 准备)
1. 保持当前 **macOS 15.8** 与 **Xcode 16**，无需升级系统。
2. 终端执行一次以激活命令行编译链：
   ```bash
   sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
   ```
3. 通过 Homebrew 安装构建工具：
   ```bash
   brew install cmake ninja
   ```

---

## 四、 项目开发里程碑与验收标准

```
[Phase 0: 核心算法与离线管线] ──────────► [Phase 1: 桌面端双端落地]
          │                                         │
          ▼                                         ▼
[Phase 2: 移动端双轨取景器] ────────────► [Phase 3: 移动端高级特性]
                                                    │
                                                    ▼
                                          [Phase 4: 一致性对齐与发布]
```

### Phase 0: 核心数学提取与 C++ 计算引擎验证 (预计 1 周)
- **目标**：将 Spektrafilm 的 Python 逻辑转写为平台无关的 C++20 静态库。
- **用户验收项**：
  - [ ] 运行命令行测试程序，输入单张尼康 NEF 格式 RAW 文件，成功输出应用 Kodak Portra 400 配方的 TIFF 图像。
  - [ ] 离线生成 $33 \times 33 \times 33$ 的 `.cube` 或内存 LUT 耗时小于 50ms。

### Phase 1: 桌面端 RAW 处理软件构建 (预计 2 周)
- **目标**：Windows (Qt6 + Vulkan) 与 macOS (SwiftUI + Metal) 基础暗房软件成型。
- **用户验收项**：
  - [ ] Windows 端在 4K 显示器上界面清晰、控件无模糊拉伸。
  - [ ] macOS 端原生 arm64 极速编译，内存占用平稳。
  - [ ] 两端均实现左侧胶片预设缩略图列表，右侧大图视口支持平移与无级缩放。

### Phase 2: 移动端实时取景与抓拍双轨跑通 (预计 2 周)
- **目标**：Android 与 iOS 实现取景器实时滤镜渲染与 RAW 抓拍。
- **用户验收项**：
  - [ ] 三星 S25 真机实测：取景器流畅稳定 60fps，无严重发热。
  - [ ] iPhone 17 Pro Max 实测：取景器丝滑渲染，按下快门无黑屏卡顿。

### Phase 3: 移动端相册冲洗与 HDR 增强 (预计 1 周)
- **目标**：从系统相册导入照片进行高精度胶片模拟冲洗。
- **用户验收项**：
  - [ ] 三星 S25 导出照片在相册中触发 Ultra HDR 高光增强显示。
  - [ ] iOS 支持导入 ProRAW DNG 并输出 Display P3 色彩空间照片。

### Phase 4: 跨端一致性校验与自用打包 (预计 1 周)
- **目标**：像素级误差比对与应用分发打包。
- **用户验收项**：
  - [ ] 运行自动化色卡验证脚本，Windows 与 macOS 输出文件的 $\Delta E_{00} < 0.1$。
  - [ ] Windows 生成便携绿色包，macOS 生成 `.dmg`，Android 生成 `.apk`，iOS 生成自签名 `.ipa`。