import wy3d

# Xiaomi 15 body dimensions (mm)
PHONE_BODY_WIDTH = 71.2
PHONE_BODY_LENGTH = 152.3
PHONE_BODY_THICKNESS = 8.08

# Case fit and structure (mm)
FIT_CLEARANCE = 0.35
WALL_THICKNESS = 1.8
BACK_THICKNESS = 1.6
TOP_LIP_HEIGHT = 0.7

# Derived shell dimensions (mm)
INNER_WIDTH = PHONE_BODY_WIDTH + 2.0 * FIT_CLEARANCE
INNER_LENGTH = PHONE_BODY_LENGTH + 2.0 * FIT_CLEARANCE
CASE_WIDTH = INNER_WIDTH + 2.0 * WALL_THICKNESS
CASE_LENGTH = INNER_LENGTH + 2.0 * WALL_THICKNESS
CASE_HEIGHT = PHONE_BODY_THICKNESS + BACK_THICKNESS + TOP_LIP_HEIGHT

INNER_CORNER_RADIUS = 9.6
OUTER_CORNER_RADIUS = INNER_CORNER_RADIUS + WALL_THICKNESS

# Camera cutout (top-left, back side)
CAMERA_CUT_WIDTH = 36.0
CAMERA_CUT_LENGTH = 36.0
CAMERA_LEFT_MARGIN = 5.8
CAMERA_TOP_MARGIN = 6.2
CAMERA_CUT_CENTER_X = -CASE_WIDTH * 0.5 + CAMERA_LEFT_MARGIN + CAMERA_CUT_WIDTH * 0.5
CAMERA_CUT_CENTER_Y = CASE_LENGTH * 0.5 - CAMERA_TOP_MARGIN - CAMERA_CUT_LENGTH * 0.5

# Flash cutout
FLASH_CUT_RADIUS = 2.8
FLASH_CUT_CENTER_X = CAMERA_CUT_CENTER_X + 12.0
FLASH_CUT_CENTER_Y = CAMERA_CUT_CENTER_Y - 12.0

# Bottom functional openings (Type-C / speaker / mic)
BOTTOM_OPENING_CENTER_Z = BACK_THICKNESS + 2.6
BOTTOM_CUT_DEPTH = WALL_THICKNESS + 2.2
BOTTOM_CORNER_KEEP_OUT = 1.0

USB_C_CUT_WIDTH = 12.4
USB_C_CUT_HEIGHT = 5.2

SPEAKER_SLOT_WIDTH = 16.0
SPEAKER_SLOT_HEIGHT = 2.4
SPEAKER_SLOT_CENTER_X = 22.0

MIC_HOLE_RADIUS = 0.9
MIC_HOLE_CENTER_X = -18.0


def create_line(trans, sketch, x1, y1, x2, y2):
    line = wy3d.SketchLine.create(trans, wy3d.Vector2(x1, y1), wy3d.Vector2(x2, y2))
    trans.addNewlyCreatedElement(line)
    sketch.addEntity(line)


def create_arc(trans, sketch, cx, cy, radius, start_angle, end_angle):
    arc = wy3d.SketchArc.create(trans, wy3d.Vector2(cx, cy), radius, start_angle, end_angle)
    trans.addNewlyCreatedElement(arc)
    sketch.addEntity(arc)


def create_circle(trans, sketch, cx, cy, radius):
    circle = wy3d.SketchCircle.create(trans, wy3d.Vector2(cx, cy), radius)
    trans.addNewlyCreatedElement(circle)
    sketch.addEntity(circle)


def clamp(value, min_value, max_value):
    return max(min_value, min(max_value, value))


def clamp_opening_center_x(requested_center_x, opening_half_span, straight_half_span):
    max_center = max(0.0, straight_half_span - opening_half_span)
    return clamp(requested_center_x, -max_center, max_center)


def create_rectangle_sketch(trans, plane, width, length, center_x=0.0, center_y=0.0):
    hx = width * 0.5
    hy = length * 0.5

    sketch = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch)

    create_line(trans, sketch, center_x - hx, center_y - hy, center_x + hx, center_y - hy)
    create_line(trans, sketch, center_x + hx, center_y - hy, center_x + hx, center_y + hy)
    create_line(trans, sketch, center_x + hx, center_y + hy, center_x - hx, center_y + hy)
    create_line(trans, sketch, center_x - hx, center_y + hy, center_x - hx, center_y - hy)

    return sketch


def create_rounded_rectangle_sketch(trans, plane, width, length, radius, center_x=0.0, center_y=0.0):
    hx = width * 0.5
    hy = length * 0.5
    r = max(0.0, min(radius, hx, hy))

    sketch = wy3d.Sketch.create(trans, plane)
    trans.addNewlyCreatedElement(sketch)

    x_right = center_x + hx
    x_left = center_x - hx
    y_top = center_y + hy
    y_bottom = center_y - hy

    ctr = (center_x + hx - r, center_y + hy - r)
    ctl = (center_x - hx + r, center_y + hy - r)
    cbl = (center_x - hx + r, center_y - hy + r)
    cbr = (center_x + hx - r, center_y - hy + r)

    # Arc direction is CCW in yi3d.
    create_arc(trans, sketch, ctr[0], ctr[1], r, 0.0, wy3d.PI_2)
    create_line(trans, sketch, center_x + hx - r, y_top, center_x - hx + r, y_top)

    create_arc(trans, sketch, ctl[0], ctl[1], r, wy3d.PI_2, wy3d.PI)
    create_line(trans, sketch, x_left, center_y + hy - r, x_left, center_y - hy + r)

    create_arc(trans, sketch, cbl[0], cbl[1], r, wy3d.PI, wy3d.PI + wy3d.PI_2)
    create_line(trans, sketch, center_x - hx + r, y_bottom, center_x + hx - r, y_bottom)

    create_arc(trans, sketch, cbr[0], cbr[1], r, wy3d.PI + wy3d.PI_2, 2.0 * wy3d.PI)
    create_line(trans, sketch, x_right, center_y - hy + r, x_right, center_y + hy - r)

    return sketch


def create_xiaomi_phone_case():
    db = wy3d.getActiveDatabase()
    trans = db.getTransactionManager().startTransaction()

    plane_bottom = wy3d.SketchPlane(
        origin=wy3d.Vector3(0.0, 0.0, 0.0),
        normal=wy3d.Vector3(0.0, 0.0, 1.0),
        xDir=wy3d.Vector3(1.0, 0.0, 0.0),
    )

    outer_sketch = create_rounded_rectangle_sketch(
        trans,
        plane_bottom,
        CASE_WIDTH,
        CASE_LENGTH,
        OUTER_CORNER_RADIUS,
    )
    outer_body = wy3d.Extrusion.create(trans, outer_sketch, CASE_HEIGHT)
    trans.addNewlyCreatedElement(outer_body)

    plane_inner = wy3d.SketchPlane(
        origin=wy3d.Vector3(0.0, 0.0, BACK_THICKNESS),
        normal=wy3d.Vector3(0.0, 0.0, 1.0),
        xDir=wy3d.Vector3(1.0, 0.0, 0.0),
    )
    inner_sketch = create_rounded_rectangle_sketch(
        trans,
        plane_inner,
        INNER_WIDTH,
        INNER_LENGTH,
        INNER_CORNER_RADIUS,
    )
    inner_cutter = wy3d.Extrusion.create(trans, inner_sketch, CASE_HEIGHT)
    trans.addNewlyCreatedElement(inner_cutter)

    shell = wy3d.Difference.create(trans, outer_body, [inner_cutter])
    trans.addNewlyCreatedElement(shell)

    camera_sketch = create_rectangle_sketch(
        trans,
        plane_bottom,
        CAMERA_CUT_WIDTH,
        CAMERA_CUT_LENGTH,
        CAMERA_CUT_CENTER_X,
        CAMERA_CUT_CENTER_Y,
    )
    camera_cutter = wy3d.Extrusion.create(trans, camera_sketch, BACK_THICKNESS + 1.0)
    trans.addNewlyCreatedElement(camera_cutter)

    shell_with_camera = wy3d.Difference.create(trans, shell, [camera_cutter])
    trans.addNewlyCreatedElement(shell_with_camera)

    flash_sketch = wy3d.Sketch.create(trans, plane_bottom)
    trans.addNewlyCreatedElement(flash_sketch)
    create_circle(trans, flash_sketch, FLASH_CUT_CENTER_X, FLASH_CUT_CENTER_Y, FLASH_CUT_RADIUS)

    flash_cutter = wy3d.Extrusion.create(trans, flash_sketch, BACK_THICKNESS + 1.0)
    trans.addNewlyCreatedElement(flash_cutter)

    shell_with_back_holes = wy3d.Difference.create(trans, shell_with_camera, [flash_cutter])
    trans.addNewlyCreatedElement(shell_with_back_holes)

    bottom_plane = wy3d.SketchPlane(
        origin=wy3d.Vector3(0.0, -CASE_LENGTH * 0.5 - 0.2, BOTTOM_OPENING_CENTER_Z),
        normal=wy3d.Vector3(0.0, 1.0, 0.0),
        xDir=wy3d.Vector3(1.0, 0.0, 0.0),
    )

    bottom_straight_half_span = CASE_WIDTH * 0.5 - OUTER_CORNER_RADIUS - BOTTOM_CORNER_KEEP_OUT
    speaker_center_x = clamp_opening_center_x(
        SPEAKER_SLOT_CENTER_X,
        SPEAKER_SLOT_WIDTH * 0.5,
        bottom_straight_half_span,
    )
    mic_center_x = clamp_opening_center_x(
        MIC_HOLE_CENTER_X,
        MIC_HOLE_RADIUS,
        bottom_straight_half_span,
    )

    usb_sketch = create_rectangle_sketch(
        trans,
        bottom_plane,
        USB_C_CUT_WIDTH,
        USB_C_CUT_HEIGHT,
        0.0,
        0.0,
    )
    usb_cutter = wy3d.Extrusion.create(trans, usb_sketch, BOTTOM_CUT_DEPTH)
    trans.addNewlyCreatedElement(usb_cutter)

    speaker_sketch = create_rectangle_sketch(
        trans,
        bottom_plane,
        SPEAKER_SLOT_WIDTH,
        SPEAKER_SLOT_HEIGHT,
        speaker_center_x,
        0.0,
    )
    speaker_cutter = wy3d.Extrusion.create(trans, speaker_sketch, BOTTOM_CUT_DEPTH)
    trans.addNewlyCreatedElement(speaker_cutter)

    mic_sketch = wy3d.Sketch.create(trans, bottom_plane)
    trans.addNewlyCreatedElement(mic_sketch)
    create_circle(trans, mic_sketch, mic_center_x, 0.0, MIC_HOLE_RADIUS)
    mic_cutter = wy3d.Extrusion.create(trans, mic_sketch, BOTTOM_CUT_DEPTH)
    trans.addNewlyCreatedElement(mic_cutter)

    final_shell = wy3d.Difference.create(
        trans,
        shell_with_back_holes,
        [usb_cutter, speaker_cutter, mic_cutter],
    )
    trans.addNewlyCreatedElement(final_shell)

    db.getTransactionManager().endTransaction()


create_xiaomi_phone_case()
