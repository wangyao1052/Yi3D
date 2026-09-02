"""测试 Draft 拔模"""
import wy3d
import math

db = wy3d.getActiveDatabase()

# 事务1: 创建基体
trans1 = db.getTransactionManager().startTransaction("create-box")
box = wy3d.Box.create(transaction=trans1, length=30.0, width=20.0, height=20.0)
trans1.addNewlyCreatedElement(box)
db.getTransactionManager().endTransaction()
print(f"Box created: id={box.getId().value()}")

# 事务2: 创建拔模
# 注意: angle 是弧度, kMaxDraftAngle = 89.9° ≈ 1.57 rad
trans2 = db.getTransactionManager().startTransaction("create-draft")
boxForWrite = trans2.getElementForWrite(box.getId())

angle_rad = math.radians(5.0)  # 5 度 → 0.087 弧度

found = False
for neutral_idx in range(6):
    for face_idx in range(6):
        if face_idx == neutral_idx:
            continue
        draft = wy3d.Draft.create(
            transaction=trans2,
            solid=boxForWrite,
            neutralFaceIndex=neutral_idx,
            faceIndices=[face_idx],
            angle=angle_rad
        )
        if draft is not None:
            trans2.addNewlyCreatedElement(draft)
            print(f"Draft created: neutralFace={neutral_idx}, draftFaces=[{face_idx}]")
            print(f"  angle={draft.getAngle():.4f} rad ({math.degrees(draft.getAngle()):.1f} deg)")
            print(f"  neutralFace={draft.getNeutralFace()}")
            print(f"  faces={draft.getFaces()}")
            found = True
            break
    if found:
        break

if found:
    db.getTransactionManager().endTransaction()
    print("Draft test PASSED")
else:
    db.getTransactionManager().abortTransaction()
    print("Draft test FAILED - no valid face indices found")
