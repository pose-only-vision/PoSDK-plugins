# DegensacTwoViewEstimator Plugin

Degensac两视图位姿估计插件（**纯C++版本**），直接调用Degensac C代码，无需Python依赖。

从 `1.1.1` 起，本插件按 PoSDK Marketplace 的
`open_plugin_open_dependencies` 模式发布。公开仓库中的同版本 Git commit
包含构建签名 `.pospkg` 所使用的完整插件、DEGENSAC 与 CCMATH 源码。

## 概述

本插件实现了使用DEGENSAC算法进行两视图位姿估计的功能。与`PyDegensacTwoViewEstimator`相比，本插件：

- ✅ **纯C++实现**：无需Python环境
- ✅ **高性能**：直接内存操作，无文件I/O开销
- ✅ **可复现采样**：workflow 的 `random_seed` 进入线程局部采样器
- ✅ **并发隔离**：随机状态与 LO 哈希表不会在不同视图线程间共享
- ✅ **相同算法**：使用与pydegensac完全相同的C核心代码

## DEGENSAC算法特点

DEGENSAC (Degeneracy-Aware RANSAC) 是一种改进的RANSAC算法：

1. **LO-RANSAC**：带局部优化的RANSAC，提高模型精度
2. **退化检测**：能检测并处理平面场景等退化情况
3. **定向约束**：使用定向极线约束
4. **MSAC评分**：比标准RANSAC评分更鲁棒

## 文件结构

```
DegensacTwoViewEstimator/
├── CMakeLists.txt                    # CMake构建配置
├── degensac_two_view_estimator.hpp   # C++头文件
├── degensac_two_view_estimator.cpp   # C++实现文件
├── degensac_two_view_estimator.ini   # 配置文件
├── degensac_rng.c/.h                 # 线程局部确定性采样器
├── BUILDING.md / RELINKING.md        # 独立构建与 LGPL 重链接说明
├── LICENSE                           # PoSDK适配层的MPL-2.0许可证
├── pydegensac/                       # 本插件自带并由Git跟踪的DEGENSAC源码
│   └── src/pydegensac/matutls/
│       └── lgpl.license              # CCMATH LGPL-2.1完整许可证
├── THIRD_PARTY_NOTICES.md            # 第三方版权与许可证说明
└── README.md                         # 本文档
```

## 依赖要求

- **LAPACK**：线性代数库
- **BLAS**：基础线性代数子程序库
- **OpenCV**：计算机视觉库
- **DEGENSAC源码**：已固定在本插件的 `pydegensac/` 中；不读取相邻
  `PyDegensacTwoViewEstimator` 目录

### 安装依赖

#### macOS
```bash
brew install lapack openblas
```

#### Ubuntu
```bash
sudo apt-get install liblapack-dev libblas-dev
```

## 安装步骤

### 构建插件

插件会在主项目构建时自动构建。CMake会自动检测Degensac源码是否存在。

```bash
cmake --build build/presets/macos-arm64-developer \
  --target degensac_two_view_estimator
```

## 使用方法

### 配置参数

编辑 `degensac_two_view_estimator.ini` 文件：

```ini
[degensac_two_view_estimator]
view_i=0                           # 源视图ID
view_j=1                           # 目标视图ID
ransac_threshold=1.0               # RANSAC阈值（像素）
confidence=0.999                   # 置信度
max_iterations=50000               # 最大迭代次数
random_seed=0                      # 显式、可复现的采样种子
enable_degeneracy_check=true       # 启用退化检测
error_type=sampson                 # 误差类型
```

### 默认参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ransac_threshold` | 1.0 | 内点阈值（像素）|
| `confidence` | 0.999 | RANSAC置信度 |
| `max_iterations` | 50000 | 最大迭代次数 |
| `random_seed` | 0 | 同输入、同seed得到相同采样序列 |

## 与PyDegensac插件对比

| 方面 | PyDegensacTwoViewEstimator | DegensacTwoViewEstimator |
|------|---------------------------|--------------------------|
| **实现语言** | C++ + Python调用 | 纯C++ |
| **依赖** | Python + pydegensac包 | LAPACK/BLAS |
| **性能** | 需要文件I/O和进程调用 | 直接内存操作 |
| **部署** | 需要conda环境 | 原生dylib及其许可证/合规材料 |
| **算法** | Degensac C代码 | 相同的Degensac C代码 |
| **兼容性** | 跨平台（需Python） | 跨平台（无需Python） |

## 平台兼容性

当前认证范围是 macOS arm64。Consumer 与 Developer Kit DMG 不内置本插件；
普通用户通过插件市场安装目标平台签名 `.pospkg`。插件包含 MIT 的
DEGENSAC/LO-RANSAC 代码和 LGPL-2.1 的 CCMATH 代码；具体声明、完整许可证、
源代码和重链接步骤见 `THIRD_PARTY_NOTICES.md`、`BUILDING.md` 与
`RELINKING.md`。

## 工作原理

1. **C++插件**接收输入数据（匹配点、特征、相机模型）
2. **准备点对数据**为Degensac格式 `[x1,y1,1,x2,y2,1]`
3. `random_seed` 初始化插件自己的线程局部 RNG
4. **直接调用** `exp_ransacFcustomLAFSeeded` 估计基础矩阵
5. **转换为本质矩阵**：`E = K_2^T F K_1`
6. 在两相机各自归一化坐标中恢复位姿，并提交最终正深度校验后的内点掩码

插件当前会拒绝泛化相机和带畸变的经典相机，避免把不满足基础矩阵成像模型的输入
静默当作无畸变针孔数据。运行日志中的 `[DEGENSAC_DIAG]` 会记录精确后端、seed、
阈值、迭代预算、退化检测开关、采样次数与采样指纹。

为防止恶意或误配置输入造成过量内存/CPU消耗，适配层还会拒绝超过产品上限的匹配
规模与“匹配数 × 迭代数”组合。

## 故障排除

### 问题1：找不到LAPACK/BLAS

**解决方案：**
```bash
# macOS
brew install lapack openblas

# Ubuntu
sudo apt-get install liblapack-dev libblas-dev libopenblas-dev
```

### 问题2：基础矩阵估计失败

**可能原因：**
1. 匹配点数量不足（至少需要8个点）
2. RANSAC阈值设置不当
3. 输入数据质量问题

**解决方案：**
- 检查输入匹配数量
- 调整`ransac_threshold`参数
- 增加`max_iterations`

## 参考文献

1. Chum, O., Werner, T., & Matas, J. (2005). Two-View Geometry Estimation Unaffected by a Dominant Plane. CVPR.
2. Chum, O., Matas, J., & Kittler, J. (2003). Locally Optimized RANSAC. DAGM.
3. PyDegensac GitHub: https://github.com/ducha-aiki/pydegensac

## 作者

Qi Cai <qicaiCN@gmail.com>
