import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import wy3d

from dxf_export import export_dxf
from dxf_import import import_dxf

V = wy3d.Vector2

db = wy3d.getActiveDatabase()
trans = db.getTransactionManager().startTransaction()
if trans is None:
    raise SystemExit("ROUNDTRIP_TEST: cannot start transaction")

plane = wy3d.SketchPlane(
    wy3d.Vector3(0.0, 0.0, 0.0),
    wy3d.Vector3(0.0, 0.0, 1.0),
    wy3d.Vector3(1.0, 0.0, 0.0))
sketch = wy3d.Sketch.create(trans, plane)
trans.addNewlyCreatedElement(sketch)


def add(entity):
    trans.addNewlyCreatedElement(entity)
    sketch.addEntity(entity)


PI = wy3d.PI
HALF_PI = PI / 2.0
TWO_PI = 2.0 * PI

# Build one entity of every supported type, in this exact order
add(wy3d.SketchLine.create(trans, V(0.0, 0.0), V(100.0, 50.0)))
add(wy3d.SketchCenterLine.create(trans, V(0.0, 0.0), V(0.0, 100.0)))
line = wy3d.SketchLine.create(trans, V(10.0, 10.0), V(20.0, 20.0))
line.setConstruction(True)
add(line)
add(wy3d.SketchCircle.create(trans, V(5.0, 5.0), 7.0))
add(wy3d.SketchArc.create(trans, V(0.0, 0.0), 10.0, 0.5, 2.0))
add(wy3d.SketchEllipse.create(trans, V(0.0, 0.0), V(20.0, 0.0), 0.5))
add(wy3d.SketchEllipseArc.create(trans, V(1.0, 1.0), V(15.0, 0.0), 0.25, 0.0, PI))
add(wy3d.SketchSpline.createByFitPoints(trans, [V(0.0, 0.0), V(10.0, 5.0), V(20.0, 0.0)]))
add(wy3d.SketchSpline.createByControlPoints(trans, 3, [V(0.0, 10.0), V(5.0, 15.0), V(10.0, 10.0), V(15.0, 15.0)]))
add(wy3d.SketchPoint.create(trans, V(3.0, 3.0)))

# Expected descriptors in creation order: (class, checker)
EXPECTED = [
    ("SketchLine", lambda e: (e.getStartPoint().x(), e.getStartPoint().y(), e.getEndPoint().x(), e.getEndPoint().y()) == (0.0, 0.0, 100.0, 50.0) and not e.isConstruction()),
    ("SketchCenterLine", lambda e: (e.getStartPoint().x(), e.getEndPoint().y()) == (0.0, 100.0)),
    ("SketchLine", lambda e: e.isConstruction() and (e.getStartPoint().x(), e.getEndPoint().x()) == (10.0, 20.0)),
    ("SketchCircle", lambda e: abs(e.getCenter().x() - 5.0) < 1e-12 and abs(e.getRadius() - 7.0) < 1e-12),
    ("SketchArc", lambda e: abs(e.getRadius() - 10.0) < 1e-12 and abs(e.getStartAngle() - 0.5) < 1e-12 and abs(e.getTotalAngle() - 1.5) < 1e-12),
    ("SketchEllipse", lambda e: abs(e.getMajorAxis().x() - 20.0) < 1e-12 and abs(e.getRadiusRatio() - 0.5) < 1e-12),
    ("SketchEllipseArc", lambda e: abs(e.getMajorAxis().x() - 15.0) < 1e-12 and abs(e.getRadiusRatio() - 0.25) < 1e-12 and abs(e.getStartAngle()) < 1e-12 and abs(e.getTotalAngle() - PI) < 1e-12),
    ("SketchSpline", lambda e: e.getMode() == wy3d.SplineMode.InterpolationPoints and len(e.getPoints()) == 3),
    ("SketchSpline", lambda e: e.getMode() == wy3d.SplineMode.ControlPoints and e.getDegree() == 3 and len(e.getPoints()) == 4),
    ("SketchPoint", lambda e: (e.getPosition().x(), e.getPosition().y()) == (3.0, 3.0)),
]

tmp_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "_roundtrip.dxf")
try:
    export_dxf(tmp_path, sketch)

    # Import into a second sketch, same transaction (mirrors the C++ flow)
    sketch2 = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch2)
    stats = import_dxf(tmp_path, trans, sketch2)
finally:
    if os.path.exists(tmp_path):
        os.remove(tmp_path)

db.getTransactionManager().endTransaction()

# Compare entity-by-entity in order (export order == import order)
got = [sketch2.getDatabase().getElement(eid) for eid in sketch2]
failures = []
if len(got) != len(EXPECTED):
    failures.append("entity count %d != %d" % (len(got), len(EXPECTED)))
for i, (expect_name, check) in enumerate(EXPECTED):
    if i >= len(got):
        break
    e = got[i]
    name = e.getClassName().replace("wy3d::", "")
    if name != expect_name:
        failures.append("entity[%d] type %s != %s" % (i, name, expect_name))
        continue
    if not check(e):
        failures.append("entity[%d] %s value mismatch" % (i, name))

print("ROUNDTRIP_TEST: skipped = %d" % len(stats.skipped))
if failures:
    for f in failures:
        print("ROUNDTRIP_TEST: FAIL - %s" % f)
    raise SystemExit("ROUNDTRIP_TEST: FAILED")
print("ROUNDTRIP_TEST: PASSED (%d entities)" % len(got))
