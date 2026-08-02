import wy3d


def createShell():
    """创建抽壳特征 — 移除顶面，向内抽壳 1mm"""

    db = wy3d.getActiveDatabase()

    # ========== 事务1: 创建基体 ==========
    trans1 = db.getTransactionManager().startTransaction("create-box")
    box = wy3d.Box.create(transaction=trans1, length=30.0, width=20.0, height=15.0)
    trans1.addNewlyCreatedElement(box)
    db.getTransactionManager().endTransaction()
    print(f"Box created: id={box.getId().value()}")

    # ========== 事务2: 创建抽壳 ==========
    trans2 = db.getTransactionManager().startTransaction("create-shell")
    boxForWrite = trans2.getElementForWrite(box.getId())

    # 移除顶面(索引1)，向内抽壳
    shell = wy3d.Shell.create(
        transaction=trans2,
        solid=boxForWrite,
        faceIndices=[1],  # 移除顶面
        thickness=1.0,
        direction=wy3d.ShellDirection.Inward
    )

    if shell is None:
        print("❌ Shell 创建失败 — 面索引可能无效")
        db.getTransactionManager().abortTransaction()
        return

    trans2.addNewlyCreatedElement(shell)
    db.getTransactionManager().endTransaction()

    print(f"✅ Shell created: id={shell.getId().value()}")
    print(f"  className    = {shell.getClassName()}")
    print(f"  thickness    = {shell.getThickness()}")
    print(f"  direction    = {shell.getDirection()}")
    print(f"  joinType     = {shell.getJoinType()}")
    print(f"  offsetMode   = {shell.getOffsetMode()}")
    print(f"  intersection = {shell.getIntersection()}")
    print(f"  faces        = {shell.getFaces()}")


def createShellScanFaces():
    """扫描面索引，找到有效面并创建开口抽壳"""

    db = wy3d.getActiveDatabase()

    # 事务1: 创建基体
    trans1 = db.getTransactionManager().startTransaction("create-box")
    box = wy3d.Box.create(transaction=trans1, length=40.0, width=30.0, height=20.0)
    trans1.addNewlyCreatedElement(box)
    db.getTransactionManager().endTransaction()
    print(f"Box created: id={box.getId().value()}")

    # 事务2: 逐一尝试面索引
    trans2 = db.getTransactionManager().startTransaction("create-shell")
    boxForWrite = trans2.getElementForWrite(box.getId())

    for face_idx in range(6):
        shell = wy3d.Shell.create(
            transaction=trans2,
            solid=boxForWrite,
            faceIndices=[face_idx],
            thickness=1.0,
            direction=wy3d.ShellDirection.Inward
        )
        if shell is not None:
            trans2.addNewlyCreatedElement(shell)
            print(f"  ✅ 面索引 {face_idx} — 开口抽壳成功 (thickness={shell.getThickness()})")
            # 找到一个就停，因为每个面只能抽一次
            break
    else:
        print("  ❌ 面索引 0~5 全部无效")

    db.getTransactionManager().endTransaction()


def createShellNoOpening():
    """不开口抽壳 — 形成封闭中空壳体"""

    db = wy3d.getActiveDatabase()

    # 事务1: 创建球体
    trans1 = db.getTransactionManager().startTransaction("create-sphere")
    sphere = wy3d.Sphere.create(transaction=trans1, radius=20.0)
    trans1.addNewlyCreatedElement(sphere)
    db.getTransactionManager().endTransaction()
    print(f"Sphere created: id={sphere.getId().value()}")

    # 事务2: 不开口，向外抽壳
    trans2 = db.getTransactionManager().startTransaction("create-shell")
    sphereForWrite = trans2.getElementForWrite(sphere.getId())

    shell = wy3d.Shell.create(
        transaction=trans2,
        solid=sphereForWrite,
        faceIndices=[],  # 不开口
        thickness=2.0,
        direction=wy3d.ShellDirection.Outward
    )

    if shell is None:
        print("❌ Shell 创建失败")
        db.getTransactionManager().abortTransaction()
        return

    trans2.addNewlyCreatedElement(shell)
    db.getTransactionManager().endTransaction()

    print(f"✅ Shell (closed, outward): id={shell.getId().value()}")
    print(f"  thickness    = {shell.getThickness()}")
    print(f"  direction    = {shell.getDirection()}")


# =============================================================================
if __name__ == "__main__":
    createShell()
