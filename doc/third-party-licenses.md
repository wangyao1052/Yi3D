# yi3d 第三方代码许可说明

本项目（yi3d）主体代码采用 **Apache License 2.0** 开源。

`3rdParty/` 目录下的第三方代码采用各自的原始开源协议，不受 Apache 2.0 约束。具体如下：

---

## 第三方代码清单

### 1. RTree

- **路径**: `3rdParty/RTree/`
- **许可**: Public Domain / MIT 双协议
- **作者**: Yariv Barkan
- **说明**: 空间索引库，用于快速区域查询。在无法承认 Public Domain 的司法管辖区（如德国），以 MIT 协议发布。
- **商用**: 完全兼容，无需额外授权。

### 2. base64

- **路径**: `3rdParty/base64/`
- **许可**: zlib-like（René Nyffenegger 自定义宽松协议）
- **作者**: René Nyffenegger
- **说明**: Base64 编解码的 C++ 实现。
- **商用**: 完全兼容，需保留版权声明。修改版需标明修改。

### 3. osgQt

- **路径**: `3rdParty/osgQt/`
- **许可**: OSGPL（OpenSceneGraph Public License）
- **作者**: Wang Rui / OpenSceneGraph 社区
- **说明**: 将 OpenSceneGraph 渲染集成到 Qt 窗口的适配层。本项目对其源码有修改。
- **注意**: 此目录下的修改后代码仍保持 OSGPL 许可。OSGPL 基于 LGPL，允许链接使用而不要求主体代码以相同许可发布。

### 4. utfcpp

- **路径**: `3rdParty/utfcpp/`
- **许可**: Boost Software License 1.0 (BSL-1.0)
- **作者**: Nemanja Trifunovic
- **说明**: 轻量级 UTF-8 字符串处理库（header-only）。
- **商用**: 完全兼容，需保留版权声明和许可文本。

### 5. ModelRefine

- **路径**: 独立开源项目（构建产物安装到 `3rdParty/bundles/ModelRefine/`）
- **许可**: LGPL-2.1-or-later
- **作者**: 移植自 FreeCAD 1.1.3 的 `src/Mod/Part/App/modelRefine.*`（原作者 Thomas Anderson），Wang Yao 移植
- **说明**: 清理布尔并集后冗余分割面的动态链接库；源码见 https://github.com/wangyao1052/ModelRefine
- **商用**: 以独立 DLL 动态链接，符合 LGPL 条款。

---

## Apache 2.0 与其它许可的兼容性

以上所有第三方协议均与 Apache 2.0 兼容。Apache 2.0 与 MIT、BSD、zlib-like、LGPL/OSGPL 等宽松或弱 copyleft 协议可以并存于同一项目。

- **MIT / Public Domain / zlib-like / BSL-1.0**: 无任何冲突。可以整体项目以 Apache 2.0 发布，只需在发行版中保留各第三方的原始许可声明。
- **OSGPL（基于 LGPL）**: LGPL 允许你的主体代码使用任意许可（包括 Apache 2.0），但要求被修改的 LGPL 部分保持原许可。osgQt 在运行时以动态链接库（DLL）形式使用，符合 LGPL 条款。
- **LGPL-2.1-or-later（ModelRefine）**: 主体代码保持 Apache 2.0；ModelRefine 作为独立开源项目构建成 DLL，在运行时动态链接，符合 LGPL 条款。

---

## 实践说明

本项目遵循开源社区的标准做法（与 VS Code、Chromium、Qt、LLVM 等项目一致）：

1. **项目根目录 `LICENSE`** 文件声明主体代码的 Apache 2.0 许可
2. **`3rdParty/` 目录**中各组件保留原始协议头部声明
3. **本文件**作为第三方许可的汇总说明，在发行时一并提供

---

## 参考

- Apache 2.0 与其它协议的兼容性说明：https://www.apache.org/legal/resolved.html
- OpenSceneGraph 许可：https://www.openscenegraph.org/license/
- MIT 协议文本：https://choosealicense.com/licenses/mit/
