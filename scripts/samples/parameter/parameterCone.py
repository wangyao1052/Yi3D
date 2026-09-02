import wy3d

db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建圆锥体
cone = wy3d.Cone.create(trans, radius = 10.0, height = 20.0)
trans.addNewlyCreatedElement(cone)

# 列举出所有参数
params = cone.listParameters()
print("wy3d.Cone")
for className, paramName in params:
    paramValue = cone.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 更改参数值
cone.setParameterValue(cone.getClassName(), "Radius", wy3d.ParameterValue.createDouble(15.0))
cone.setParameterValue(cone.getClassName(), "Height", wy3d.ParameterValue.createDouble(25.0))

# 列举出所有参数
params = cone.listParameters()
print("after setParameterValue")
for className, paramName in params:
    paramValue = cone.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 提交事务
db.getTransactionManager().endTransaction()