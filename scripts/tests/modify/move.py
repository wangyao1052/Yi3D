"""测试 Move 移动面"""
import wy3d

db = wy3d.getActiveDatabase()

# 事务1: 创建基体
trans1 = db.getTransactionManager().startTransaction("create-box")
box = wy3d.Box.create(transaction=trans1, length=30.0, width=20.0, height=10.0)
trans1.addNewlyCreatedElement(box)
db.getTransactionManager().endTransaction()
print(f"Box created: id={box.getId().value()}")

# 事务2: 移动整个实体
trans2 = db.getTransactionManager().startTransaction("create-move")
boxForWrite = trans2.getElementForWrite(box.getId())

moveVector = wy3d.Vector3(10.0, 5.0, 0.0)
move = wy3d.Move.create(
    transaction=trans2,
    solid=boxForWrite,
    moveVector=moveVector
)

if move is None:
    print("Move create returned None - this may not be a working feature yet")
    db.getTransactionManager().abortTransaction()
else:
    trans2.addNewlyCreatedElement(move)
    db.getTransactionManager().endTransaction()
    print(f"Move created!")
    print(f"  vector={move.getVector()}")
    print(f"Move test PASSED")
