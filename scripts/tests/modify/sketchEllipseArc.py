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
    ellipseArc = wy3d.SketchEllipseArc.create(trans, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(20.0, 0.0), 0.5, 0.0, wy3d.PI)
    trans.addNewlyCreatedElement(ellipseArc)
    sketch.addEntity(ellipseArc)
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
    # 将所有椭圆弧的圆心偏移(100.0, 100.0)
    # 将所有椭圆弧的长轴半径增大一倍,短轴半径维持不变
    # 将所有椭圆弧的起始角度增加180度
    # 将所有椭圆弧的终止角度增加180度
    for entityId in sketch:
        entityForRead = db.getElement(entityId)
        if not isinstance(entityForRead, wy3d.SketchEllipseArc):
            continue
        ellipseArc = trans.getElementForWrite(entityId)
        ellipseArc.setCenter(ellipseArc.getCenter() + wy3d.Vector2(100.0, 100.0))
        ellipseArc.setMajorAxis(ellipseArc.getMajorAxis() * 2)
        ellipseArc.setRadiusRatio(ellipseArc.getRadiusRatio() * 0.5, False)
        ellipseArc.setStartAngle(ellipseArc.getStartAngle() + wy3d.PI)
        ellipseArc.setEndAngle(ellipseArc.getEndAngle() + wy3d.PI)
        #ellipseArc.setConstruction(True)

# 提交事务
db.getTransactionManager().endTransaction()


