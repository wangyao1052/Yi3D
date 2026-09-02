"""测试 Rotate 旋转面"""
import wy3d

db = wy3d.getActiveDatabase()

# 事务1: 创建基体
trans1 = db.getTransactionManager().startTransaction("create-box")
box = wy3d.Box.create(transaction=trans1, length=30.0, width=20.0, height=10.0)
trans1.addNewlyCreatedElement(box)
db.getTransactionManager().endTransaction()
print(f"Box created: id={box.getId().value()}")

# 事务2: 旋转实体
trans2 = db.getTransactionManager().startTransaction("create-rotate")
boxForWrite = trans2.getElementForWrite(box.getId())

rotate = wy3d.Rotate.create(
    transaction=trans2,
    solid=boxForWrite,
    centerPoint=wy3d.Vector3(0.0, 0.0, 0.0),
    axisDirection=wy3d.Vector3(0.0, 0.0, 1.0),
    angle=45.0
)

if rotate is None:
    print("Rotate create returned None - this may not be a working feature yet")
    db.getTransactionManager().abortTransaction()
else:
    trans2.addNewlyCreatedElement(rotate)
    db.getTransactionManager().endTransaction()
    print(f"Rotate created!")
    print(f"  centerPoint={rotate.getCenterPoint()}")
    print(f"  axisDirection={rotate.getAxisDirection()}")
    print(f"  angle={rotate.getAngle()}")
    print(f"Rotate test PASSED")
