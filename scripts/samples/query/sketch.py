import wy3d

def printSketchPoint(sketchPoint):
    print(f"position = {sketchPoint.getPosition()}")

def printSketchLine(sketchLine):
    print(f"isConstruction = {sketchLine.isConstruction()}")
    print(f"startPoint = {sketchLine.getStartPoint()}")
    print(f"endPoint = {sketchLine.getEndPoint()}")
    print(f"isClosed = {sketchLine.isClosed()}")
    print(f"length = {sketchLine.getLength()}")

def printSketchCenterLine(sketchCenterLine):
    print(f"isConstruction = {sketchCenterLine.isConstruction()}")
    print(f"startPoint = {sketchCenterLine.getStartPoint()}")
    print(f"endPoint = {sketchCenterLine.getEndPoint()}")
    print(f"isClosed = {sketchCenterLine.isClosed()}")
    print(f"length = {sketchCenterLine.getLength()}")

def printSketchCircle(sketchCircle):
    print(f"isConstruction = {sketchCircle.isConstruction()}")
    print(f"center = {sketchCircle.getCenter()}")
    print(f"radius = {sketchCircle.getRadius()}")
    print(f"isClosed = {sketchCircle.isClosed()}")
    print(f"length = {sketchCircle.getLength()}")

def printSketchArc(sketchArc):
    print(f"isConstruction = {sketchArc.isConstruction()}")
    print(f"center = {sketchArc.getCenter()}")
    print(f"radius = {sketchArc.getRadius()}")
    print(f"startAngle = {wy3d.radiansToDegrees(sketchArc.getStartAngle())}")
    print(f"endAngle = {wy3d.radiansToDegrees(sketchArc.getEndAngle())}")
    print(f"isClosed = {sketchArc.isClosed()}")
    print(f"length = {sketchArc.getLength()}")

def printSketchEllipse(sketchEllipse):
    print(f"isConstruction = {sketchEllipse.isConstruction()}")
    print(f"center = {sketchEllipse.getCenter()}")
    print(f"majorAxis = {sketchEllipse.getMajorAxis()}")
    print(f"minorAxis = {sketchEllipse.getMinorAxis()}")
    print(f"majorRadius = {sketchEllipse.getMajorRadius()}")
    print(f"minorRadius = {sketchEllipse.getMinorRadius()}")
    print(f"radiusRatio = {sketchEllipse.getRadiusRatio()}")
    print(f"isClosed = {sketchEllipse.isClosed()}")
    print(f"length = {sketchEllipse.getLength()}")

def printSketchEllipseArc(sketchEllipseArc):
    print(f"isConstruction = {sketchEllipseArc.isConstruction()}")
    print(f"center = {sketchEllipseArc.getCenter()}")
    print(f"majorAxis = {sketchEllipseArc.getMajorAxis()}")
    print(f"minorAxis = {sketchEllipseArc.getMinorAxis()}")
    print(f"majorRadius = {sketchEllipseArc.getMajorRadius()}")
    print(f"minorRadius = {sketchEllipseArc.getMinorRadius()}")
    print(f"radiusRatio = {sketchEllipseArc.getRadiusRatio()}")
    print(f"startAngle = {wy3d.radiansToDegrees(sketchEllipseArc.getStartAngle())}")
    print(f"endAngle = {wy3d.radiansToDegrees(sketchEllipseArc.getEndAngle())}")
    print(f"isClosed = {sketchEllipseArc.isClosed()}")
    print(f"length = {sketchEllipseArc.getLength()}")

def printSketchSpline(sketchSpline):
    print(f"isConstruction = {sketchSpline.isConstruction()}")
    print(f"mode = {sketchSpline.getMode()}")
    print(f"degree = {sketchSpline.getDegree()}")
    print(f"points = {sketchSpline.getPoints()}")
    print(f"isClosed = {sketchSpline.isClosed()}")
    print(f"length = {sketchSpline.getLength()}")

def printSketchEntity(entity):
    print(f"---{entity.getClassName()}---")
    if type(entity) is wy3d.SketchPoint:
        printSketchPoint(entity)
    elif type(entity) is wy3d.SketchLine:
        printSketchLine(entity)
    elif type(entity) is wy3d.SketchCenterLine:
        printSketchCenterLine(entity)
    elif type(entity) is wy3d.SketchCircle:
        printSketchCircle(entity)
    elif type(entity) is wy3d.SketchArc:
        printSketchArc(entity)
    elif type(entity) is wy3d.SketchEllipse:
        printSketchEllipse(entity)
    elif type(entity) is wy3d.SketchEllipseArc:
        printSketchEllipseArc(entity)
    elif type(entity) is wy3d.SketchSpline:
        printSketchSpline(entity)

def printSketch(sketch):
    print("---------SKETCH---------")
    print(f"id = {sketch.getId()}")
    print(f"className = {sketch.getClassName()}")
    for entityId in sketch:
        entity = sketch.getDatabase().getElement(entityId)
        printSketchEntity(entity)

db = wy3d.getActiveDatabase()
for elemId in db:
    elem = db.getElement(elemId)
    if not isinstance(elem, wy3d.Sketch):
        continue
    printSketch(elem)
