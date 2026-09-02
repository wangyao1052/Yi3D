import wy3d

db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建草图
plane = wy3d.SketchPlane(wy3d.Vector3(0.0, 0.0, 0.0), wy3d.Vector3(0.0, 0.0, 1.0), wy3d.Vector3(1.0, 0.0, 0.0))
sketch = wy3d.Sketch.create(trans, plane)
trans.addNewlyCreatedElement(sketch)

# 中心线
centerLine1 = wy3d.SketchCenterLine.create(trans, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(100.0, 0.0))
trans.addNewlyCreatedElement(centerLine1)
sketch.addEntity(centerLine1)
# 圆
circle1 = wy3d.SketchCircle.create(trans, wy3d.Vector2(20.0, 20.0), 5)
trans.addNewlyCreatedElement(circle1)
sketch.addEntity(circle1)

# 创建旋转体
revolution1 = wy3d.Revolution.create(trans, sketch, 0.0, wy3d.PI)
trans.addNewlyCreatedElement(revolution1)

# 列举出所有参数
params = revolution1.listParameters()
print("wy3d.Tube")
for className, paramName in params:
    paramValue = revolution1.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 更改参数值
revolution1.setParameterValue(revolution1.getClassName(), "StartAngle", wy3d.ParameterValue.createDouble(90.0))
revolution1.setParameterValue(revolution1.getClassName(), "EndAngle", wy3d.ParameterValue.createDouble(360.0))

# 列举出所有参数
params = revolution1.listParameters()
print("after setParameterValue")
for className, paramName in params:
    paramValue = revolution1.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 提交事务
db.getTransactionManager().endTransaction()