import wy3d

# 构造函数
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
print(v1)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
print(v2)

# 零向量
print(f"wy3d.Vector3.kZero = {wy3d.Vector3.kZero}")
# X轴单位向量
print(f"wy3d.Vector3.kXAxis = {wy3d.Vector3.kXAxis}")
# Y轴单位向量
print(f"wy3d.Vector3.kYAxis = {wy3d.Vector3.kYAxis}")
# Z轴单位向量
print(f"wy3d.Vector3.kZAxis = {wy3d.Vector3.kZAxis}")

# 获取x&y&z分量
print(v1.x())
print(v1.y())
print(v1.z())

# 设置x&y&z分量
v1.set(1.1, 2.2, 3.3)
print(v1)
v2.setX(10.1)
v2.setY(20.1)
v2.setZ(30.1)

# 长度&长度的平方
print(v2)
print(v2.length())
print(v2.length2())

# 单位化向量
v2.normalize()
print(v2)
print(v2.length())
print(v2.length2())
v6 = v1.normalized() # 不改变v1
print(v6)

# 点积
v7 = wy3d.Vector3(1.1, 2.2, 3.3)
print(v7.dot(wy3d.Vector3.kXAxis))
print(v7.dot(wy3d.Vector3.kYAxis))
print(v7.dot(wy3d.Vector3.kZAxis))

# 叉积
print(wy3d.Vector3.kXAxis.cross(wy3d.Vector3.kYAxis))

# +=
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v1 += v2
print(v1)

# -=
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v1 -= v2
print(v1)

# *=
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v1 *= 5
print(v1)

# /=
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v1 /= 5
print(v1)

# ==
if v1 == v2:
  print("v1 == v2")
else:
  print("v1 != v2")
  
# +
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v3 = v1 + v2
print(v3)

# -
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v3 = v1 - v2
print(v3)

# *
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v3 = v1 * 10
print(v3)
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v3 = 10 * v1
print(v3)

# /
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v3 = v1 / 10
print(v3)

# 负
v1 = wy3d.Vector3(1.0, 2.0, 3.0)
v2 = wy3d.Vector3(10.0, 20.0, 30.0)
v3 = -v1
print(v3)

# 求向量间的夹角
print(wy3d.Vector2.angle(wy3d.Vector2.kXAxis, wy3d.Vector2.kYAxis))