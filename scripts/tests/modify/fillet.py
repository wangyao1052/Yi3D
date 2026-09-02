import wy3d


def createFillet():
    """创建倒圆角特征 — 两步事务模式"""

    db = wy3d.getActiveDatabase()

    # ========== 事务1: 创建基体 ==========
    trans1 = db.getTransactionManager().startTransaction("create-box")
    box = wy3d.Box.create(transaction=trans1, length=30.0, width=20.0, height=10.0)
    trans1.addNewlyCreatedElement(box)
    db.getTransactionManager().endTransaction()
    print(f"Box created: id={box.getId().value()}")

    # ========== 事务2: 创建倒圆角 (需要基体拓扑已生成) ==========
    trans2 = db.getTransactionManager().startTransaction("create-fillet")

    # 重新获取 box 的写入权限
    boxForWrite = trans2.getElementForWrite(box.getId())

    # 对指定边倒圆角
    # faceIndices=[]  edgeIndices=[0,1,2,3] 对前4条边倒圆角
    fillet = wy3d.Fillet.create(
        transaction=trans2,
        solid=boxForWrite,
        faceIndices=[],
        edgeIndices=[0, 1, 2, 3],
        radius=2.0
    )

    if fillet is None:
        print("❌ Fillet 创建失败 — 边索引可能无效，尝试从 0 开始逐条测试")
        db.getTransactionManager().abortTransaction()
        return

    trans2.addNewlyCreatedElement(fillet)
    db.getTransactionManager().endTransaction()

    print(f"✅ Fillet created: id={fillet.getId().value()}")
    print(f"  className   = {fillet.getClassName()}")
    print(f"  radius      = {fillet.getRadius()}")
    print(f"  edges       = {fillet.getEdges()}")
    print(f"  faces       = {fillet.getFaces()}")


def createFilletAllEdges():
    """对所有边倒圆角 — 逐条边索引尝试"""

    db = wy3d.getActiveDatabase()

    # 事务1: 创建基体
    trans1 = db.getTransactionManager().startTransaction("create-box")
    box = wy3d.Box.create(transaction=trans1, length=40.0, width=30.0, height=20.0)
    trans1.addNewlyCreatedElement(box)
    db.getTransactionManager().endTransaction()
    print(f"Box created: id={box.getId().value()}")

    # 事务2: 逐边尝试倒圆角
    trans2 = db.getTransactionManager().startTransaction("create-fillet")
    boxForWrite = trans2.getElementForWrite(box.getId())

    # 逐一尝试边索引 0~11，找到哪些有效
    valid_edges = []
    for edge_idx in range(12):
        fillet = wy3d.Fillet.create(
            transaction=trans2,
            solid=boxForWrite,
            faceIndices=[],
            edgeIndices=[edge_idx],
            radius=3.0
        )
        if fillet is not None:
            valid_edges.append(edge_idx)
            trans2.addNewlyCreatedElement(fillet)
            print(f"  ✅ 边索引 {edge_idx} — Fillet 创建成功")
        else:
            print(f"  ⚠️  边索引 {edge_idx} — 无效")

    db.getTransactionManager().endTransaction()
    print(f"\n有效边索引: {valid_edges}")


def createFilletOnExistingSolid():
    """对场景中已存在的实体创建倒圆角"""
    ss = wy3d.getSelectionSet()
    if ss.getCount() == 0:
        print("❌ 请先在场景中选中一个实体")
        return

    db = wy3d.getActiveDatabase()
    trans = db.getTransactionManager().startTransaction("create-fillet")

    for sel in ss:
        elemId = sel.getElementId()
        elem = trans.getElementForWrite(elemId)

        if not isinstance(elem, wy3d.Solid):
            print(f"  ⚠️  跳过: {elem.getClassName()} (非 Solid)")
            continue

        solid = elem

        # 尝试每条边，成功一条就继续
        success = False
        for edge_idx in range(12):
            fillet = wy3d.Fillet.create(
                transaction=trans,
                solid=solid,
                faceIndices=[],
                edgeIndices=[edge_idx],
                radius=2.0
            )
            if fillet is not None:
                trans.addNewlyCreatedElement(fillet)
                print(f"✅ Fillet on edge {edge_idx}: "
                      f"solid={solid.getClassName()}(id={elemId.value()}), "
                      f"radius={fillet.getRadius()}")
                success = True
                break

        if not success:
            print(f"❌ 边索引 0~11 全部无效，尝试用面索引。solid={solid.getClassName()}")

    db.getTransactionManager().endTransaction()


# =============================================================================
if __name__ == "__main__":
    createFillet()
