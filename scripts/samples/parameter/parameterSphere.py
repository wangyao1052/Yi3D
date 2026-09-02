import wy3d

db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建球体
sphere = wy3d.Sphere.create(trans, radius = 10.0)
trans.addNewlyCreatedElement(sphere)

# 列举出所有参数
params = sphere.listParameters()
print("wy3d.Sphere")
for className, paramName in params:
    paramValue = sphere.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 更改参数值
sphere.setParameterValue(sphere.getClassName(), "Radius", wy3d.ParameterValue.createDouble(15.0))

# 列举出所有参数
params = sphere.listParameters()
print("after setParameterValue")
for className, paramName in params:
    paramValue = sphere.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 提交事务
db.getTransactionManager().endTransaction()