import wy3d

def createHelix():
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
    # 圆
    circle = wy3d.SketchCircle.create(transaction = trans, center = wy3d.Vector2(0.0, 0.0), radius = 20.0)
    trans.addNewlyCreatedElement(circle)
    sketch.addEntity(circle)
    # 螺旋线
    helix1 = wy3d.Helix.create(transaction = trans, sketch = sketch, pitch = 5.0, turns = 10, startAngle = wy3d.PI)
    trans.addNewlyCreatedElement(helix1)
    # 提交事务
    db.getTransactionManager().endTransaction()

# 创建螺旋线
createHelix()
