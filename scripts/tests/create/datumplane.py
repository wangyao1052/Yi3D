import wy3d

def createDatumPlane():
    # 获取当前文档数据库
    db = wy3d.getActiveDatabase()
    # 开启事务
    trans = db.getTransactionManager().startTransaction()
    # 创建基准面
    datumPlane1 = wy3d.DatumPlane.create(trans, wy3d.SketchPlane(
        origin = wy3d.Vector3(0.0, 0.0, 100.0),
        normal = wy3d.Vector3(0.0, 0.0, 1.0),
        xDir = wy3d.Vector3(1.0, 0.0, 0.0)))
    trans.addNewlyCreatedElement(datumPlane1)
    # 提交事务
    db.getTransactionManager().endTransaction()

# 创建基准面
createDatumPlane()
