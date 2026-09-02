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
    circle = wy3d.SketchCircle.create(trans, wy3d.Vector2(0.0, 0.0), 20.0)
    trans.addNewlyCreatedElement(circle)
    sketch.addEntity(circle)
    obj = wy3d.Helix.create(transaction = trans, sketch = sketch, pitch = 5.0, turns = 10, startAngle = wy3d.PI)
    trans.addNewlyCreatedElement(obj)
    ss.clear()
    ss.add(wy3d.Selection(obj.getId()))

# 遍历当前选择集
for sel in ss:
    elemId = sel.getElementId()
    elem = trans.getElementForWrite(elemId)
    if not isinstance(elem, wy3d.Helix):
        continue
    helix = elem

    # 打印螺旋线的属性
    print(helix.getClassName())
    print(f"sketch = {helix.getSketch()}")
    print(f"pitch = {helix.getPitch()}")
    print(f"turns = {helix.getTurns()}")
    print(f"startAngle = {helix.getStartAngle()}")
    print(f"isClockWise = {helix.isClockWise()}")
    print(f"isReversed = {helix.isReversed()}")

    # 更改长方体的属性
    helix.setPitch(helix.getPitch() * 2)
    helix.setTurns(helix.getTurns() * 2)
    helix.setStartAngle(helix.getStartAngle() + wy3d.PI)
    helix.setClockWise(not helix.isClockWise())
    helix.setReversed(not helix.isReversed())

    # 打印螺旋线的属性
    print("after modify")
    print(f"sketch = {helix.getSketch()}")
    print(f"pitch = {helix.getPitch()}")
    print(f"turns = {helix.getTurns()}")
    print(f"startAngle = {helix.getStartAngle()}")
    print(f"isClockWise = {helix.isClockWise()}")
    print(f"isReversed = {helix.isReversed()}")

# 提交事务
db.getTransactionManager().endTransaction()


