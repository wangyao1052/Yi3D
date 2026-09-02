import math
import wy3d

# Spur gear parameters (mm / degree)
MODULE = 3.0
TOOTH_COUNT = 24
PRESSURE_ANGLE_DEG = 20.0
THICKNESS = 24.0
BORE_DIAMETER = 12.0
ADDENDUM_COEFF = 1.0
CLEARANCE_COEFF = 0.25
BACKLASH = 0.0

# Involute spline sampling
INVOLUTE_POINT_COUNT = 8

# Derived dimensions
PITCH_DIAMETER = MODULE * TOOTH_COUNT
PITCH_RADIUS = PITCH_DIAMETER * 0.5
PRESSURE_ANGLE_RAD = math.radians(PRESSURE_ANGLE_DEG)
BASE_RADIUS = PITCH_RADIUS * math.cos(PRESSURE_ANGLE_RAD)
ADDENDUM = MODULE * ADDENDUM_COEFF
DEDENDUM = MODULE * (1.0 + CLEARANCE_COEFF)
OUTER_RADIUS = PITCH_RADIUS + ADDENDUM
BORE_RADIUS = BORE_DIAMETER * 0.5
ROOT_RADIUS = max(PITCH_RADIUS - DEDENDUM, BORE_RADIUS + 1.2)
TOOTH_PITCH_ANGLE = 2.0 * math.pi / TOOTH_COUNT
TOOTH_THICKNESS_AT_PITCH = math.pi * MODULE * 0.5 - BACKLASH
HALF_TOOTH_ANGLE_AT_PITCH = TOOTH_THICKNESS_AT_PITCH / (2.0 * PITCH_RADIUS)
HALF_TOOTH_ANGLE = min(0.49 * TOOTH_PITCH_ANGLE, HALF_TOOTH_ANGLE_AT_PITCH)
START_RADIUS = max(BASE_RADIUS, ROOT_RADIUS)


def validate_parameters():
    if MODULE <= 0.0:
        raise ValueError("MODULE must be > 0.")
    if TOOTH_COUNT < 6:
        raise ValueError("TOOTH_COUNT must be >= 6.")
    if THICKNESS <= 0.0:
        raise ValueError("THICKNESS must be > 0.")
    if BORE_DIAMETER <= 0.0:
        raise ValueError("BORE_DIAMETER must be > 0.")
    if ROOT_RADIUS >= OUTER_RADIUS:
        raise ValueError("ROOT_RADIUS must be smaller than OUTER_RADIUS.")
    if BORE_DIAMETER >= 2.0 * ROOT_RADIUS:
        raise ValueError("BORE_DIAMETER must be smaller than root diameter.")
    if HALF_TOOTH_ANGLE <= 0.0:
        raise ValueError("Invalid half tooth angle.")


def involute_function(base_radius, radius):
    if radius <= base_radius:
        return 0.0
    t = math.sqrt((radius * radius) / (base_radius * base_radius) - 1.0)
    return t - math.atan(t)


PITCH_INVOLUTE = involute_function(BASE_RADIUS, PITCH_RADIUS)


def flank_angles(radius):
    inv_value = involute_function(BASE_RADIUS, max(radius, BASE_RADIUS))
    delta = inv_value - PITCH_INVOLUTE
    right_angle = -HALF_TOOTH_ANGLE + delta
    left_angle = HALF_TOOTH_ANGLE - delta
    return right_angle, left_angle


def polar_xy(radius, angle):
    return wy3d.Vector2(radius * math.cos(angle), radius * math.sin(angle))


def rotate_xy(point, angle):
    c = math.cos(angle)
    s = math.sin(angle)
    x = point.x()
    y = point.y()
    return wy3d.Vector2(x * c - y * s, x * s + y * c)


def create_line(trans, sketch, p1, p2):
    line = wy3d.SketchLine.create(trans, p1, p2)
    trans.addNewlyCreatedElement(line)
    sketch.addEntity(line)


def create_arc(trans, sketch, center, radius, start_angle, end_angle):
    if end_angle <= start_angle:
        end_angle += 2.0 * math.pi
    arc = wy3d.SketchArc.create(trans, center, radius, start_angle, end_angle)
    trans.addNewlyCreatedElement(arc)
    sketch.addEntity(arc)


def create_spline_by_fit_points(trans, sketch, points):
    spline = wy3d.SketchSpline.createByFitPoints(trans, points)
    trans.addNewlyCreatedElement(spline)
    sketch.addEntity(spline)


def create_circle_sketch(trans, plane, radius):
    sketch = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch)

    circle = wy3d.SketchCircle.create(trans, wy3d.Vector2(0.0, 0.0), radius)
    trans.addNewlyCreatedElement(circle)
    sketch.addEntity(circle)
    return sketch


def build_involute_points(is_left):
    points = []
    segment_count = max(2, INVOLUTE_POINT_COUNT)
    for i in range(segment_count):
        t = i / float(segment_count - 1)
        radius = START_RADIUS + (OUTER_RADIUS - START_RADIUS) * t
        right_angle, left_angle = flank_angles(radius)
        angle = left_angle if is_left else right_angle
        points.append(polar_xy(radius, angle))
    return points


def build_single_tooth_angles(center_angle):
    right_root_angle, left_root_angle = flank_angles(START_RADIUS)
    right_tip_angle, left_tip_angle = flank_angles(OUTER_RADIUS)
    return {
        "right_root": right_root_angle + center_angle,
        "left_root": left_root_angle + center_angle,
        "right_tip": right_tip_angle + center_angle,
        "left_tip": left_tip_angle + center_angle,
    }


def create_gear_outline_sketch(trans, plane, right_flank_local, left_flank_local):
    sketch = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch)

    tooth_angles = [build_single_tooth_angles(i * TOOTH_PITCH_ANGLE) for i in range(TOOTH_COUNT)]

    for i in range(TOOTH_COUNT):
        a = tooth_angles[i]
        n = tooth_angles[(i + 1) % TOOTH_COUNT]

        right_root = polar_xy(ROOT_RADIUS, a["right_root"])
        left_root = polar_xy(ROOT_RADIUS, a["left_root"])

        right_spline = [rotate_xy(p, i * TOOTH_PITCH_ANGLE) for p in right_flank_local]
        left_spline_desc = [rotate_xy(p, i * TOOTH_PITCH_ANGLE) for p in reversed(left_flank_local)]

        right_start = right_spline[0]
        left_start = left_spline_desc[-1]

        if ROOT_RADIUS < START_RADIUS:
            create_line(trans, sketch, right_root, right_start)

        create_spline_by_fit_points(trans, sketch, right_spline)

        create_arc(
            trans,
            sketch,
            wy3d.Vector2(0.0, 0.0),
            OUTER_RADIUS,
            a["right_tip"],
            a["left_tip"],
        )

        create_spline_by_fit_points(trans, sketch, left_spline_desc)

        if ROOT_RADIUS < START_RADIUS:
            create_line(trans, sketch, left_start, left_root)

        next_right_root_angle = n["right_root"]
        if i == TOOTH_COUNT - 1:
            next_right_root_angle += 2.0 * math.pi
        create_arc(
            trans,
            sketch,
            wy3d.Vector2(0.0, 0.0),
            ROOT_RADIUS,
            a["left_root"],
            next_right_root_angle,
        )

    return sketch


def create_spur_gear():
    validate_parameters()

    right_root_angle, left_root_angle = flank_angles(START_RADIUS)
    right_tip_angle, left_tip_angle = flank_angles(OUTER_RADIUS)
    root_angle_width = left_root_angle - right_root_angle
    tip_angle_width = left_tip_angle - right_tip_angle
    if not (tip_angle_width < root_angle_width):
        raise ValueError("Tooth semantic check failed: tip width must be smaller than root width.")

    db = wy3d.getActiveDatabase()
    trans = db.getTransactionManager().startTransaction()

    base_plane = wy3d.SketchPlane(
        origin=wy3d.Vector3(0.0, 0.0, 0.0),
        normal=wy3d.Vector3(0.0, 0.0, 1.0),
        xDir=wy3d.Vector3(1.0, 0.0, 0.0),
    )

    right_flank = build_involute_points(False)
    left_flank = build_involute_points(True)

    # No union: build one closed outer profile and extrude once.
    gear_outline = create_gear_outline_sketch(trans, base_plane, right_flank, left_flank)
    gear_body = wy3d.Extrusion.create(trans, gear_outline, THICKNESS)
    trans.addNewlyCreatedElement(gear_body)

    # Center bore by difference with independent cutter.
    bore_sketch = create_circle_sketch(trans, base_plane, BORE_RADIUS)
    bore_cutter = wy3d.Extrusion.create(trans, bore_sketch, THICKNESS + 1.0)
    trans.addNewlyCreatedElement(bore_cutter)

    final_gear = wy3d.Difference.create(trans, gear_body, [bore_cutter])
    trans.addNewlyCreatedElement(final_gear)

    db.getTransactionManager().endTransaction()


create_spur_gear()