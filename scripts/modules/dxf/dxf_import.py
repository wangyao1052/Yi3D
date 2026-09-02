import math

import ezdxf

import wy3d

from dxf_common import (
    LAYER_CENTERLINE,
    LAYER_CONSTRUCTION,
    MAX_SKIP_DETAILS,
    TOL,
    TWO_PI,
    add_centerline,
    add_curve,
    layer_name,
    make_vec2,
    normalize_angle,
)


class ImportStats:
    def __init__(self):
        self.imported = 0
        self.skipped = []

    def skip(self, dxftype, reason):
        if len(self.skipped) < MAX_SKIP_DETAILS:
            self.skipped.append((dxftype, reason))


def _angle_from_xy(x, y):
    a = math.atan2(y, x)
    return normalize_angle(a)


def _import_line(e, trans, sketch, stats, layer):
    if layer == LAYER_CENTERLINE:
        if add_centerline(trans, sketch, e.dxf.start, e.dxf.end):
            stats.imported += 1
        else:
            stats.skip("LINE", "centerline create failed")
    else:
        line = wy3d.SketchLine.create(trans, make_vec2(e.dxf.start), make_vec2(e.dxf.end))
        if line is None:
            stats.skip("LINE", "create failed")
            return
        add_curve(trans, sketch, line, layer == LAYER_CONSTRUCTION)
        stats.imported += 1


def _import_circle(e, trans, sketch, stats, layer):
    center = e.dxf.center
    radius = float(e.dxf.radius)
    if radius <= TOL:
        stats.skip("CIRCLE", "radius too small")
        return
    circle = wy3d.SketchCircle.create(trans, make_vec2(center), radius)
    if circle is None:
        stats.skip("CIRCLE", "create failed")
        return
    add_curve(trans, sketch, circle, layer == LAYER_CONSTRUCTION)
    stats.imported += 1


def _import_arc(e, trans, sketch, stats, layer):
    start = math.radians(float(e.dxf.start_angle))
    end = math.radians(float(e.dxf.end_angle))
    # DXF arcs are CCW; end may be < start (crossing 0 deg) or > start + 2*pi
    sweep = end - start
    if sweep < 0.0:
        sweep += TWO_PI * math.ceil(-sweep / TWO_PI)
    # A full circle is not a valid wy3d arc (total angle must be < 2*pi)
    if sweep >= TWO_PI - TOL:
        _import_circle(e, trans, sketch, stats, layer)
        return
    if sweep <= TOL:
        stats.skip("ARC", "sweep too small")
        return
    center = e.dxf.center
    radius = float(e.dxf.radius)
    if radius <= TOL:
        stats.skip("ARC", "radius too small")
        return
    arc = wy3d.SketchArc.create(trans, make_vec2(center), radius, start, start + sweep)
    if arc is None:
        stats.skip("ARC", "create failed")
        return
    add_curve(trans, sketch, arc, layer == LAYER_CONSTRUCTION)
    stats.imported += 1


def _import_ellipse(e, trans, sketch, stats, layer):
    center = e.dxf.center
    major = (float(e.dxf.major_axis[0]), float(e.dxf.major_axis[1]))
    ratio = float(e.dxf.ratio)
    major_len = math.hypot(major[0], major[1])
    if major_len <= TOL:
        stats.skip("ELLIPSE", "major axis too small")
        return
    if ratio <= TOL:
        stats.skip("ELLIPSE", "ratio too small")
        return
    if ratio > 1.0:
        # Non-conformant input (the DXF spec requires ratio <= 1) and YI3D
        # enforces the same rule, so such entities are skipped
        stats.skip("ELLIPSE", "ratio > 1 not supported")
        return

    start = float(e.dxf.start_param)
    end = float(e.dxf.end_param)
    # end may be < start (crossing 0); wy3d requires end > start with total < 2*pi
    total = math.fmod(end - start, TWO_PI)
    if total < 0.0:
        total += TWO_PI

    major_vec2 = wy3d.Vector2(major[0], major[1])
    if total <= TOL:
        ellipse = wy3d.SketchEllipse.create(trans, make_vec2(center), major_vec2, ratio)
        if ellipse is None:
            stats.skip("ELLIPSE", "create failed")
            return
        add_curve(trans, sketch, ellipse, layer == LAYER_CONSTRUCTION)
        stats.imported += 1
        return
    # start_param/end_param are measured from the major axis, same as wy3d angles
    ellipse_arc = wy3d.SketchEllipseArc.create(
        trans, make_vec2(center), major_vec2, ratio, start, start + total)
    if ellipse_arc is None:
        stats.skip("ELLIPSE", "arc create failed")
        return
    add_curve(trans, sketch, ellipse_arc, layer == LAYER_CONSTRUCTION)
    stats.imported += 1


def _import_spline(e, trans, sketch, stats, layer):
    fit_points = list(e.fit_points)
    if len(fit_points) >= 2:
        spline = wy3d.SketchSpline.createByFitPoints(
            trans, [make_vec2(p) for p in fit_points])
        if spline is None:
            stats.skip("SPLINE", "fit create failed")
            return
        add_curve(trans, sketch, spline, layer == LAYER_CONSTRUCTION)
        stats.imported += 1
        return
    control_points = list(e.control_points)
    degree = int(e.dxf.degree)
    if len(control_points) >= 2 and 1 <= degree <= 5:
        spline = wy3d.SketchSpline.createByControlPoints(
            trans, degree, [make_vec2(p) for p in control_points])
        if spline is None:
            stats.skip("SPLINE", "control create failed")
            return
        add_curve(trans, sketch, spline, layer == LAYER_CONSTRUCTION)
        stats.imported += 1
        return
    stats.skip("SPLINE", "unsupported degree/point count")


def _import_point(e, trans, sketch, stats, layer):
    point = wy3d.SketchPoint.create(trans, make_vec2(e.dxf.location))
    if point is None:
        stats.skip("POINT", "create failed")
        return
    trans.addNewlyCreatedElement(point)
    sketch.addEntity(point)
    stats.imported += 1


def _bulge_to_arc(p0, p1, bulge):
    """Return ('line',) or ('arc', center, radius, start, end) for one polyline segment."""
    theta = 4.0 * math.atan(bulge)  # signed, CCW positive in world XY
    dx = p1[0] - p0[0]
    dy = p1[1] - p0[1]
    chord = math.hypot(dx, dy)
    if chord <= TOL:
        return None
    if abs(theta) <= TOL:
        return ("line",)
    radius = (chord / 2.0) / abs(math.sin(theta / 2.0))
    if radius > 1.0e9:
        return ("line",)
    # chord left normal (CCW 90 deg)
    nx = -dy / chord
    ny = dx / chord
    mx = (p0[0] + p1[0]) / 2.0
    my = (p0[1] + p1[1]) / 2.0
    cot = 1.0 / math.tan(theta / 2.0)
    cx = mx + nx * (chord / 2.0) * cot
    cy = my + ny * (chord / 2.0) * cot
    a0 = _angle_from_xy(p0[0] - cx, p0[1] - cy)
    a1 = a0 + theta
    if theta > 0.0:
        return ("arc", (cx, cy), radius, a0, a1)
    return ("arc", (cx, cy), radius, a1, a0)


def _import_lwpolyline(e, trans, sketch, stats, layer):
    points = e.get_points("xyb")  # [(x, y, bulge), ...]
    if len(points) < 2:
        stats.skip("LWPOLYLINE", "too few vertices")
        return
    segments = list(zip(points, points[1:]))
    if e.closed:
        segments.append((points[-1], points[0]))

    is_construction = layer == LAYER_CONSTRUCTION
    is_centerline = layer == LAYER_CENTERLINE

    for p0, p1 in segments:
        arc = _bulge_to_arc(p0[:2], p1[:2], p0[2])
        if arc is None:
            continue
        if arc[0] == "line":
            if is_centerline:
                add_centerline(trans, sketch, p0[:2], p1[:2])
            else:
                line = wy3d.SketchLine.create(trans, make_vec2(p0[:2]), make_vec2(p1[:2]))
                if line is None:
                    stats.skip("LWPOLYLINE", "line create failed")
                    continue
                add_curve(trans, sketch, line, is_construction)
            stats.imported += 1
        else:
            _, center, radius, start, end = arc
            circle = wy3d.SketchArc.create(trans, wy3d.Vector2(center[0], center[1]),
                                           radius, start, end)
            if circle is None:
                stats.skip("LWPOLYLINE", "arc create failed")
                continue
            add_curve(trans, sketch, circle, is_construction)
            stats.imported += 1


def import_dxf(dxf_path, trans, sketch):
    """Import all supported entities of the DXF file into the sketch.

    The caller owns the transaction: entities are created inside it but nothing
    is committed or aborted here.
    """
    stats = ImportStats()
    doc = ezdxf.readfile(dxf_path)
    for e in doc.modelspace():
        dxftype = e.dxftype()
        layer = layer_name(e)
        if dxftype == "LINE":
            _import_line(e, trans, sketch, stats, layer)
        elif dxftype == "CIRCLE":
            _import_circle(e, trans, sketch, stats, layer)
        elif dxftype == "ARC":
            _import_arc(e, trans, sketch, stats, layer)
        elif dxftype == "ELLIPSE":
            _import_ellipse(e, trans, sketch, stats, layer)
        elif dxftype == "SPLINE":
            _import_spline(e, trans, sketch, stats, layer)
        elif dxftype == "LWPOLYLINE":
            _import_lwpolyline(e, trans, sketch, stats, layer)
        elif dxftype == "POLYLINE":
            _import_polyline(e, trans, sketch, stats, layer)
        elif dxftype == "POINT":
            _import_point(e, trans, sketch, stats, layer)
        else:
            stats.skip(dxftype, "unsupported entity type")
    return stats


def _import_polyline(e, trans, sketch, stats, layer):
    # POLYLINE entities are converted to line segments (arc segments are rare)
    points = list(e.points())
    if len(points) < 2:
        stats.skip("POLYLINE", "too few vertices")
        return
    segments = list(zip(points, points[1:]))
    if e.is_closed:
        segments.append((points[-1], points[0]))
    is_construction = layer == LAYER_CONSTRUCTION
    is_centerline = layer == LAYER_CENTERLINE
    for p0, p1 in segments:
        if is_centerline:
            add_centerline(trans, sketch, p0, p1)
        else:
            line = wy3d.SketchLine.create(trans, make_vec2(p0), make_vec2(p1))
            if line is None:
                stats.skip("POLYLINE", "line create failed")
                continue
            add_curve(trans, sketch, line, is_construction)
        stats.imported += 1
