import wy3d

def createUnion():
    # 获取当前文档数据库
    db = wy3d.getActiveDatabase()
    # 开启事务
    trans = db.getTransactionManager().startTransaction()
    # 创建立方体
    box1 = wy3d.Box.create(transaction = trans, length = 10.0, width = 15.0, height = 5.0)
    trans.addNewlyCreatedElement(box1)
    # 创建圆柱体
    cylinder1 = wy3d.Cylinder.create(transaction = trans, radius = 10.0, height = 20.0)
    trans.addNewlyCreatedElement(cylinder1)
    # 布尔并集
    union1 = wy3d.Union.create(trans, box1, [cylinder1])
    trans.addNewlyCreatedElement(union1)
    # 提交事务
    db.getTransactionManager().endTransaction()

# 创建布尔并集
createUnion()