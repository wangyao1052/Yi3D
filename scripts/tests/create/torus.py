import wy3d

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建圆环体
torus = wy3d.Torus.create(transaction = trans, majorRadius = 20.0, minorRadius = 5.0)
trans.addNewlyCreatedElement(torus)

# 提交事务
db.getTransactionManager().endTransaction()

# 打印圆环体的属性
print(f"MajorRadius = {torus.getMajorRadius()}")
print(f"MinorRadius = {torus.getMinorRadius()}")
