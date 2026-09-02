import math

import wy3d

# Layer conventions for sketch semantics not representable in DXF:
#   isConstruction curve  -> layer CONSTRUCTION
#   SketchCenterLine      -> layer CENTERLINE (LINE entity, CENTER linetype)
LAYER_CONSTRUCTION = "CONSTRUCTION"
LAYER_CENTERLINE = "CENTERLINE"

TWO_PI = 2.0 * math.pi
TOL = 1e-9

# Max count of detailed skip records kept per import
MAX_SKIP_DETAILS = 20


def layer_name(e):
    layer = getattr(e.dxf, "layer", "") or ""
    return layer.strip().upper()


def make_vec2(p):
    return wy3d.Vector2(float(p[0]), float(p[1]))


def normalize_angle(a):
    a = math.fmod(a, TWO_PI)
    if a < 0.0:
        a += TWO_PI
    return a


def sweep_total(start, end):
    total = math.fmod(end - start, TWO_PI)
    if total < 0.0:
        total += TWO_PI
    return total


def add_curve(trans, sketch, curve, is_construction):
    trans.addNewlyCreatedElement(curve)
    sketch.addEntity(curve)
    if is_construction:
        curve.setConstruction(True)


def add_centerline(trans, sketch, p0, p1):
    centerLine = wy3d.SketchCenterLine.create(trans, make_vec2(p0), make_vec2(p1))
    if centerLine is None:
        return False
    trans.addNewlyCreatedElement(centerLine)
    sketch.addEntity(centerLine)
    return True
