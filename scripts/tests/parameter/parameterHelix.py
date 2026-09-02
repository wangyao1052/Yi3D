import wy3d

db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建草图
plane = wy3d.SketchPlane(
    origin = wy3d.Vector3(0.0, 0.0, 0.0),
    normal = wy3d.Vector3(0.0, 0.0, 1.0),
    xDir = wy3d.Vector3(1.0, 0.0, 0.0))
sketch = wy3d.Sketch.create(trans, plane)
trans.addNewlyCreatedElement(sketch)
# 圆
circle = wy3d.SketchCircle.create(transaction = trans, center = wy3d.Vector2(0.0, 0.0), radius = 20.0)
trans.addNewlyCreatedElement(circle)
sketch.addEntity(circle)

# 螺旋线
helix1 = wy3d.Helix.create(transaction = trans, sketch = sketch, pitch = 5.0, turns = 10, startAngle = wy3d.PI)
trans.addNewlyCreatedElement(helix1)

# 列举出所有参数
params = helix1.listParameters()
print("wy3d.Helix")
for className, paramName in params:
    paramValue = helix1.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 更改参数值
helix1.setParameterValue(helix1.getClassName(), "Pitch", wy3d.ParameterValue.createDouble(15.0))
helix1.setParameterValue(helix1.getClassName(), "Turns", wy3d.ParameterValue.createDouble(5.0))
helix1.setParameterValue(helix1.getClassName(), "StartAngle", wy3d.ParameterValue.createDouble(0.0))
helix1.setParameterValue(helix1.getClassName(), "IsClockWise", wy3d.ParameterValue.createBoolean(True))
helix1.setParameterValue(helix1.getClassName(), "IsReversed", wy3d.ParameterValue.createBoolean(True))

# 列举出所有参数
params = helix1.listParameters()
print("after setParameterValue")
for className, paramName in params:
    paramValue = helix1.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 提交事务
db.getTransactionManager().endTransaction()