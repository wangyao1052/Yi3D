import wy3d

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建球体
sphere = wy3d.Sphere.create(transaction = trans, radius = 10.0)
trans.addNewlyCreatedElement(sphere)

# 提交事务
db.getTransactionManager().endTransaction()

# 打印球体的属性
print(f"Radius = {sphere.getRadius()}")
