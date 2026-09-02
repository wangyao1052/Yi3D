import wy3d

db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建长方体
box = wy3d.Box.create(trans, length = 10.0, width = 10.0, height = 10.0)
trans.addNewlyCreatedElement(box)

# 列举出所有参数
params = box.listParameters()
print("wy3d.Box")
for className, paramName in params:
    paramValue = box.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 更改参数值
box.setParameterValue(box.getClassName(), "Length", wy3d.ParameterValue.createDouble(100.0))
box.setParameterValue(box.getClassName(), "Width", wy3d.ParameterValue.createDouble(200.0))
box.setParameterValue(box.getClassName(), "Height", wy3d.ParameterValue.createDouble(300.0))

# 列举出所有参数
params = box.listParameters()
print("after setParameterValue")
for className, paramName in params:
    paramValue = box.getParameterValue(className, paramName)
    print(f"  {paramName}: {paramValue}")

# 提交事务
db.getTransactionManager().endTransaction()