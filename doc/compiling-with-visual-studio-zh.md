# 使用 Visual Studio 编译 Yi3D

## 环境要求

| 项目 | 要求 |
|------|------|
| **操作系统** | Windows 7 或更高版本（64 位） |
| **Visual Studio** | Visual Studio 2019 / 2022 / 2026（Community / Professional / Enterprise） |
| **CMake** | 3.15 或更高版本（Visual Studio 自带的即可） |
| **编译器** | MSVC v142（VS 2019）、v143（VS 2022）或 v145（VS 2026），C++17 |

## 第一步：获取源码

```bash
git clone https://github.com/wangyao1052/Yi3D.git
```

---

## 第二步：下载依赖包

从 [https://github.com/wangyao1052/Yi3D-LibBundles-Windows/releases/tag/v1.0.1](https://github.com/wangyao1052/Yi3D-LibBundles-Windows/releases/tag/v1.0.1) 下载 `Yi3D-LibBundles-1.0.1.zip`，解压到 `Yi3D/3rdParty/bundles/`。

![](images/Yi3D-LibBundles.png)

解压后，项目目录结构如下：

```
Yi3D
├── 3rdParty/
│   └── bundles/
│       ├── occt/
│       ├── osg/
│       ├── python3/
│       ├── qt5/
│       └── wyaf/
├── inc/
├── src/
├── CMakeLists.txt
├── CMakeSettings.json
└── ...
```

---

## 第三步：使用 Visual Studio 打开项目

1. 启动 **Visual Studio**
2. 选择 **"打开本地文件夹"**（Open a local folder）
   - 或者通过菜单：**文件 → 打开 → 文件夹...**
3. 选择 Yi3D 项目的**根目录**（包含 `CMakeLists.txt` 的目录）

Visual Studio 会自动检测到 `CMakeSettings.json` 并解析其中的配置。

---

## 第四步：配置与编译

打开项目后，Visual Studio 会自动运行 CMake 生成。可以在输出窗口查看进度。

### 可用的生成配置

项目提供了两个预定义配置（在 `CMakeSettings.json` 中定义）：

| 配置名称 | 说明 | 生成器 |
|----------|------|--------|
| **x64-Debug** | 调试版本，包含调试符号，不优化 | Ninja |
| **x64-Release** | 发布版本，含优化但保留 PDB 调试信息 | Ninja |

### 编译

从菜单选择 **生成 → 生成解决方案**（或按 `Ctrl+Shift+B`）。

### 编译输出

编译产物将输出到 `out/<CMAKE_BUILD_TYPE>/` 目录：

```
out/
├── Debug/
│   ├── YI3D.exe         ← 主程序入口
│   ├── wy3d.dll
│   ├── wy3dPY.pyd
│   ├── unitTest.exe
│   ├── scripts/        (Python 脚本示例)
│   ├── samples/        (示例模型文件)
│   └── python3/        (Python 运行时，自动拷贝)
└── Release/
    ├── YI3D.exe         ← 主程序入口
    ├── wy3d.dll
    └── ...
```

CMake 中间生成文件放在 `out/build/<配置名称>/` 目录下，该目录已被 git 忽略。
