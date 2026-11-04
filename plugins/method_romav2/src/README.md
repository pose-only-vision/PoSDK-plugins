# RoMa v2 — 插件源码 / Plugin source

对应市场版本：**0.1.0**，发布标签：[`posdk-method_romav2-v0.1.0`](https://github.com/pose-only-vision/PoSDK-plugins/releases/tag/posdk-method_romav2-v0.1.0)。

本目录提供这个已发布版本的 PoSDK 插件封装源码：原生 C++ 接口、Python 执行/SSH 传输（如适用）、配置及原始构建文件。Python 与 INI 逐字节取自已签名发布包；文件来源及 SHA-256 见 [source-manifest.json](source-manifest.json)。

`../deps/` 保存依赖锁定、安装与来源说明；算法实现和模型按这些锁定记录从上游获取。PoSDK 主项目和其他插件的实现不在本目录中。

## 构建

使用与该插件 ABI 匹配的 **PoSDK Plugin SDK**；当前发布目标为 macOS arm64。原始 CMake 文件沿用发布时的开发目录布局。构建助手只在指定 build 目录中组合本目录、相邻 deps 和许可证，并调整 SDK include 路径及共享头文件位置。

在插件目录执行：

```bash
python3 src/build.py --sdk-root "/path/to/PoSDK Plugin SDK" --build-dir "/tmp/method_romav2-build"
```

只检查配置可追加 `--configure-only`。依赖运行环境由上一级 `install.sh` 和 `deps/` 定义；模型文件不包含在源码快照中。现有市场安装入口仍为签名 `.pospkg`。

## 许可与版本

保留[现有 LICENSE](../LICENSE)及[第三方声明](../THIRD_PARTY_NOTICES.md)。源码的可见性不额外授予这些文件未规定的许可；各上游作者与许可证保持原样。

本次提供源码阅读和独立构建入口；已发布安装包、签名、版本与原有分发元数据保持其发布记录。
