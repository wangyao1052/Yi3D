import wy3d

# 获取当前选择集
ss = wy3d.getSelectionSet()

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 如果没有选中元素，先创建对应类型元素
if ss.getCount() == 0:
    obj = wy3d.Torus.create(transaction = trans, majorRadius = 20.0, minorRadius = 5.0)
    trans.addNewlyCreatedElement(obj)
    ss.clear()
    ss.add(wy3d.Selection(obj.getId()))

# 遍历当前选择集
for sel in ss:
    elemId = sel.getElementId()
    elem = trans.getElementForWrite(elemId)
    if not isinstance(elem, wy3d.Torus):
        continue
    torus = elem

    # 打印圆环体的属性
    print(torus.getClassName())
    print(f"MajorRadius = {torus.getMajorRadius()}")
    print(f"MinorRadius = {torus.getMinorRadius()}")

    # 更改长方体的属性
    torus.setMajorRadius(torus.getMajorRadius() + 10.0)
    torus.setMinorRadius(torus.getMinorRadius() + 5.0)

    # 打印圆环体的属性
    print("after modify")
    print(f"MajorRadius = {torus.getMajorRadius()}")
    print(f"MinorRadius = {torus.getMinorRadius()}")

# 提交事务
db.getTransactionManager().endTransaction()


