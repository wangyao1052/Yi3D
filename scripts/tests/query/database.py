import wy3d

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 遍历当前数据库
for elemId in db:
    element = db.getElement(elemId)
    print(f"{elemId} {element}")
    # 打印所有参数值
    params = element.listParameters()
    for className, paramName in params:
        paramValue = element.getParameterValue(className, paramName)
        print(f"  {paramName:15} : {paramValue}")