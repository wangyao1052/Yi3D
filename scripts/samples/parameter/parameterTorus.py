import wy3d

db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建圆环体
torus = wy3d.Torus.create(trans, majorRadius = 10.0, minorRadius = 2.0)
trans.addNewlyCreatedElement(torus)

# 列举出所有参数
params = torus.listParameters()
print("wy3d.Torus")
for className, paramName in params:
    paramValue = torus.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 更改参数值
torus.setParameterValue(torus.getClassName(), "MajorRadius", wy3d.ParameterValue.createDouble(20.0))
torus.setParameterValue(torus.getClassName(), "MinorRadius", wy3d.ParameterValue.createDouble(5.0))

# 列举出所有参数
params = torus.listParameters()
print("after setParameterValue")
for className, paramName in params:
    paramValue = torus.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 提交事务
db.getTransactionManager().endTransaction()