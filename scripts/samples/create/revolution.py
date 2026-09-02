import wy3d

def createRevolution():
    # 获取当前文档数据库
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

    # 提交事务
    db.getTransactionManager().endTransaction()

# 创建旋转体
createRevolution()
