import wy3d

db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建圆管
tube = wy3d.Tube.create(trans, outerRadius = 10.0, innerRadius = 5.0, height = 20.0)
trans.addNewlyCreatedElement(tube)

# 列举出所有参数
params = tube.listParameters()
print("wy3d.Tube")
for className, paramName in params:
    paramValue = tube.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 更改参数值
tube.setParameterValue(tube.getClassName(), "OuterRadius", wy3d.ParameterValue.createDouble(20.0))
tube.setParameterValue(tube.getClassName(), "InnerRadius", wy3d.ParameterValue.createDouble(10.0))
tube.setParameterValue(tube.getClassName(), "Height", wy3d.ParameterValue.createDouble(50.0))

# 列举出所有参数
params = tube.listParameters()
print("after setParameterValue")
for className, paramName in params:
    paramValue = tube.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 提交事务
db.getTransactionManager().endTransaction()