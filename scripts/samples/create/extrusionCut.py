import wy3d

def createExtrusionCut():
    # 获取当前文档数据库
    db = wy3d.getActiveDatabase()
    # 开启事务
    trans = db.getTransactionManager().startTransaction()
    # 创建立方体
    box = wy3d.Box.create(trans, 100.0, 100.0, 200.0)
    trans.addNewlyCreatedElement(box)
    # 创建草图
    plane = wy3d.SketchPlane(
        origin = wy3d.Vector3(0.0, 0.0, 0.0),
        normal = wy3d.Vector3(0.0, 0.0, 1.0),
        xDir = wy3d.Vector3(1.0, 0.0, 0.0))
    sketch = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch)
    # 线1
    line1 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(100.0, 0.0))
    trans.addNewlyCreatedElement(line1)
    sketch.addEntity(line1)
    # 线2
    line2 = wy3d.SketchLine.create(trans, wy3d.Vector2(100.0, 0.0), wy3d.Vector2(100.0, 50.0))
    trans.addNewlyCreatedElement(line2)
    sketch.addEntity(line2)
    # 线3
    line3 = wy3d.SketchLine.create(trans, wy3d.Vector2(100.0, 50.0), wy3d.Vector2(0.0, 50.0))
    trans.addNewlyCreatedElement(line3)
    sketch.addEntity(line3)
    # 线4
    line4 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 50.0), wy3d.Vector2(0.0, 0.0))
    trans.addNewlyCreatedElement(line4)
    sketch.addEntity(line4)
    # 拉伸体(切除,单侧)
    extrusion1 = wy3d.Extrusion.createCut(trans, sketch, 20.0, box)
    trans.addNewlyCreatedElement(extrusion1)
    # 对称切除:再建一个草图,总深度 30,沿法向两侧各 15
    sketch2 = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch2)
    line5 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 100.0), wy3d.Vector2(100.0, 100.0))
    trans.addNewlyCreatedElement(line5)
    sketch2.addEntity(line5)
    line6 = wy3d.SketchLine.create(trans, wy3d.Vector2(100.0, 100.0), wy3d.Vector2(100.0, 150.0))
    trans.addNewlyCreatedElement(line6)
    sketch2.addEntity(line6)
    line7 = wy3d.SketchLine.create(trans, wy3d.Vector2(100.0, 150.0), wy3d.Vector2(0.0, 150.0))
    trans.addNewlyCreatedElement(line7)
    sketch2.addEntity(line7)
    line8 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 150.0), wy3d.Vector2(0.0, 100.0))
    trans.addNewlyCreatedElement(line8)
    sketch2.addEntity(line8)
    extrusion2 = wy3d.Extrusion.createCut(trans, sketch2, wy3d.ExtrusionDirection.Symmetric, 30.0, box)
    trans.addNewlyCreatedElement(extrusion2)
    # 提交事务
    db.getTransactionManager().endTransaction()

# 创建拉伸切除
createExtrusionCut()
