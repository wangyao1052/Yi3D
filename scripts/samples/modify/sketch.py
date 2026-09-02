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
    circle = wy3d.SketchCircle.create(trans, wy3d.Vector2(0.0, 0.0), 10.0)
    trans.addNewlyCreatedElement(circle)
    sketch.addEntity(circle)
    ellipse = wy3d.SketchEllipse.create(trans, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(20.0, 0.0), 0.5)
    trans.addNewlyCreatedElement(ellipse)
    sketch.addEntity(ellipse)
    ss.clear()
    ss.add(wy3d.Selection(sketch.getId()))

# 遍历当前选择集
for sel in ss:
    elemId = sel.getElementId()
    elem = trans.getElementForWrite(elemId)
    if not isinstance(elem, wy3d.Sketch):
        continue
    sketch = elem

    # 向草图中添加一个圆
    circle = wy3d.SketchCircle.create(trans, center = wy3d.Vector2(0.0, 0.0), radius = 10.0)
    trans.addNewlyCreatedElement(circle)
    sketch.addEntity(circle)

    # 删除草图中的所有椭圆
    for entityId in sketch:
        entityForRead = db.getElement(entityId)
        if not isinstance(entityForRead, wy3d.SketchEllipse):
            continue
        entity = trans.getElementForWrite(entityId)
        entity.erase()

# 提交事务
db.getTransactionManager().endTransaction()


