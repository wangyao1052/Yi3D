import wy3d

# 3D草图作路径的扫掠体: 轮廓仍然是2D草图
def createSweep3D():
    # 获取当前文档数据库
    db = wy3d.getActiveDatabase()

    # 开启事务
    trans = db.getTransactionManager().startTransaction()

    # 创建路径(3D草图)
    pathSketch3D = wy3d.Sketch3D.create(trans)
    trans.addNewlyCreatedElement(pathSketch3D)
    # 线1: 沿Z轴向上
    line1 = wy3d.SketchLine3D.create(trans,
        wy3d.Vector3(0.0, 0.0, 0.0), wy3d.Vector3(0.0, 0.0, 100.0))
    trans.addNewlyCreatedElement(line1)
    pathSketch3D.addEntity(line1)
    # 线2: 转弯后沿X轴
    line2 = wy3d.SketchLine3D.create(trans,
        wy3d.Vector3(0.0, 0.0, 100.0), wy3d.Vector3(100.0, 0.0, 100.0))
    trans.addNewlyCreatedElement(line2)
    pathSketch3D.addEntity(line2)

    # 创建轮廓草图(在路径起点处, 与起始方向垂直)
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
    sweep1 = wy3d.Sweep.create(trans, pathSketch3D, profileSketch)
    trans.addNewlyCreatedElement(sweep1)

    # 提交事务
    db.getTransactionManager().endTransaction()

# 创建扫掠体
createSweep3D()
