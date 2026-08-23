import wy3d

def createExtrudedSheet():
    # 获取当前文档数据库
    db = wy3d.getActiveDatabase()
    # 开启事务
    trans = db.getTransactionManager().startTransaction()
    # 创建草图(XY 平面上的开放 U 形链)
    plane = wy3d.SketchPlane(
        origin = wy3d.Vector3(0.0, 0.0, 0.0),
        normal = wy3d.Vector3(0.0, 0.0, 1.0),
        xDir = wy3d.Vector3(1.0, 0.0, 0.0))
    sketch = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch)
    # U 形:底边 + 两侧
    line1 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(100.0, 0.0))
    trans.addNewlyCreatedElement(line1)
    sketch.addEntity(line1)
    line2 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(0.0, 50.0))
    trans.addNewlyCreatedElement(line2)
    sketch.addEntity(line2)
    line3 = wy3d.SketchLine.create(trans, wy3d.Vector2(100.0, 0.0), wy3d.Vector2(100.0, 50.0))
    trans.addNewlyCreatedElement(line3)
    sketch.addEntity(line3)
    # 拉伸曲面(单侧)
    sheet1 = wy3d.ExtrudedSheet.create(trans, sketch, 20.0)
    trans.addNewlyCreatedElement(sheet1)
    # 对称拉伸曲面:再建一个草图,总深度 40,沿法向两侧各 20
    sketch2 = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch2)
    line4 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 100.0), wy3d.Vector2(100.0, 100.0))
    trans.addNewlyCreatedElement(line4)
    sketch2.addEntity(line4)
    line5 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 100.0), wy3d.Vector2(0.0, 150.0))
    trans.addNewlyCreatedElement(line5)
    sketch2.addEntity(line5)
    line6 = wy3d.SketchLine.create(trans, wy3d.Vector2(100.0, 100.0), wy3d.Vector2(100.0, 150.0))
    trans.addNewlyCreatedElement(line6)
    sketch2.addEntity(line6)
    sheet2 = wy3d.ExtrudedSheet.create(trans, sketch2, wy3d.ExtrusionDirection.Symmetric, 40.0)
    trans.addNewlyCreatedElement(sheet2)
    # 提交事务
    db.getTransactionManager().endTransaction()

# 创建拉伸曲面
createExtrudedSheet()
