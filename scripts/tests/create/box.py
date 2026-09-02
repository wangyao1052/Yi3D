import wy3d

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建长方体
box = wy3d.Box.create(transaction = trans, length = 10.0, width = 15.0, height = 5.0)
trans.addNewlyCreatedElement(box)

# 提交事务
db.getTransactionManager().endTransaction()

# 打印长方体的属性
print(f"Length = {box.getLength()}")
print(f"Width  = {box.getWidth()}")
print(f"Height = {box.getHeight()}")
