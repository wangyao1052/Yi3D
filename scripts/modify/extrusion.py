import wy3d


def createAndModifyExtrusion():
    """创建对称拉伸体,再修改其方向与深度 — 两步事务模式"""

    db = wy3d.getActiveDatabase()

    # ========== 事务1: 创建对称拉伸体 ==========
    trans1 = db.getTransactionManager().startTransaction("create-extrusion")
    plane = wy3d.SketchPlane(
        origin=wy3d.Vector3(0.0, 0.0, 0.0),
        normal=wy3d.Vector3(0.0, 0.0, 1.0),
        xDir=wy3d.Vector3(1.0, 0.0, 0.0))
    sketch = wy3d.Sketch.create(trans1, plane)
    trans1.addNewlyCreatedElement(sketch)
    line1 = wy3d.SketchLine.create(trans1, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(100.0, 0.0))
    trans1.addNewlyCreatedElement(line1)
    sketch.addEntity(line1)
    line2 = wy3d.SketchLine.create(trans1, wy3d.Vector2(100.0, 0.0), wy3d.Vector2(100.0, 50.0))
    trans1.addNewlyCreatedElement(line2)
    sketch.addEntity(line2)
    line3 = wy3d.SketchLine.create(trans1, wy3d.Vector2(100.0, 50.0), wy3d.Vector2(0.0, 50.0))
    trans1.addNewlyCreatedElement(line3)
    sketch.addEntity(line3)
    line4 = wy3d.SketchLine.create(trans1, wy3d.Vector2(0.0, 50.0), wy3d.Vector2(0.0, 0.0))
    trans1.addNewlyCreatedElement(line4)
    sketch.addEntity(line4)
    extrusion = wy3d.Extrusion.create(trans1, sketch, wy3d.ExtrusionDirection.Symmetric, 40.0)
    trans1.addNewlyCreatedElement(extrusion)
    db.getTransactionManager().endTransaction()
    print(f"Extrusion created: id={extrusion.getId().value()}")
    print(f"  direction = {extrusion.getDirection()}")
    print(f"  depth     = {extrusion.getDepth()}")

    # ========== 事务2: 修改方向与深度 ==========
    trans2 = db.getTransactionManager().startTransaction("modify-extrusion")
    extrusionForWrite = trans2.getElementForWrite(extrusion.getId())
    extrusionForWrite.setDirection(wy3d.ExtrusionDirection.OneSide)
    extrusionForWrite.setDepth(30.0)
    db.getTransactionManager().endTransaction()

    print(f"Extrusion modified: id={extrusion.getId().value()}")
    print(f"  direction = {extrusion.getDirection()}")
    print(f"  depth     = {extrusion.getDepth()}")


createAndModifyExtrusion()
