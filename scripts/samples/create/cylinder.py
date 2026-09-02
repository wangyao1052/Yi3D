import wy3d

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建圆柱体
cylinder = wy3d.Cylinder.create(transaction = trans, radius = 10.0, height = 50.0)
trans.addNewlyCreatedElement(cylinder)

# 提交事务
db.getTransactionManager().endTransaction()

# 打印圆柱体的属性
print(f"Radius = {cylinder.getRadius()}")
print(f"Height  = {cylinder.getHeight()}")
