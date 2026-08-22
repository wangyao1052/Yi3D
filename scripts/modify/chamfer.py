import math
import wy3d


def createChamfer():
    """创建倒角特征 — 两步事务模式 (旧 5 参 API, 向后兼容)"""

    db = wy3d.getActiveDatabase()

    # ========== 事务1: 创建基体 ==========
    trans1 = db.getTransactionManager().startTransaction("create-box")
    box = wy3d.Box.create(transaction=trans1, length=30.0, width=20.0, height=10.0)
    trans1.addNewlyCreatedElement(box)
    db.getTransactionManager().endTransaction()
    print(f"Box created: id={box.getId().value()}")

    # ========== 事务2: 创建倒角 (需要基体拓扑已生成) ==========
    trans2 = db.getTransactionManager().startTransaction("create-chamfer")
    boxForWrite = trans2.getElementForWrite(box.getId())

    # 对指定边倒角
    chamfer = wy3d.Chamfer.create(
        transaction=trans2,
        solid=boxForWrite,
        faceIndices=[],
        edgeIndices=[0, 1, 2, 3],
        distance=2.0
    )

    if chamfer is None:
        print("❌ Chamfer 创建失败 — 边索引可能无效")
        db.getTransactionManager().abortTransaction()
        return

    trans2.addNewlyCreatedElement(chamfer)
    db.getTransactionManager().endTransaction()

    print(f"✅ Chamfer created: id={chamfer.getId().value()}")
    print(f"  className    = {chamfer.getClassName()}")
    print(f"  chamferType  = {chamfer.getChamferType()}")
    print(f"  distance1    = {chamfer.getDistance1()}")
    print(f"  distance2    = {chamfer.getDistance2()}")
    print(f"  angle (deg)  = {math.degrees(chamfer.getAngle()):.1f}")
    print(f"  isFlipped    = {chamfer.isFlipped()}")
    print(f"  edges        = {chamfer.getEdges()}")
    print(f"  faces        = {chamfer.getFaces()}")


def createChamferScanEdges():
    """扫描所有边索引，找到有效边并创建倒角"""

    db = wy3d.getActiveDatabase()

    # 事务1: 创建基体
    trans1 = db.getTransactionManager().startTransaction("create-box")
    box = wy3d.Box.create(transaction=trans1, length=40.0, width=30.0, height=20.0)
    trans1.addNewlyCreatedElement(box)
    db.getTransactionManager().endTransaction()
    print(f"Box created: id={box.getId().value()}")

    # 事务2: 扫描边索引
    trans2 = db.getTransactionManager().startTransaction("create-chamfer")
    boxForWrite = trans2.getElementForWrite(box.getId())

    valid_edges = []
    for edge_idx in range(12):
        chamfer = wy3d.Chamfer.create(
            transaction=trans2,
            solid=boxForWrite,
            faceIndices=[],
            edgeIndices=[edge_idx],
            distance=2.0
        )
        if chamfer is not None:
            valid_edges.append(edge_idx)
            trans2.addNewlyCreatedElement(chamfer)
            print(f"  ✅ 边索引 {edge_idx} — Chamfer 创建成功")
        else:
            print(f"  ⚠️  边索引 {edge_idx} — 无效")

    db.getTransactionManager().endTransaction()
    print(f"\n有效边索引: {valid_edges}")


def createChamferEqualDistance():
    """新 9 参 API: 等距倒角"""

    db = wy3d.getActiveDatabase()

    # 事务1: 创建基体
    trans1 = db.getTransactionManager().startTransaction("create-box")
    box = wy3d.Box.create(transaction=trans1, length=30.0, width=20.0, height=10.0)
    trans1.addNewlyCreatedElement(box)
    db.getTransactionManager().endTransaction()

    # 事务2: 创建倒角
    trans2 = db.getTransactionManager().startTransaction("create-chamfer-equal")
    boxForWrite = trans2.getElementForWrite(box.getId())

    chamfer = wy3d.Chamfer.create(
        transaction=trans2,
        solid=boxForWrite,
        faceIndices=[],
        edgeIndices=[0, 1, 2, 3],
        distance1=2.0,
        distance2=2.0,
        angle=math.radians(45.0),
        chamferType=wy3d.ChamferType.EqualDistance,
        isFlipped=False
    )

    if chamfer is None:
        print("❌ EqualDistance Chamfer 创建失败")
        db.getTransactionManager().abortTransaction()
        return

    trans2.addNewlyCreatedElement(chamfer)
    db.getTransactionManager().endTransaction()

    print(f"✅ EqualDistance Chamfer created: id={chamfer.getId().value()}")
    print(f"  chamferType  = {chamfer.getChamferType()}")
    print(f"  distance1    = {chamfer.getDistance1()}")
    print(f"  distance2    = {chamfer.getDistance2()}")
    print(f"  angle (deg)  = {math.degrees(chamfer.getAngle()):.1f}")
    print(f"  isFlipped    = {chamfer.isFlipped()}")


def createChamferTwoDistances():
    """新 9 参 API: 双距离倒角 (翻转: distance1 量在边的最后一个相邻面侧)"""

    db = wy3d.getActiveDatabase()

    # 事务1: 创建基体
    trans1 = db.getTransactionManager().startTransaction("create-box")
    box = wy3d.Box.create(transaction=trans1, length=30.0, width=20.0, height=10.0)
    trans1.addNewlyCreatedElement(box)
    db.getTransactionManager().endTransaction()

    # 事务2: 创建倒角
    trans2 = db.getTransactionManager().startTransaction("create-chamfer-two-dist")
    boxForWrite = trans2.getElementForWrite(box.getId())

    chamfer = wy3d.Chamfer.create(
        transaction=trans2,
        solid=boxForWrite,
        faceIndices=[],
        edgeIndices=[0, 1, 2, 3],
        distance1=2.0,
        distance2=4.0,
        angle=math.radians(45.0),
        chamferType=wy3d.ChamferType.DistanceDistance,
        isFlipped=True
    )

    if chamfer is None:
        print("❌ TwoDistances Chamfer 创建失败")
        db.getTransactionManager().abortTransaction()
        return

    trans2.addNewlyCreatedElement(chamfer)
    db.getTransactionManager().endTransaction()

    print(f"✅ TwoDistances Chamfer created: id={chamfer.getId().value()}")
    print(f"  chamferType  = {chamfer.getChamferType()}")
    print(f"  distance1    = {chamfer.getDistance1()}")
    print(f"  distance2    = {chamfer.getDistance2()}")
    print(f"  angle (deg)  = {math.degrees(chamfer.getAngle()):.1f}")
    print(f"  isFlipped    = {chamfer.isFlipped()}")


def createChamferDistanceAngle():
    """新 9 参 API: 距离+角度倒角 (角度为弧度)"""

    db = wy3d.getActiveDatabase()

    # 事务1: 创建基体
    trans1 = db.getTransactionManager().startTransaction("create-box")
    box = wy3d.Box.create(transaction=trans1, length=30.0, width=20.0, height=10.0)
    trans1.addNewlyCreatedElement(box)
    db.getTransactionManager().endTransaction()

    # 事务2: 创建倒角
    trans2 = db.getTransactionManager().startTransaction("create-chamfer-angle")
    boxForWrite = trans2.getElementForWrite(box.getId())

    chamfer = wy3d.Chamfer.create(
        transaction=trans2,
        solid=boxForWrite,
        faceIndices=[],
        edgeIndices=[0, 1, 2, 3],
        distance1=3.0,
        distance2=3.0,
        angle=math.radians(30.0),
        chamferType=wy3d.ChamferType.DistanceAngle,
        isFlipped=False
    )

    if chamfer is None:
        print("❌ DistanceAngle Chamfer 创建失败")
        db.getTransactionManager().abortTransaction()
        return

    trans2.addNewlyCreatedElement(chamfer)
    db.getTransactionManager().endTransaction()

    print(f"✅ DistanceAngle Chamfer created: id={chamfer.getId().value()}")
    print(f"  chamferType  = {chamfer.getChamferType()}")
    print(f"  distance1    = {chamfer.getDistance1()}")
    print(f"  distance2    = {chamfer.getDistance2()}")
    print(f"  angle (deg)  = {math.degrees(chamfer.getAngle()):.1f}")
    print(f"  isFlipped    = {chamfer.isFlipped()}")


# =============================================================================
if __name__ == "__main__":
    createChamfer()
    createChamferScanEdges()
    createChamferEqualDistance()
    createChamferTwoDistances()
    createChamferDistanceAngle()
