import math

import ezdxf

import wy3d

from dxf_common import LAYER_CENTERLINE, LAYER_CONSTRUCTION, TOL, TWO_PI


def _ensure_layer(doc, name, **attribs):
    if name not in doc.layers:
        doc.layers.add(name, **attribs)


def _ensure_center_linetype(doc):
    if "CENTER" in doc.linetypes:
        return
    try:
        from ezdxf.tools.standards import setup_linetypes
        setup_linetypes(doc)
    except ImportError:
        pass


def _p2(p):
    return (p.x(), p.y(), 0.0)


def _layer_for(is_construction):
    return LAYER_CONSTRUCTION if is_construction else "0"


def _export_entity(msp, entity):
    if type(entity) is wy3d.SketchCenterLine:
        msp.add_line(_p2(entity.getStartPoint()), _p2(entity.getEndPoint()),
                     dxfattribs={"layer": LAYER_CENTERLINE})
    elif type(entity) is wy3d.SketchLine:
        msp.add_line(_p2(entity.getStartPoint()), _p2(entity.getEndPoint()),
                     dxfattribs={"layer": _layer_for(entity.isConstruction())})
    elif type(entity) is wy3d.SketchCircle:
        center = entity.getCenter()
        msp.add_circle((center.x(), center.y(), 0.0), entity.getRadius(),
                       dxfattribs={"layer": _layer_for(entity.isConstruction())})
    elif type(entity) is wy3d.SketchArc:
        total = entity.getTotalAngle()
        if total <= TOL:
            return
        center = entity.getCenter()
        a0 = math.degrees(entity.getStartAngle()) % 360.0
        # Do not wrap a1: DXF allows end < start (or > 360) for CCW arcs crossing 0
        a1 = a0 + math.degrees(total)
        msp.add_arc((center.x(), center.y(), 0.0), entity.getRadius(), a0, a1,
                    dxfattribs={"layer": _layer_for(entity.isConstruction())})
    elif type(entity) is wy3d.SketchEllipse:
        center = entity.getCenter()
        major = entity.getMajorAxis()
        msp.add_ellipse((center.x(), center.y(), 0.0),
                        (major.x(), major.y(), 0.0), entity.getRadiusRatio(),
                        dxfattribs={"layer": _layer_for(entity.isConstruction())})
    elif type(entity) is wy3d.SketchEllipseArc:
        center = entity.getCenter()
        major = entity.getMajorAxis()
        total = entity.getTotalAngle()
        start = entity.getStartAngle()
        end = start + total
        if total >= TWO_PI - TOL:
            start = 0.0
            end = TWO_PI
        msp.add_ellipse((center.x(), center.y(), 0.0),
                        (major.x(), major.y(), 0.0), entity.getRadiusRatio(),
                        start, end,
                        dxfattribs={"layer": _layer_for(entity.isConstruction())})
    elif type(entity) is wy3d.SketchSpline:
        mode = entity.getMode()
        if mode == wy3d.SplineMode.InterpolationPoints:
            msp.add_spline(fit_points=[_p2(p) for p in entity.getPoints()], degree=3,
                           dxfattribs={"layer": _layer_for(entity.isConstruction())})
        elif mode == wy3d.SplineMode.ControlPoints:
            # ezdxf 1.0.3 has no control_points kwarg; build the spline via
            # set_open_uniform which also generates the clamped uniform knots
            spline = msp.add_spline(dxfattribs={"layer": _layer_for(entity.isConstruction())})
            spline.set_open_uniform([_p2(p) for p in entity.getPoints()],
                                    degree=entity.getDegree())
    elif type(entity) is wy3d.SketchPoint:
        pos = entity.getPosition()
        msp.add_point((pos.x(), pos.y(), 0.0))
    else:
        raise TypeError("unsupported sketch entity: " + type(entity).__name__)


def export_dxf(dxf_path, sketch):
    """Write all entities of the sketch to a DXF R2010 file (world XY plane)."""
    doc = ezdxf.new("R2010", setup=True)
    _ensure_center_linetype(doc)
    _ensure_layer(doc, LAYER_CONSTRUCTION, color=8)
    _ensure_layer(doc, LAYER_CENTERLINE, color=9, linetype="CENTER")
    msp = doc.modelspace()

    db = sketch.getDatabase()
    for entity_id in sketch:
        entity = db.getElement(entity_id)
        if entity is None or entity.isErased():
            continue
        _export_entity(msp, entity)

    doc.saveas(dxf_path)
