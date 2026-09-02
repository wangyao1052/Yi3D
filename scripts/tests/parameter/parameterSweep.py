import wy3d

db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建路径草图
pathPlane = wy3d.SketchPlane(
    origin = wy3d.Vector3(0.0, 0.0, 0.0),
    normal = wy3d.Vector3(0.0, -1.0, 0.0),
    xDir = wy3d.Vector3(1.0, 0.0, 0.0))
pathSketch = wy3d.Sketch.create(trans, pathPlane)
trans.addNewlyCreatedElement(pathSketch)
# 线1
line1 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(0.0, 100.0))
trans.addNewlyCreatedElement(line1)
pathSketch.addEntity(line1)
# 线2
line2 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 100.0), wy3d.Vector2(100.0, 100.0))
trans.addNewlyCreatedElement(line2)
pathSketch.addEntity(line2)

# 创建轮廓草图
profilePlane = wy3d.SketchPlane(
    origin = wy3d.Vector3(0.0, 0.0, 0.0),
    normal = wy3d.Vector3(0.0, 0.0, 1.0),
    xDir = wy3d.Vector3(1.0, 0.0, 0.0))
profileSketch = wy3d.Sketch.create(trans, profilePlane)
trans.addNewlyCreatedElement(profileSketch)
# 圆
circle1 = wy3d.SketchCircle.create(trans, wy3d.Vector2(0.0, 0.0), 10.0)
trans.addNewlyCreatedElement(circle1)
profileSketch.addEntity(circle1)

# 扫掠体
sweep1 = wy3d.Sweep.create(trans, pathSketch, profileSketch)
trans.addNewlyCreatedElement(sweep1)

# 列举出所有参数
params = sweep1.listParameters()
print("wy3d.Tube")
for className, paramName in params:
    paramValue = sweep1.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 提交事务
db.getTransactionManager().endTransaction()