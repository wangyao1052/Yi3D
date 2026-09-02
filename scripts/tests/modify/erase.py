import wy3d

# 获取当前选择集
ss = wy3d.getSelectionSet()

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 如果没有选中元素，先创建任意元素
if ss.getCount() == 0:
    obj = wy3d.Box.create(transaction = trans, length = 10.0, width = 10.0, height = 10.0)
    trans.addNewlyCreatedElement(obj)
    ss.clear()
    ss.add(wy3d.Selection(obj.getId()))

# 遍历当前选择集获取所有选中的元素ID
idSet = set()
for sel in ss:
    elemId = sel.getElementId()
    idSet.add(elemId)

# 删除选中的元素
for elemId in idSet:
    elem = trans.getElementForWrite(elemId)
    if elem is None:
        continue
    elem.erase(True)

# 提交事务
db.getTransactionManager().endTransaction()
    

