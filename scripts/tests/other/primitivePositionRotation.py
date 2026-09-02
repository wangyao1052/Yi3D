import wy3d

# 获取当前选择集
ss = wy3d.getSelectionSet()

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 打印选中的基础形体的位置和旋转
primitiveIds = set()
for sel in ss:
    elemId = sel.getElementId()
    elem = db.getElement(elemId)
    if elem is None:
        continue
    if isinstance(elem, wy3d.Primitive):
        primitiveIds.add(elemId)
        print(f"id = elem.getId()")
        print(f"  position = {elem.getPosition()}")
        print(f"  rotation = {elem.getRotation()}")

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 如果没有选中元素，先创建对应类型元素
if len(primitiveIds) == 0 and ss.getCount() == 0:
    obj = wy3d.Box.create(transaction = trans, length = 10.0, width = 10.0, height = 10.0)
    trans.addNewlyCreatedElement(obj)
    ss.clear()
    ss.add(wy3d.Selection(obj.getId()))
    # 重新收集基础形体的ID
    for sel in ss:
        elemId = sel.getElementId()
        elem = db.getElement(elemId)
        if elem is not None and isinstance(elem, wy3d.Primitive):
            primitiveIds.add(elemId)
            print(f"id = elem.getId()")
            print(f"  position = {elem.getPosition()}")
            print(f"  rotation = {elem.getRotation()}")

# 修改位置和旋转
for elemId in primitiveIds:
    primitive = trans.getElementForWrite(elemId)
    primitive.setPosition(primitive.getPosition() + wy3d.Vector3(10.0, 10.0, 10.0))
    primitive.setRotation(primitive.getRotation() + wy3d.Vector3(wy3d.PI_2, 0.0, 0.0))

# 提交事务
db.getTransactionManager().endTransaction()


    