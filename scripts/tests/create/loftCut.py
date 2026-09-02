import wy3d

def createLoftCut():
    # 获取当前文档数据库
    db = wy3d.getActiveDatabase()

    # 开启事务
    trans = db.getTransactionManager().startTransaction()

    # 创建圆柱体
    cylinder = wy3d.Cylinder.create(trans, 30.0, 50.0)
    trans.addNewlyCreatedElement(cylinder)

    # 创建轮廓草图1
    plane1 = wy3d.SketchPlane(
        origin = wy3d.Vector3(0.0, 0.0, 0.0),
        normal = wy3d.Vector3(0.0, 0.0, 1.0),
        xDir = wy3d.Vector3(1.0, 0.0, 0.0))
    sketch1 = wy3d.Sketch.create(trans, plane1)
    trans.addNewlyCreatedElement(sketch1)
    # 圆
    circle1 = wy3d.SketchCircle.create(trans, wy3d.Vector2(0.0, 0.0), 10.0)
    trans.addNewlyCreatedElement(circle1)
    sketch1.addEntity(circle1)

    # 创建轮廓草图2
    plane2 = wy3d.SketchPlane(
        origin = wy3d.Vector3(0.0, 0.0, 50.0),
        normal = wy3d.Vector3(0.0, 0.0, 1.0),
        xDir = wy3d.Vector3(1.0, 0.0, 0.0))
    sketch2 = wy3d.Sketch.create(trans, plane2)
    trans.addNewlyCreatedElement(sketch2)
    # 圆
    circle2 = wy3d.SketchCircle.create(trans, wy3d.Vector2(0.0, 0.0), 25.0)
    trans.addNewlyCreatedElement(circle2)
    sketch2.addEntity(circle2)

    # 放样体
    loft1 = wy3d.Loft.createCut(trans, [sketch1, sketch2], cylinder)
    trans.addNewlyCreatedElement(loft1)
    # 提交事务
    db.getTransactionManager().endTransaction()

# 创建放样切除
createLoftCut()
