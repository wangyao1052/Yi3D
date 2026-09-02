import wy3d

# 获取当前选择集
ss = wy3d.getSelectionSet()

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 如果没有选中元素，先创建对应类型元素
if ss.getCount() == 0:
    obj = wy3d.Cone.create(transaction = trans, radius = 10.0, height = 20.0)
    trans.addNewlyCreatedElement(obj)
    ss.clear()
    ss.add(wy3d.Selection(obj.getId()))

# 遍历当前选择集
for sel in ss:
    elemId = sel.getElementId()
    elem = trans.getElementForWrite(elemId)
    if not isinstance(elem, wy3d.Cone):
        continue
    cone = elem

    # 打印圆锥体的属性
    print(cone.getClassName())
    print(f"Radius = {cone.getRadius()}")
    print(f"Height = {cone.getHeight()}")

    # 更改圆锥体的属性
    cone.setRadius(cone.getRadius() + 5.0)
    cone.setHeight(cone.getHeight() + 10.0)

    # 打印圆柱体的属性
    print("after modify")
    print(f"Radius = {cone.getRadius()}")
    print(f"Height = {cone.getHeight()}")

# 提交事务
db.getTransactionManager().endTransaction()


