import os
import wy3d


SAVE_FILE_PATH = r"D:\tmp\extrusion_demo.wy3d"


def createExtrusionAndSave():
    db = wy3d.getActiveDatabase()
    trans = db.getTransactionManager().startTransaction()

    plane = wy3d.SketchPlane(
        origin=wy3d.Vector3(0.0, 0.0, 0.0),
        normal=wy3d.Vector3(0.0, 0.0, 1.0),
        xDir=wy3d.Vector3(1.0, 0.0, 0.0),
    )
    sketch = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch)

    line1 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(100.0, 0.0))
    line2 = wy3d.SketchLine.create(trans, wy3d.Vector2(100.0, 0.0), wy3d.Vector2(100.0, 50.0))
    line3 = wy3d.SketchLine.create(trans, wy3d.Vector2(100.0, 50.0), wy3d.Vector2(0.0, 50.0))
    line4 = wy3d.SketchLine.create(trans, wy3d.Vector2(0.0, 50.0), wy3d.Vector2(0.0, 0.0))

    trans.addNewlyCreatedElement(line1)
    trans.addNewlyCreatedElement(line2)
    trans.addNewlyCreatedElement(line3)
    trans.addNewlyCreatedElement(line4)

    sketch.addEntity(line1)
    sketch.addEntity(line2)
    sketch.addEntity(line3)
    sketch.addEntity(line4)

    extrusion = wy3d.Extrusion.create(trans, sketch, 20.0)
    trans.addNewlyCreatedElement(extrusion)

    db.getTransactionManager().endTransaction()

    save_dir = os.path.dirname(SAVE_FILE_PATH)
    if save_dir:
        os.makedirs(save_dir, exist_ok=True)

    option = wy3d.WriteFileOption()
    option.fileType = wy3d.FileType.Text
    status = db.writeFile(SAVE_FILE_PATH, option)
    print(f"save status = {status}")
    print(f"saved to: {SAVE_FILE_PATH}")


createExtrusionAndSave()
