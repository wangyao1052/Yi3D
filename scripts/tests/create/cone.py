import wy3d

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建圆锥体
cone = wy3d.Cone.create(transaction = trans, radius = 10.0, height = 20.0)
trans.addNewlyCreatedElement(cone)

# 提交事务
db.getTransactionManager().endTransaction()

# 打印圆锥体的属性
print(f"Radius = {cone.getRadius()}")
print(f"Height = {cone.getHeight()}")
