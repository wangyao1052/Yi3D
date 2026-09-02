import wy3d

db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建圆柱体
cylinder = wy3d.Cylinder.create(trans, radius = 10.0, height = 100.0)
trans.addNewlyCreatedElement(cylinder)

# 列举出所有参数
params = cylinder.listParameters()
print("wy3d.Cylinder")
for className, paramName in params:
    paramValue = cylinder.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 更改参数值
cylinder.setParameterValue(cylinder.getClassName(), "Radius", wy3d.ParameterValue.createDouble(5.0))
cylinder.setParameterValue(cylinder.getClassName(), "Height", wy3d.ParameterValue.createDouble(20.0))

# 列举出所有参数
params = cylinder.listParameters()
print("after setParameterValue")
for className, paramName in params:
    paramValue = cylinder.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 提交事务
db.getTransactionManager().endTransaction()