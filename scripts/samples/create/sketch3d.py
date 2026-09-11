import math

import wy3d

# 创建直线段
def createLine3D(trans, sketch3d, startPnt, endPnt):
    line = wy3d.SketchLine3D.create(trans, startPnt, endPnt)
    trans.addNewlyCreatedElement(line)
    sketch3d.addEntity(line)

# 创建圆
def createCircle3D(trans, sketch3d, center, normal, xDir, radius):
    circle = wy3d.SketchCircle3D.create(trans, center, normal, xDir, radius)
    trans.addNewlyCreatedElement(circle)
    sketch3d.addEntity(circle)

# 创建圆弧(角度为弧度)
def createArc3D(trans, sketch3d, center, normal, xDir, radius, startAngle, endAngle):
    arc = wy3d.SketchArc3D.create(trans, center, normal, xDir, radius, startAngle, endAngle)
    trans.addNewlyCreatedElement(arc)
    sketch3d.addEntity(arc)

# 创建椭圆(xDir为长轴方向,radiusRatio为短半轴/长半轴)
def createEllipse3D(trans, sketch3d, center, normal, xDir, majorRadius, radiusRatio):
    ellipse = wy3d.SketchEllipse3D.create(trans, center, normal, xDir, majorRadius, radiusRatio)
    trans.addNewlyCreatedElement(ellipse)
    sketch3d.addEntity(ellipse)

# 创建椭圆弧(起止角为自长轴方向量起的极角,弧度)
def createEllipseArc3D(trans, sketch3d, center, normal, xDir, majorRadius, radiusRatio, startAngle, endAngle):
    ellipseArc = wy3d.SketchEllipseArc3D.create(trans, center, normal, xDir, majorRadius, radiusRatio, startAngle, endAngle)
    trans.addNewlyCreatedElement(ellipseArc)
    sketch3d.addEntity(ellipseArc)

# 获取当前文档数据库
db = wy3d.getActiveDatabase()

# 开启事务
trans = db.getTransactionManager().startTransaction()

# 创建3D草图
sketch3d = wy3d.Sketch3D.create(trans)
trans.addNewlyCreatedElement(sketch3d)

# 创建直线段
createLine3D(trans, sketch3d, wy3d.Vector3(0.0, 0.0, 0.0), wy3d.Vector3(100.0, 50.0, 30.0))

# 创建圆
createCircle3D(trans, sketch3d, wy3d.Vector3(50.0, 0.0, 0.0), wy3d.Vector3(0.0, 1.0, 0.0), wy3d.Vector3(1.0, 0.0, 0.0), 25.0)

# 创建圆弧(半圆)
createArc3D(trans, sketch3d, wy3d.Vector3(0.0, 50.0, 0.0), wy3d.Vector3(0.0, 1.0, 0.0), wy3d.Vector3(1.0, 0.0, 0.0), 25.0, 0.0, math.pi)

# 创建椭圆
createEllipse3D(trans, sketch3d, wy3d.Vector3(0.0, -50.0, 0.0), wy3d.Vector3(0.0, 0.0, 1.0), wy3d.Vector3(1.0, 0.0, 0.0), 30.0, 0.5)

# 创建椭圆弧(自长轴方向 0 到 PI/2)
createEllipseArc3D(trans, sketch3d, wy3d.Vector3(100.0, -50.0, 0.0), wy3d.Vector3(0.0, 0.0, 1.0), wy3d.Vector3(1.0, 0.0, 0.0), 20.0, 0.5, 0.0, math.pi * 0.5)

# 提交事务
db.getTransactionManager().endTransaction()
