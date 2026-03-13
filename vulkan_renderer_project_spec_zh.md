# Vulkan 实时渲染器项目完整规范

## 1. 文档目的

本文档用于定义一个**面向求职的 Vulkan 渲染器项目**，目标岗位是网易、腾讯、米哈游等公司的游戏引擎开发 / 游戏图形开发岗位。

本文档的目标读者包括：

- 对当前上下文不了解的其他 AI
- 没参与前置讨论的开发者
- 未来回头接手这个项目的你自己

这不是一个“画三角形”的教学 Demo，而是一个**小而完整、可展示、可写进简历、可用于面试展开的实时渲染器项目**。

本文档尽量把范围、架构、里程碑、验收标准、目录结构、实现顺序和常见坑写死，避免后续实现者跑偏。

## 2. 项目最终目标

使用 `C++20 + Vulkan` 开发一个独立桌面程序，能够实时加载并渲染 3D 场景，具备以下能力：

- 支持 `glTF 2.0` 场景加载
- 支持 `PBR` 材质渲染（metallic-roughness 工作流）
- 支持平行光阴影映射
- 支持 HDR 离屏渲染
- 支持 Tone Mapping
- 支持可选 Bloom
- 支持 ImGui 调试界面
- 支持基础相机控制
- 具备清晰的 GPU 资源管理方式
- 能产出适合截图、录屏和作品展示的最终效果

这个项目必须能够体现以下能力：

- 理解 Vulkan 初始化、设备选择和每帧渲染生命周期
- 理解图形管线、描述符和资源绑定
- 理解 Buffer、Image、同步和多帧资源复用
- 有能力写出结构化渲染程序，而不是单文件教程代码
- 具备用 RenderDoc 调试图形问题的思路

## 3. 明确不做的内容

第一版完整项目**不要**扩展成完整游戏引擎，以下内容明确不在首版范围内：

- 脚本系统
- 动画系统
- 骨骼动画
- 编辑器停靠式 UI
- 完整 Frame Graph
- ECS 架构
- 网络
- 音频
- 延迟渲染
- Clustered Lighting
- TAA
- SSAO
- 光线追踪
- 虚拟纹理

这些都可以后续扩展，但不能阻塞第一版完整可展示版本的交付。

## 4. 这个项目完成后应该支撑的简历表述

项目完成后，应该足以支撑类似以下简历描述：

- 独立使用 `C++` 与 `Vulkan` 实现实时渲染器，完成 Swapchain、命令提交、描述符管理、图形管线和多帧同步等核心模块。
- 实现 `glTF` 场景加载、`PBR` 材质、阴影映射、HDR 渲染与 Tone Mapping，支持复杂场景实时渲染。
- 构建可复用的 GPU Buffer、Texture、Sampler、Frame Resource 和 Render Pass 抽象，提升渲染器可维护性与可调试性。
- 集成 `ImGui`，并结合 `RenderDoc` 对渲染过程、资源绑定和阴影结果进行调试与验证。

文档中的一切范围设定都服务于这个目标。

## 5. 最终交付物要求

只有当以下内容都具备时，项目才算完成。

### 5.1 运行效果交付物

- 一个可运行的 Windows 桌面程序
- 支持键鼠控制相机
- 至少能正确渲染一个公开 `glTF` 场景
- PBR 材质正常工作
- 平行光阴影正常工作
- Tone Mapping 正常工作
- 最好具备 Bloom
- ImGui 面板可以调节灯光和渲染参数

### 5.2 工程交付物

- 清晰的源代码目录结构
- `CMake` 构建系统
- `README.md`，包含编译与运行说明
- `docs/screenshots/` 下有截图
- `docs/` 下有简短技术说明
- 有资产来源清单，说明模型和环境贴图来源

### 5.3 面试交付物

项目实现者必须能讲清楚：

- Vulkan Instance、Physical Device、Logical Device、Swapchain、Queue 是怎么建的
- 为什么需要 per-frame 资源
- 描述符是怎么组织的
- 阴影 Pass 是怎么工作的
- PBR 的输入是怎么进入着色器的
- HDR 离屏图像是怎么映射到 Swapchain 的
- CPU 和 GPU 同步是怎么做的
- 如果结果有问题，怎么用 RenderDoc 去定位

## 6. 第一版必须实现的功能

第一版完整项目必须至少包含如下功能。

### 6.1 窗口和程序基础设施

- 使用 Win32 或 GLFW 创建窗口
- 支持窗口尺寸变化
- Debug 模式启用 Vulkan Validation Layers
- 程序退出时按正确顺序销毁资源

### 6.2 相机系统

- 自由相机
- `W/A/S/D` 移动
- 按住鼠标右键拖动视角
- 滚轮调整移动速度或 FOV

### 6.3 场景加载

- 支持 `glTF 2.0` 网格加载
- 支持以下材质输入：
  - base color
  - metallic-roughness
  - normal map
  - emissive map（如果资源中有）
- 支持节点层级变换
- 支持从 `glTF` 引用的图像文件中加载纹理

### 6.4 核心渲染

- 支持 indexed draw
- 支持深度测试
- 使用 metallic-roughness 工作流实现 PBR
- 至少支持一个平行光
- 支持环境光近似
  - 最低可接受版本：常量环境光
  - 更推荐版本：Skybox + 简单环境光近似或 IBL

### 6.5 阴影

- 一个平行光阴影贴图
- 深度预渲染 Shadow Pass
- 主渲染 Pass 中读取阴影图
- 使用 `PCF` 或类似的简单软化方式

### 6.6 后处理

- 不透明物体先渲染到 HDR 离屏颜色目标
- 使用 Tone Mapping 将 HDR 图像映射到 LDR Swapchain 图像
- 正确进行 Gamma 校正
- 推荐加上 Bloom：亮部提取 + 分离模糊

### 6.7 UI 和诊断能力

- 集成 ImGui
- 至少显示和调节以下内容：
  - 帧时间
  - 相机位置
  - 平行光方向
  - 曝光值
  - Bloom 开关
  - 阴影 Bias
  - Normal Map 开关

### 6.8 可调试性

- 尽可能通过 Vulkan Debug Utils 给资源命名
- 关键路径加入断言与日志
- `docs/debugging.md` 中记录至少一次调试流程

## 7. 可选加分项

只有在主功能全部稳定完成之后，才允许增加**一个**加分模块：

- `Compute Shader` 粒子模拟
- GPU 视锥剔除
- GPU 间接绘制预处理

推荐加分项：

**Compute Shader 粒子系统**

原因：

- 与已有的 GPU 计算背景衔接最好
- 可视化效果明显
- 面试里比纯优化项更容易讲清楚

但要强调：这部分是可选项，不能拖慢主渲染器的完成。

## 8. 建议技术栈

除非环境明确受限，否则建议使用以下技术栈。

### 8.1 核心技术

- `C++20`
- `CMake 3.28+`
- `Vulkan SDK 1.3+`

### 8.2 推荐依赖库

- `GLFW`：窗口管理
- `glm`：数学库
- `VulkanMemoryAllocator`：Buffer / Image 内存分配
- `vk-bootstrap`：简化 Vulkan 初始化流程
- `tinygltf`：加载 `glTF`
- `stb_image`：图像读取
- `ImGui`：调试 UI
- `spdlog` 或自定义轻量日志系统

### 8.3 可选工具

- `glslc` 或 `shaderc`：编译着色器
- `RenderDoc`：图形调试
- `Tracy` 或 GPU Timestamp Query：后续性能分析

## 9. 架构要求

必须采用**分层式渲染器程序结构**，而不是把所有逻辑塞进 `main.cpp`。

架构设计必须遵循以下原则：

- Vulkan Handle 尽量由轻量封装类管理生命周期
- 场景数据与渲染后端数据分离
- 多帧资源显式管理
- 窗口尺寸变化时，只重建与尺寸相关的资源
- 将各个 Render Pass 的职责和上层帧调度解耦

## 10. 规定目录结构

建议尽量采用下面的目录结构：

```text
project_root/
  CMakeLists.txt
  README.md
  assets/
    models/
    textures/
    environments/
  shaders/
    common/
    fullscreen_triangle.vert
    pbr.vert
    pbr.frag
    shadow.vert
    shadow.frag
    skybox.vert
    skybox.frag
    bloom_extract.frag
    bloom_blur.frag
    tonemap.frag
  src/
    main.cpp
    app/
      Application.h
      Application.cpp
      InputState.h
      Timer.h
    core/
      Log.h
      Assert.h
      FileIO.h
    vulkan/
      VulkanContext.h
      VulkanContext.cpp
      Device.h
      Device.cpp
      Swapchain.h
      Swapchain.cpp
      CommandContext.h
      FrameContext.h
      DescriptorAllocator.h
      Buffer.h
      Buffer.cpp
      Image.h
      Image.cpp
      SamplerCache.h
      PipelineCache.h
      DebugUtils.h
      VmaUsage.h
    render/
      Renderer.h
      Renderer.cpp
      RenderResources.h
      RenderResources.cpp
      RenderScene.h
      RenderScene.cpp
      MaterialSystem.h
      MaterialSystem.cpp
      passes/
        ShadowPass.h
        ShadowPass.cpp
        ForwardPass.h
        ForwardPass.cpp
        SkyboxPass.h
        SkyboxPass.cpp
        BloomPass.h
        BloomPass.cpp
        TonemapPass.h
        TonemapPass.cpp
        ImGuiPass.h
        ImGuiPass.cpp
    scene/
      Scene.h
      Scene.cpp
      Node.h
      Mesh.h
      Material.h
      Camera.h
      Light.h
      GltfLoader.h
      GltfLoader.cpp
    math/
      Transform.h
      Frustum.h
  docs/
    architecture.md
    debugging.md
    asset_credits.md
    screenshots/
```

允许小范围调整，但 `app`、`vulkan`、`render`、`scene` 这几个职责层必须保留。

## 11. 各模块职责

### 11.1 `Application`

负责：

- 程序启动与关闭
- 创建窗口
- 处理事件循环
- 收集输入
- 更新相机
- 调用 `Renderer::renderFrame()`
- 响应窗口尺寸变化

### 11.2 `VulkanContext`

负责：

- 创建 Vulkan Instance
- 启用 Validation Layers
- 创建 Debug Messenger
- 选择 Physical Device
- 创建 Logical Device
- 获取图形队列 / 呈现队列
- 创建 Surface
- 初始化 `VMA`

### 11.3 `Swapchain`

负责：

- 创建和重建 Swapchain
- 管理 Swapchain Image View
- 选择 Surface Format 和 Present Mode
- 记录当前窗口尺寸

### 11.4 `FrameContext`

每个 in-flight frame 需要一个。至少包含：

- command pool
- primary command buffer
- image acquire semaphore
- render complete semaphore
- frame fence
- 当前帧需要的 uniform / staging 分配区

建议使用 `2` 帧 in-flight。

### 11.5 `Buffer` 和 `Image`

作为对底层 Vulkan 资源的轻量封装，至少应管理：

- Vulkan Handle
- 分配信息
- size / format / extent
- 上传与 layout transition 辅助函数

### 11.6 `Scene`

CPU 侧场景表示，至少包含：

- 节点层级
- 变换
- Mesh
- Material
- Texture
- Camera
- Light

### 11.7 `RenderScene`

GPU 侧可渲染场景表示，至少包含：

- 上传后的 Vertex / Index Buffer
- GPU 纹理与 Sampler
- 材质在 GPU 侧的引用
- 可绘制对象列表

它的作用是将导入的场景数据与渲染后端状态分离开。

### 11.8 `Renderer`

负责一帧的高层组织：

- 获取 Swapchain 图像
- 更新当前帧数据
- 按顺序执行各个 Pass
- 提交命令缓冲
- 呈现
- 处理 Resize 和 Out-of-Date Swapchain

### 11.9 各 Render Pass

每个 Pass 应该独立拥有：

- pipeline
- descriptor set layout
- 本 Pass 需要的 framebuffer / render target
- 更新与录制命令的接口

每个 Pass 不应该持有整个应用的全部状态。

## 12. 固定渲染流程

第一版渲染器必须按以下顺序组织一帧：

1. `Shadow Pass`
2. `Forward Opaque Pass`，输出到 HDR 颜色图 + 深度图
3. `Skybox Pass`，继续写入 HDR 颜色图
4. `Bloom Extract Pass`
5. `Bloom Blur Pass`，如果 Bloom 已启用
6. `Tonemap Pass`，输出到 Swapchain 图像
7. `ImGui Pass`

如果 Bloom 还没完成，可以暂时跳过第 4 和第 5 步，但架构上必须预留位置。

## 13. 推荐图像格式

建议使用如下格式：

- Swapchain：根据 Surface 支持选择，通常为 `VK_FORMAT_B8G8R8A8_SRGB`
- HDR 场景颜色图：`VK_FORMAT_R16G16B16A16_SFLOAT`
- 场景深度图：`VK_FORMAT_D32_SFLOAT` 或最佳可用深度格式
- 阴影图：`VK_FORMAT_D32_SFLOAT` 或最佳可用深度格式
- Bloom 中间图：`VK_FORMAT_R16G16B16A16_SFLOAT`

这些格式很常见，效果与面试可解释性都比较好。

## 14. 描述符组织策略

不要按每个对象随意设计描述符布局，必须采用稳定策略。

建议方案如下：

### 14.1 Global Set

包含：

- 摄像机矩阵
- 摄像机位置
- 平行光数据
- 曝光值与全局渲染参数

### 14.2 Material Set

包含：

- base color texture
- normal texture
- metallic-roughness texture
- emissive texture（如使用）
- 材质常量 buffer 或 push constants
- 阴影图采样器（也可以放到 Global Set）

### 14.3 Pass-Specific Set

用于：

- 环境贴图
- Bloom 输入图
- Tone Mapping 输入图

Push Constant 可用于：

- model matrix 索引
- material 索引
- 小型 per-draw 标志位

## 15. 场景数据结构要求

CPU 侧场景结构至少应等价于如下设计：

```cpp
struct Transform {
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;
    glm::mat4 localMatrix() const;
};

struct MeshPrimitive {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    int materialIndex;
};

struct Mesh {
    std::vector<MeshPrimitive> primitives;
};

struct Material {
    glm::vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    glm::vec3 emissiveFactor;
    int baseColorTexture;
    int normalTexture;
    int metallicRoughnessTexture;
    int emissiveTexture;
    bool doubleSided;
};

struct Node {
    int parent;
    std::vector<int> children;
    int meshIndex;
    Transform transform;
    glm::mat4 worldMatrix;
};

struct Scene {
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<TextureAsset> textures;
};
```

具体命名可以不同，但能力必须等价。

## 16. 顶点格式要求

为了直接支持 PBR 和 Normal Mapping，顶点结构建议固定为：

```cpp
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 uv0;
};
```

原因：

- Normal Mapping 需要切线空间
- `glTF` 常见资源本身提供 tangent
- 这是更接近真实项目的顶点布局

如果资产中没有 tangent，可采用以下策略之一：

- 导入时自动计算 tangent
- 暂时对该网格关闭 normal map

推荐做法：尽量自动计算 tangent。

## 17. Shader 要求

### 17.1 `pbr.vert`

至少输出：

- world position
- normal
- tangent basis（如果使用 normal mapping）
- UV
- 阴影投影坐标

### 17.2 `pbr.frag`

至少实现：

- base color 采样
- metallic-roughness 采样
- normal map 应用
- Cook-Torrance 风格或等价标准 PBR 近似
- NDF、Geometry、Fresnel 项
- 平行光着色
- 阴影采样
- emissive 叠加

### 17.3 `shadow.vert`

将模型顶点变换到光源裁剪空间并写入深度。

### 17.4 `shadow.frag`

对于 depth-only 渲染可以为空，或按管线需要省略。

### 17.5 `skybox.vert` / `skybox.frag`

如果实现 Skybox，则需要这两个着色器。

### 17.6 `fullscreen_triangle.vert`

用于后处理全屏 Pass。

### 17.7 `tonemap.frag`

至少实现：

- 读取 HDR 场景纹理
- 如果启用 Bloom，则叠加 Bloom 结果
- 应用曝光
- 应用 ACES-like 或 Reinhard Tone Mapping
- Gamma 校正

### 17.8 `bloom_extract.frag` / `bloom_blur.frag`

强烈推荐，但可以在 Tone Mapping 跑通后再补。

## 18. PBR 实现要求

必须满足以下规则：

- 使用 metallic-roughness 工作流
- `glTF` 的 base color 纹理按 sRGB 读取
- normal / metallic-roughness / emissive 按线性空间处理
- 即使某些纹理不存在，也要正确应用材质常量因子
- 至少支持：
  - albedo
  - roughness
  - metallic
  - normal
  - emissive

环境光方案的优先级如下：

1. 最佳：IBL + cubemap 预处理
2. 可接受：Skybox + 简单环境光近似
3. 最低版本：常量环境光

第一版可以先做到第 3 级，但如果想拿来做作品展示，最好至少升级到第 2 级。

## 19. 阴影实现要求

实现一个平行光 Shadow Map，要求如下：

- 阴影图分辨率从 `2048 x 2048` 起步
- 使用正交投影
- 支持可调深度 Bias
- 推荐 `PCF 3x3`
- 在 ImGui 中可视化调参

第一版不要尝试级联阴影贴图。

实现者必须理解：

- shadow acne 为什么发生
- peter-panning 为什么发生
- depth bias 与表面斜率、深度精度之间的关系

## 20. 后处理实现要求

如果目标包含 Tone Mapping，就必须先把场景渲染到 HDR 离屏纹理，再输出到 Swapchain，**不能**直接把 PBR 结果画进 Swapchain。

推荐后处理设计：

- 不透明 Pass 写入 HDR 图像
- Tonemap Pass 读取 HDR 图像后输出到 Swapchain
- Bloom 开启时，从 HDR 图像中提取亮部并做模糊，再在 Tonemap 时合成

推荐 Tone Mapping 算法：

- ACES 近似

可接受的回退方案：

- Reinhard

## 21. 同步要求

必须正确实现显式同步。

最低要求：

- 每个 in-flight frame 一个 fence
- 一个 acquire semaphore 用于等待 Swapchain 图像可用
- 一个 render complete semaphore 用于等待呈现
- 复用当前帧资源前必须等待对应 fence

可使用：

- 传统 Vulkan 同步写法
- 或 `VK_KHR_synchronization2`

二者都可以，但代码必须清晰、可解释。

## 22. Resize 处理要求

窗口尺寸变化时，必须正确完成以下流程：

- 等待设备空闲或到达安全同步点
- 销毁所有尺寸相关资源
- 重建 Swapchain
- 重建以下对象：
  - Swapchain Image View
  - HDR 目标图像
  - 深度图像
  - Bloom 中间图像
  - 如使用 framebuffer，则重建 framebuffer

不要无意义地重建与尺寸无关的长期资源。

## 23. 构建系统要求

必须使用 `CMake`。

项目构建层面要满足：

- 能自动编译 Shader，或至少提供脚本编译 Shader
- 按模块组织源文件
- 支持 `Debug` 和 `Release`

推荐方式：

- 调用 Vulkan SDK 自带的 `glslc`
- 将 `.spv` 放到 `build/shaders/` 或 `assets/shaders/compiled/`

## 24. 资源资产计划

必须使用公开可获取的资源，并记录来源。

推荐最小资产集合：

- `Sponza`：适合展示场景复杂度和阴影
- `DamagedHelmet` 或类似 `glTF` 示例：适合验证 PBR 材质
- 一张 HDRI 或 cubemap：如果实现 Skybox / 环境光

所有来源记录到 `docs/asset_credits.md`。

## 25. 必须遵循的里程碑顺序

必须按下面顺序推进，不允许跳着做。

### 里程碑 0：项目骨架

交付物：

- CMake 工程能构建
- 窗口能打开
- Vulkan Instance / Device 初始化成功
- Debug 模式下 Validation Layers 生效
- 空帧循环能运行

退出标准：

- 程序启动和退出时没有严重 Validation Error

### 里程碑 1：Swapchain 和基础显示

交付物：

- 成功创建 Swapchain
- Render Pass 或 Dynamic Rendering 跑通
- Command Buffer 能录制和提交
- 能显示一个三角形或正确清屏

退出标准：

- 呈现稳定
- Resize 可正常重建
- 没有严重 Validation Error

### 里程碑 2：Mesh 上传和相机

交付物：

- Vertex / Index Buffer 上传通路
- 能渲染简单网格
- 深度缓冲工作正常
- 自由相机可用

退出标准：

- 能正确渲染一个立方体或简单静态模型

### 里程碑 3：glTF 场景加载

交付物：

- 解析节点、网格、材质、纹理
- 把场景上传到 GPU
- 支持多个 primitive 和变换

退出标准：

- 一个公开 `glTF` 场景能被加载并显示基础贴图效果

### 里程碑 4：PBR

交付物：

- PBR Shader
- 材质纹理与材质因子生效
- Normal Mapping 生效
- 平行光基础着色生效

退出标准：

- `DamagedHelmet` 或同类模型看起来材质表现合理

### 里程碑 5：阴影映射

交付物：

- Shadow Map Render Target
- Shadow Pass
- 主 Pass 中完成阴影采样
- UI 中能调节光源参数

退出标准：

- 阴影能随着光方向变化而变化
- 通过 Bias 调整可以抑制明显 acne

### 里程碑 6：HDR 与 Tone Mapping

交付物：

- HDR 离屏颜色图
- 全屏 Tonemap Pass
- UI 中可调曝光

退出标准：

- HDR 原始结果与 Tone Mapping 后结果有明确可见差异

### 里程碑 7：Bloom

交付物：

- 亮部提取
- 模糊 Pass
- Tonemap 阶段完成 Bloom 合成

退出标准：

- Bloom 强度可调
- 效果明显但不过度泛光

### 里程碑 8：ImGui 和文档

交付物：

- ImGui 调试面板
- README
- 截图
- 架构说明
- 调试说明

退出标准：

- 其他人或其他 AI 可以根据仓库独立理解、编译和运行这个项目

### 里程碑 9：可选 Compute 加分项

交付物：

- Compute Pipeline
- 粒子存储 Buffer
- Compute 更新与渲染联动

退出标准：

- 粒子有可见动画
- 功能开关不会破坏主渲染流程

## 26. 强制验收标准

只有当以下条件全部满足时，主项目才算通过验收：

- `Debug` 和 `Release` 都能构建
- 正常渲染时没有反复出现的严重 Validation Error
- `glTF` 场景能以带纹理的 PBR 方式正确渲染
- 平行光阴影工作正常
- Tone Mapping 工作正常
- UI 可以调曝光和光方向
- Resize 正常
- 相机正常
- 项目中有截图
- README 能说明编译、运行、控制方式和功能列表

推荐质量目标：

- `1920x1080`
- 在中端桌面 GPU 上可以交互运行
- 没有明显闪烁、法线错误或阴影错误

## 27. 性能目标

这不是极限性能项目，但应该达到合理的质量标准。

建议目标：

- 在 `1080p` 下，能以可交互帧率运行 `Sponza` 或类似中等复杂度场景

如果不能稳定 `60 FPS`，至少要在文档中记录：

- 测试 GPU
- 分辨率
- 平均帧时间
- 开启的特性配置

## 28. 调试文档要求

项目必须包含 `docs/debugging.md`，至少写清楚：

- 如何在 Validation Layers 下运行
- 如何用 RenderDoc 抓一帧
- 如何检查：
  - Shadow Map 内容
  - 中间图像或关键纹理
  - HDR 离屏图像
  - Descriptor 绑定
  - 纹理颜色空间是否处理正确

常见检查方向包括：

- 阴影全黑时，检查光源矩阵和深度比较逻辑
- Normal Map 异常时，检查 tangent basis 和 handedness
- 材质过灰或过亮时，检查 sRGB / linear 处理
- Resize 后无法渲染时，检查 Image View 重建和 extent 传递

## 29. 常见问题与排查方向

### 29.1 画面发灰、过亮或过暗

可能原因：

- sRGB 处理错误
- Gamma 校正重复做了两次
- Tone Mapping 重复应用

### 29.2 阴影 Acne

可能原因：

- Bias 太小
- 阴影图精度不足
- 光源投影设置错误

### 29.3 Peter-Panning

可能原因：

- Bias 太大

### 29.4 程序退出时 Validation Error 爆炸

可能原因：

- GPU 仍在使用资源时就开始销毁
- 销毁顺序不正确

### 29.5 glTF 材质表现不对

可能原因：

- metallic-roughness 通道采样错误
- 纹理坐标错了
- tangent 缺失或处理错误

### 29.6 Resize 后画面坏掉

可能原因：

- 旧 framebuffer 或 Image View 还在用
- Descriptor 还引用着已销毁图像
- viewport / scissor 的 extent 没更新

## 30. 对后续 AI 的实现约束

如果其他 AI 按本文档实现项目，必须遵守以下原则：

- 在第一版完整交付前不要擅自改 scope
- 不要把 Vulkan 换成别的图形 API
- 不要把项目退化成教程级三角形程序
- 不要省略文档和截图
- 不要在主渲染器稳定之前追高级特性
- 不要把关键设计藏进巨型工具类或脚本里
- 不要忽略 Validation Warning，除非明确无害并写入文档

如果被阻塞，应按以下顺序处理：

1. 保持架构不变
2. 只削减可选特性，不削减核心特性
3. 任何临时简化都必须写进文档
4. 以“简历价值”和“面试价值”为优先，而不是新奇程度

## 31. 每个里程碑内部的推荐实现顺序

在每个里程碑内，建议固定按以下顺序推进：

1. 先定义数据结构和头文件
2. 再实现资源生命周期管理
3. 再做管线和描述符
4. 再编译 Shader
5. 再写命令录制
6. 再跑 Validation 和日志
7. 再做可视化验证
8. 最后补文档

这样可以减少后期大面积重构。

## 32. 推荐类接口骨架

具体代码可以不同，但建议接近如下结构：

```cpp
class Application {
public:
    void run();
private:
    void initWindow();
    void initRenderer();
    void mainLoop();
    void cleanup();
    void handleResize();
};

class Renderer {
public:
    void initialize(GLFWwindow* window);
    void shutdown();
    void renderFrame(const Scene& scene, const Camera& camera);
    void resize(uint32_t width, uint32_t height);
private:
    void createPersistentResources();
    void createSizeDependentResources();
    void destroySizeDependentResources();
    void updateFrameData(const Scene& scene, const Camera& camera, uint32_t frameIndex);
    void recordShadowPass(VkCommandBuffer cmd, const RenderScene& renderScene);
    void recordForwardPass(VkCommandBuffer cmd, const RenderScene& renderScene);
    void recordBloomPass(VkCommandBuffer cmd);
    void recordTonemapPass(VkCommandBuffer cmd, VkImage swapchainImage);
    void recordImGuiPass(VkCommandBuffer cmd);
};
```

## 33. 最终必须包含的文档

### `README.md`

必须包含：

- 项目概述
- 功能列表
- 控制方式
- 依赖要求
- 编译步骤
- 运行说明
- 截图

### `docs/architecture.md`

必须包含：

- 模块划分
- 一帧渲染流程
- 描述符组织方案
- Resize 重建策略

### `docs/debugging.md`

必须包含：

- Validation Layers 的启用方式
- RenderDoc 调试流程
- 常见图形 Bug 的排查方法

### `docs/asset_credits.md`

必须包含：

- 资产名称
- 来源链接
- 许可证信息（如有）

## 34. 什么叫“达到面试可讲”的程度

只有当实现者能清晰回答下列问题时，项目才算真正面试可用：

- 为什么 HDR 必须先渲染到离屏纹理，而不是直接渲染到 Swapchain？
- 为什么 PBR 对颜色空间处理很敏感？
- 为什么需要 per-frame command buffer 和同步对象？
- 描述符怎样组织才能避免绑定复杂度失控？
- Resize 时哪些资源必须重建，为什么？
- Shadow Pass 是怎么把结果传给主 Pass 的？
- 如果一个场景渲染出来是黑的，该怎么查？

如果项目只是“跑起来了”，但这些讲不清楚，那它还不算真正完成。

## 35. 最小展示方案

在项目宣称完成之前，必须准备一个最小作品展示场景，包括：

- 一个较大的环境场景
- 一个材质细节明显的英雄物体
- 清晰可见的平行光阴影
- 一个曝光滑条演示
- 至少三张截图：
  - 环境全景
  - 材质近景
  - 带 UI 的调试截图

## 36. 时间不够时的优先级

如果时间有限，优先级固定如下：

1. Vulkan 基础框架稳定
2. glTF 加载
3. PBR
4. 阴影映射
5. HDR Tone Mapping
6. ImGui
7. Bloom
8. Compute 加分项

这个顺序最能保证求职价值和面试价值。

## 37. 完成定义

当且仅当以下条件都满足时，项目才算真正完成：

- 从干净仓库可以成功构建和运行
- 能渲染 `glTF` 场景，并具备 PBR、阴影和 Tone Mapping
- 有截图和文档
- 稳定到足以录制演示视频
- 文档清晰到其他 AI 或开发者不需要口头说明也能继续推进

到了这个阶段，这个项目就可以用于：

- 简历投递
- 作品集展示
- 技术面试展开
- 后续继续扩展更高级图形特性
