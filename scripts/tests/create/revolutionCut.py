import wy3d

def createRevolutionCut():
    # 获取当前文档数据库
    db = wy3d.getActiveDatabase()

    # 开启事务
    trans = db.getTransactionManager().startTransaction()

    # 创建立方体
    boxLength = 100.0
    boxWidth = 100.0
    boxHeight = 200.0
    box = wy3d.Box.create(trans, boxLength, boxWidth, boxHeight)
    trans.addNewlyCreatedElement(box)

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
    center = wy3d.Vector2(20.0, 20.0)
    radius = 10.0
    circle1 = wy3d.SketchCircle.create(trans, center, radius)
    trans.addNewlyCreatedElement(circle1)
    sketch.addEntity(circle1)

    # 创建旋转体
    startAngle = 0.0
    endAngle = wy3d.PI
    revolution1 = wy3d.Revolution.createCut(trans, sketch, startAngle, endAngle, box)
    trans.addNewlyCreatedElement(revolution1)

    # 提交事务
    db.getTransactionManager().endTransaction()

# 创建旋转切除
createRevolutionCut()
