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
    arc = wy3d.SketchArc.create(trans, wy3d.Vector2(0.0, 0.0), 15.0, 0.0, wy3d.PI_2)
    trans.addNewlyCreatedElement(arc)
    sketch.addEntity(arc)
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
    # 将所有圆弧的圆心偏移(100.0, 100.0)
    # 将所有圆弧的半径增大一倍
    # 将所有圆弧的起始角度增大90度
    # 将所有圆弧的终止角度增大90度
    for entityId in sketch:
        entityForRead = db.getElement(entityId)
        if not isinstance(entityForRead, wy3d.SketchArc):
            continue
        arc = trans.getElementForWrite(entityId)
        arc.setCenter(arc.getCenter() + wy3d.Vector2(100.0, 100.0))
        arc.setRadius(arc.getRadius() * 2)
        arc.setStartAngle(arc.getStartAngle() + wy3d.PI_2)
        arc.setEndAngle(arc.getEndAngle() + wy3d.PI_2)
        #arc.setConstruction(True)

# 提交事务
db.getTransactionManager().endTransaction()


