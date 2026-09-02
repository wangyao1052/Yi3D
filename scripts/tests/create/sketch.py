import wy3d

# 创建点
def createPoint(trans, sketch, position):
    point = wy3d.SketchPoint.create(trans, position)
    trans.addNewlyCreatedElement(point)
    sketch.addEntity(point)

# 创建直线段
def createLine(trans, sketch, startPnt, endPnt):
    line = wy3d.SketchLine.create(trans, startPnt, endPnt)
    trans.addNewlyCreatedElement(line)
    sketch.addEntity(line)

# 创建中心线
def createCenterLine(trans, sketch, startPnt, endPnt):
    centerLine = wy3d.SketchCenterLine.create(trans, startPnt, endPnt)
    trans.addNewlyCreatedElement(centerLine)
    sketch.addEntity(centerLine)

# 创建圆
def createCircle(trans, sketch, center, radius):
    circle = wy3d.SketchCircle.create(trans, center, radius)
    trans.addNewlyCreatedElement(circle)
    sketch.addEntity(circle)

# 创建圆弧
def createArc(trans, sketch, center, radius, startAngle, endAngle):
    arc = wy3d.SketchArc.create(trans, center, radius, startAngle, endAngle)
    trans.addNewlyCreatedElement(arc)
    sketch.addEntity(arc)

# 创建椭圆
def createEllipse(trans, sketch, center, majorAxis, radiusRatio):
    ellipse = wy3d.SketchEllipse.create(trans, center, majorAxis, radiusRatio)
    trans.addNewlyCreatedElement(ellipse)
    sketch.addEntity(ellipse)

# 创建椭圆弧
def createEllipseArc(trans, sketch, center, majorAxis, radiusRatio, startAngle, endAngle):
    ellipseArc = wy3d.SketchEllipseArc.create(trans, center, majorAxis, radiusRatio, startAngle, endAngle)
    trans.addNewlyCreatedElement(ellipseArc)
    sketch.addEntity(ellipseArc)

# 创建插值样条曲线
def createSplineByFitPoints(trans, sketch, fitPoints):
    spline = wy3d.SketchSpline.createByFitPoints(trans, fitPoints)
    trans.addNewlyCreatedElement(spline)
    sketch.addEntity(spline)

# 创建样条曲线
def createSplineByControlPoints(trans, sketch, degree, controlPoints):
    spline = wy3d.SketchSpline.createByControlPoints(trans, degree, controlPoints)
    trans.addNewlyCreatedElement(spline)
    sketch.addEntity(spline)

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建草图
plane = wy3d.SketchPlane(
    origin = wy3d.Vector3(0.0, 0.0, 0.0),
    normal = wy3d.Vector3(0.0, 0.0, 1.0),
    xDir = wy3d.Vector3(1.0, 0.0, 0.0))
sketch = wy3d.Sketch.create(trans, plane)
trans.addNewlyCreatedElement(sketch)

# 创建点
createPoint(trans, sketch, wy3d.Vector2(10.0, 10.0))

# 创建4条线组成矩形
createLine(trans, sketch, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(100.0, 0.0))
createLine(trans, sketch, wy3d.Vector2(100.0, 0.0), wy3d.Vector2(100.0, 50.0))
createLine(trans, sketch, wy3d.Vector2(100.0, 50.0), wy3d.Vector2(0.0, 50.0))
createLine(trans, sketch, wy3d.Vector2(0.0, 50.0), wy3d.Vector2(0.0, 0.0))

# 创建中心线
createCenterLine(trans, sketch, wy3d.Vector2(50.0, 0.0), wy3d.Vector2(50.0, 100.0))

# 创建圆
createCircle(trans, sketch, wy3d.Vector2(0.0, 0.0), 10.0)

# 创建圆弧
createArc(trans, sketch, wy3d.Vector2(0.0, 0.0), 15.0, 0.0, wy3d.PI_2)

# 创建椭圆
createEllipse(trans, sketch, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(10.0, 10.0), 0.5)

# 创建椭圆弧
createEllipseArc(trans, sketch, wy3d.Vector2(0.0, 0.0), wy3d.Vector2(20.0, 20.0), 0.5, 0.0, wy3d.PI)

# 创建插值样条曲线
createSplineByFitPoints(trans, sketch, [
    wy3d.Vector2(0.0, 0.0),
    wy3d.Vector2(50.0, 0.0),
    wy3d.Vector2(50.0, 50.0),
    wy3d.Vector2(0.0, 50.0)])

# 创建样条曲线
createSplineByControlPoints(trans, sketch, 3, [
    wy3d.Vector2(0.0, 0.0),
    wy3d.Vector2(100.0, 50.0),
    wy3d.Vector2(120.0, 50.0),
    wy3d.Vector2(100.0, 0.0)])

# 提交事务
db.getTransactionManager().endTransaction()
