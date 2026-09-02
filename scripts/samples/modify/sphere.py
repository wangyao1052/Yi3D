import wy3d

# 获取当前选择集
ss = wy3d.getSelectionSet()

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 如果没有选中元素，先创建对应类型元素
if ss.getCount() == 0:
    obj = wy3d.Sphere.create(transaction = trans, radius = 10.0)
    trans.addNewlyCreatedElement(obj)
    ss.clear()
    ss.add(wy3d.Selection(obj.getId()))

# 遍历当前选择集
for sel in ss:
    elemId = sel.getElementId()
    elem = trans.getElementForWrite(elemId)
    if not isinstance(elem, wy3d.Sphere):
        continue
    sphere = elem

    # 打印圆柱体的属性
    print(sphere.getClassName())
    print(f"Radius = {sphere.getRadius()}")

    # 更改长方体的属性
    sphere.setRadius(sphere.getRadius() + 5.0)

    # 打印圆柱体的属性
    print("after modify")
    print(f"Radius = {sphere.getRadius()}")

# 提交事务
db.getTransactionManager().endTransaction()


