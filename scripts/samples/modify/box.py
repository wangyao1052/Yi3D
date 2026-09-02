import wy3d

# 获取当前选择集
ss = wy3d.getSelectionSet()

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 如果没有选中元素，先创建对应类型元素
if ss.getCount() == 0:
    obj = wy3d.Box.create(transaction = trans, length = 10.0, width = 10.0, height = 10.0)
    trans.addNewlyCreatedElement(obj)
    ss.clear()
    ss.add(wy3d.Selection(obj.getId()))

# 遍历当前选择集
for sel in ss:
    elemId = sel.getElementId()
    elem = trans.getElementForWrite(elemId)
    if not isinstance(elem, wy3d.Box):
        continue
    box = elem

    # 打印长方体的属性
    print(box.getClassName())
    print(f"Length = {box.getLength()}")
    print(f"Width  = {box.getWidth()}")
    print(f"Height = {box.getHeight()}")

    # 更改长方体的属性
    box.setLength(box.getLength() + 10.0)
    box.setWidth(box.getWidth() + 10.0)
    box.setHeight(box.getHeight() + 10.0)

    # 打印长方体的属性
    print("after modify")
    print(f"Length = {box.getLength()}")
    print(f"Width  = {box.getWidth()}")
    print(f"Height = {box.getHeight()}")

# 提交事务
db.getTransactionManager().endTransaction()


