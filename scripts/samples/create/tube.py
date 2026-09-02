import wy3d

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建圆管体
tube = wy3d.Tube.create(transaction = trans, outerRadius = 20.0, innerRadius = 10.0, height = 50.0)
trans.addNewlyCreatedElement(tube)

# 提交事务
db.getTransactionManager().endTransaction()

# 打印圆管体的属性
print(f"OuterRadius = {tube.getOuterRadius()}")
print(f"InnerRadius = {tube.getInnerRadius()}")
print(f"Height = {tube.getHeight()}")
