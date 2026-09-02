import math
import wy3d


SPRING_RADIUS = 20.0
SPRING_PITCH = 8.0
SPRING_TURNS = 8.0
SPRING_START_ANGLE = 0.0
WIRE_RADIUS = 2.0
HELIX_CLOCKWISE = False
HELIX_REVERSED = False


def createSpring():
    db = wy3d.getActiveDatabase()
    trans = db.getTransactionManager().startTransaction()

    path_plane_origin = wy3d.Vector3(0.0, 0.0, 0.0)
    path_plane_normal = wy3d.Vector3(0.0, 0.0, 1.0)
    path_plane_xdir = wy3d.Vector3(1.0, 0.0, 0.0)

    pathPlane = wy3d.SketchPlane(
        origin=path_plane_origin,
        normal=path_plane_normal,
        xDir=path_plane_xdir,
    )
    pathSketch = wy3d.Sketch.create(trans, pathPlane)
    trans.addNewlyCreatedElement(pathSketch)

    pathCircle = wy3d.SketchCircle.create(trans, wy3d.Vector2(0.0, 0.0), SPRING_RADIUS)
    trans.addNewlyCreatedElement(pathCircle)
    pathSketch.addEntity(pathCircle)

    helix = wy3d.Helix.create(
        transaction=trans,
        sketch=pathSketch,
        pitch=SPRING_PITCH,
        turns=SPRING_TURNS,
        startAngle=SPRING_START_ANGLE,
    )
    trans.addNewlyCreatedElement(helix)
    helix.setClockWise(HELIX_CLOCKWISE)
    helix.setReversed(HELIX_REVERSED)

    # Profile sketch plane: pass through helix start point, normal follows start tangent.
    path_plane_ydir = path_plane_normal.cross(path_plane_xdir)
    c = math.cos(SPRING_START_ANGLE)
    s = math.sin(SPRING_START_ANGLE)

    startRadial = c * path_plane_xdir + s * path_plane_ydir
    startPoint = path_plane_origin + SPRING_RADIUS * startRadial

    tangentAround = (-s) * path_plane_xdir + c * path_plane_ydir
    if HELIX_CLOCKWISE:
        tangentAround = -tangentAround

    axial_sign = -1.0 if HELIX_REVERSED else 1.0
    axial_ratio = 0.0
    if abs(SPRING_RADIUS) > 1e-9:
        axial_ratio = SPRING_PITCH / (2.0 * wy3d.PI * SPRING_RADIUS)
    startTangent = tangentAround + axial_sign * axial_ratio * path_plane_normal
    startTangent = startTangent.normalized()

    profilePlane = wy3d.SketchPlane(
        origin=startPoint,
        normal=startTangent,
        xDir=startRadial,
    )
    profileSketch = wy3d.Sketch.create(trans, profilePlane)
    trans.addNewlyCreatedElement(profileSketch)

    profileCircle = wy3d.SketchCircle.create(
        trans,
        wy3d.Vector2(0.0, 0.0),
        WIRE_RADIUS,
    )
    trans.addNewlyCreatedElement(profileCircle)
    profileSketch.addEntity(profileCircle)

    spring = wy3d.Sweep.create(trans, helix, profileSketch)
    trans.addNewlyCreatedElement(spring)

    db.getTransactionManager().endTransaction()


createSpring()
