import wy3d

# 获取当前选择集
ss = wy3d.getSelectionSet()

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 如果没有选中元素，先创建对应类型元素
if ss.getCount() == 0:
    obj = wy3d.Tube.create(transaction = trans, outerRadius = 20.0, innerRadius = 10.0, height = 50.0)
    trans.addNewlyCreatedElement(obj)
    ss.clear()
    ss.add(wy3d.Selection(obj.getId()))

# 遍历当前选择集
for sel in ss:
    elemId = sel.getElementId()
    elem = trans.getElementForWrite(elemId)
    if not isinstance(elem, wy3d.Tube):
        continue
    tube = elem

    # 打印圆环体的属性
    print(tube.getClassName())
    print(f"OuterRadius = {tube.getOuterRadius()}")
    print(f"InnerRadius = {tube.getInnerRadius()}")
    print(f"Height = {tube.getHeight()}")

    # 更改长方体的属性
    tube.setOuterRadius(tube.getOuterRadius() + 10.0)
    tube.setInnerRadius(tube.getInnerRadius() + 10.0)
    tube.setHeight(tube.getHeight() + 30.0)

    # 打印圆环体的属性
    print("after modify")
    print(f"OuterRadius = {tube.getOuterRadius()}")
    print(f"InnerRadius = {tube.getInnerRadius()}")
    print(f"Height = {tube.getHeight()}")

# 提交事务
db.getTransactionManager().endTransaction()


