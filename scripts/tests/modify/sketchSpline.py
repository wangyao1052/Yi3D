import wy3d

# 获取当前选择集
ss = wy3d.getSelectionSet()

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 如果没有选中元素，先创建对应类型元素
if ss.getCount() == 0:
    plane = wy3d.SketchPlane(
        origin = wy3d.Vector3(0.0, 0.0, 0.0),
        normal = wy3d.Vector3(0.0, 0.0, 1.0),
        xDir = wy3d.Vector3(1.0, 0.0, 0.0))
    sketch = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch)
    points = [wy3d.Vector2(0.0, 0.0), wy3d.Vector2(50.0, 0.0), wy3d.Vector2(50.0, 50.0), wy3d.Vector2(0.0, 50.0)]
    spline = wy3d.SketchSpline.createByFitPoints(trans, points)
    trans.addNewlyCreatedElement(spline)
    sketch.addEntity(spline)
    ss.clear()
    ss.add(wy3d.Selection(sketch.getId()))

# 遍历当前选择集
for sel in ss:
    # 是否是草图
    elemId = sel.getElementId()
    elem = trans.getElementForWrite(elemId)
    if not isinstance(elem, wy3d.Sketch):
        continue
    sketch = elem

    # 遍历草图图元
    # 如果样条曲线是控制点模式,则阶数+1
    # 将所有样条曲线的点集偏移(10.0, 10.0)
    for entityId in sketch:
        entityForRead = db.getElement(entityId)
        if not isinstance(entityForRead, wy3d.SketchSpline):
            continue
        spline = trans.getElementForWrite(entityId)
        if wy3d.SplineMode.ControlPoints == spline.getMode(): # 控制点
            spline.setDegree(spline.getDegree() + 1)
        points = spline.getPoints()
        for point in points:
            point += wy3d.Vector2(10.0, 10.0)
        spline.setPoints(points)
        #spline.setConstruction(True)

# 提交事务
db.getTransactionManager().endTransaction()


