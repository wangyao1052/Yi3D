# 基础形体坐标系与变换

来源：`src/wy3d/primitives/*.cpp` 中各 `generateOriginalShape()` 实现，以及 `src/wy3d/utils/OccUtil.cpp` 中 `transformShape()`。

## 1. 各形体局部坐标系原点

每个基础形体首先生成"原始形状"（`generateOriginalShape()`），此时形体位于自身局部坐标系中，原点位置由底层 OpenCASCADE `BRepPrimAPI_Make*` 决定。

### Box 立方体

```
BRepPrimAPI_MakeBox(L, W, H)
原点 = 立方体一角（X/Y/Z 最小值顶点）

        Z
        ↑  (L,W,H)
        ┌──────┐
       /      /│
      └──────┘ │
      │      │ │
      │      │ ┘
  (0,0,0)───┘
     原点 ●
```

- 立方体从 `(0, 0, 0)` 延伸到 `(L, W, H)`
- 原点位于底面-左-前角落

### Cylinder 圆柱体

```
BRepPrimAPI_MakeCylinder(R, H)
原点 = 底面圆心

      Z ↑
        │   ┌───┐  Z=H (顶面)
        │   │   │
        │   │   │
        │   └───┘  Z=0 (底面)
        └───●──→ X (底面圆心=原点)
```

- 圆柱沿 +Z 轴延伸，底面在 Z=0，顶面在 Z=H

### Sphere 球体

```
BRepPrimAPI_MakeSphere(R)
原点 = 球心

      Z ↑
        │    ╭───╮
        │   ╱     ╲
        │  │   ●   │  (球心=原点)
        │   ╲     ╱
        │    ╰───╯
        └──────→ X
```

- 球体关于原点中心对称

### Cone 圆锥体

```
BRepPrimAPI_MakeCone(R, 0, H)
原点 = 底面圆心

      Z ↑
        │     ●  Z=H (顶点)
        │    ╱ ╲
        │   ╱   ╲
        │  ┌─────┐  Z=0 (底面半径 R)
        └──●─────→ X (底面圆心=原点)
```

- 底面半径 = R，顶面半径 = 0（尖锥）
- 沿 +Z 轴延伸，底面在 Z=0，顶点在 Z=H

### Torus 圆环体

```
BRepPrimAPI_MakeTorus(R₁, R₂)
原点 = 圆环中心（环的空洞中心）

      Z ↑ (环的中心轴)
        │
     ╭──┼──╮  ← 圆环管截面 (R₂)
    ╱   │   ╲
   │    ●─────→ X  (原点=环中心, 环半径=R₁)
    ╲       ╱
     ╰─────╯  (平放于 XY 平面)
```

- 圆环平放于 XY 平面，中心轴沿 Z 方向
- R₁ = 主半径（环半径），R₂ = 副半径（管半径）
- 约束：R₂ ≤ R₁

### Tube 圆管

```
自定义：底面同心圆环 + 沿 Z 拉伸
原点 = 底面圆环中心

      Z ↑
        │  ╭─┬─╮  Z=H (顶面)
        │  │ │ │
        │  │ │ │  (外径 R_outer, 内径 R_inner)
        │  ╰─┴─╯  Z=0 (底面)
        └───●──→ X (底面圆环中心=原点)
```

- 沿 +Z 轴延伸，底面在 Z=0，顶面在 Z=H
- 约束：R_inner < R_outer

### 汇总

| 形体 | 局部原点位置 | 延展方向 |
|------|-------------|----------|
| Box | 一角（X/Y/Z 最小值顶点） | +X, +Y, +Z |
| Cylinder | 底面圆心（Z=0） | +Z |
| Sphere | 球心 | 关于原点对称 |
| Cone | 底面圆心（Z=0） | +Z（顶点在 Z=H） |
| Torus | 圆环中心 | 平放于 XY 平面，轴沿 Z |
| Tube | 底面圆环中心（Z=0） | +Z |

## 2. 变换叠加

原始形状生成后，`Primitive::generateShape()` 调用 `OccUtil::transformShape()` 叠加位置和旋转：

```cpp
// src/wy3d/utils/OccUtil.cpp:30-66
TopoDS_Shape OccUtil::transformShape(
    const TopoDS_Shape& shape,
    const wy::Vector3& position,
    const wy::Vector3& rotation)
{
    gp_Trsf transform;
    gp_Vec translation(position.x(), position.y(), position.z());
    transform.SetTranslationPart(translation);

    // euler angles: z-->x-->y
    gp_Quaternion rotZ;
    rotZ.SetVectorAndAngle(gp_Vec(0.0, 0.0, 1.0), rotation.z());
    gp_Quaternion rotX;
    rotX.SetVectorAndAngle(gp_Vec(1.0, 0.0, 0.0), rotation.x());
    gp_Quaternion rotY;
    rotY.SetVectorAndAngle(gp_Vec(0.0, 1.0, 0.0), rotation.y());
    quaternion = rotY * rotX * rotZ;  // OCCT 四元数右结合：先Z→再X→后Y

    transform.SetRotationPart(quaternion);
    return shape.Located(TopLoc_Location(transform));
}
```

### 旋转顺序：Z → X → Y

`rotation` 参数为 Euler 角（**弧度制**），应用顺序为：

```
1. 先绕 Z 轴旋转  rotation.z
2. 再绕 X 轴旋转  rotation.x
3. 最后绕 Y 轴旋转 rotation.y
```

### 完整变换流程

```
原始形体 (局部坐标系原点如上表)
   →  绕 Z 轴旋转  rotation.z
   →  绕 X 轴旋转  rotation.x
   →  绕 Y 轴旋转  rotation.y
   →  平移到 position
   →  最终世界空间形体
```

## 3. setRotation 参数映射

| 参数 | 对应轴 | 应用顺序 |
|------|--------|----------|
| `rx` | 绕 **X** 轴旋转 | 第 2 个应用 |
| `ry` | 绕 **Y** 轴旋转 | 第 3 个应用 |
| `rz` | 绕 **Z** 轴旋转 | 第 1 个应用 |

旋转角均为**弧度制**。最终应用顺序为 **Z → X → Y**（`rz` → `rx` → `ry`），即先绕 Z、再绕 X、最后绕 Y。
