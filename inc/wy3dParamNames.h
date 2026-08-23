///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_PARAM_NAMES_H
#define WY3D_PARAM_NAMES_H

#include <wy3dDefs.h>

NS_WY3D_BEG

class WY3D_EXPORT ParamNames
{
public:
    // Box
    static const char* BOX_PARAM_LENGTH;
    static const char* BOX_PARAM_WIDTH;
    static const char* BOX_PARAM_HEIGHT;

    // Cylinder
    static const char* CYLINDER_PARAM_RADIUS;
    static const char* CYLINDER_PARAM_HEIGHT;

    // Sphere
    static const char* SPHERE_PARAM_RADIUS;

    // Cone
    static const char* CONE_PARAM_RADIUS;
    static const char* CONE_PARAM_HEIGHT;

    // Torus
    static const char* TORUS_PARAM_MAJOR_RADIUS;
    static const char* TORUS_PARAM_MINOR_RADIUS;

    // Tube
    static const char* TUBE_PARAM_OUTER_RADIUS;
    static const char* TUBE_PARAM_INNER_RADIUS;
    static const char* TUBE_PARAM_HEIGHT;

    // Extrusion
    static const char* EXTRUSION_PARAM_DEPTH;
    static const char* EXTRUSION_PARAM_START_OFFSET;
    static const char* EXTRUSION_PARAM_DIRECTION;

    // Revolution
    static const char* REVOLUTION_PARAM_START_ANGLE;
    static const char* REVOLUTION_PARAM_END_ANGLE;
    static const char* REVOLUTION_PARAM_AXIS;

    // Imported Solid
    static const char* IMPORTED_SOLID_PARAM_FILE_PATH;

    // Chamfer
    static const char* CHAMFER_DISTANCE1;
    static const char* CHAMFER_DISTANCE2;
    static const char* CHAMFER_ANGLE;
    static const char* CHAMFER_TYPE;
    static const char* CHAMFER_IS_FLIPPED;

    // Fillet
    static const char* FILLET_RADIUS;

    // Shell
    static const char* SHELL_THICKNESS;
    static const char* SHELL_DIRECTION;
    static const char* SHELL_JOIN_TYPE;
    static const char* SHELL_OFFSET_MODE;
    static const char* SHELL_INTERSECTION;

    // Draft
    static const char* DRAFT_ANGLE;

    // Move
    static const char* MOVE_VECTOR_X;
    static const char* MOVE_VECTOR_Y;
    static const char* MOVE_VECTOR_Z;

    // Rotate
    static const char* ROTATE_CENTER_X;
    static const char* ROTATE_CENTER_Y;
    static const char* ROTATE_CENTER_Z;
    static const char* ROTATE_AXIS_DIRECTION_X;
    static const char* ROTATE_AXIS_DIRECTION_Y;
    static const char* ROTATE_AXIS_DIRECTION_Z;
    static const char* ROTATE_ANGLE;

    // Datum Plane
    static const char* DATUM_PLANE_PARAM_PLANE;

    // Mirror
    static const char* MIRROR_PARAM_PLANE;

    // Solid
    static const char* SOLID_PARAM_COLOR;
    static const char* MIRROR_SOURCE;

    // Pattern
    static const char* PATTERN_SOURCE;

    // Linear Pattern
    static const char* LINEAR_PATTERN_DIRECTION_1ST_X;
    static const char* LINEAR_PATTERN_DIRECTION_1ST_Y;
    static const char* LINEAR_PATTERN_DIRECTION_1ST_Z;
    static const char* LINEAR_PATTERN_COUNT_1ST;
    static const char* LINEAR_PATTERN_SPACING_1ST;
    static const char* LINEAR_PATTERN_DIRECTION_2ND_X;
    static const char* LINEAR_PATTERN_DIRECTION_2ND_Y;
    static const char* LINEAR_PATTERN_DIRECTION_2ND_Z;
    static const char* LINEAR_PATTERN_COUNT_2ND;
    static const char* LINEAR_PATTERN_SPACING_2ND;

    // Circular Pattern
    static const char* CIRCULAR_PATTERN_CENTER_POINT_X;
    static const char* CIRCULAR_PATTERN_CENTER_POINT_Y;
    static const char* CIRCULAR_PATTERN_CENTER_POINT_Z;
    static const char* CIRCULAR_PATTERN_AXIS_DIRECTION_X;
    static const char* CIRCULAR_PATTERN_AXIS_DIRECTION_Y;
    static const char* CIRCULAR_PATTERN_AXIS_DIRECTION_Z;
    static const char* CIRCULAR_PATTERN_TOTAL_ANGLE;
    static const char* CIRCULAR_PATTERN_INSTANCE_COUNT;
    static const char* CIRCULAR_PATTERN_IS_CLOCKWISE;

    // Thicken
    static const char* THICKEN_PARAM_THICKNESS;
    static const char* THICKEN_PARAM_SOURCE;
    static const char* THICKEN_PARAM_DIRECTION;

    // OffsetSheet
    static const char* OFFSETSHEET_PARAM_OFFSET;
    static const char* OFFSETSHEET_PARAM_SOURCE;

    // SewnSheet
    static const char* SEWNSHEET_PARAM_TOLERANCE;

    // Solidify
    static const char* SOLIDIFY_PARAM_SOURCE;

    // Helix
    static const char* HELIX_PITCH;
    static const char* HELIX_TURNS;
    static const char* HELIX_START_ANGLE;
    static const char* HELIX_IS_CLOCKWISE;
    static const char* HELIX_IS_REVERSED;
};

NS_WY3D_END

#endif // WY3D_PARAM_NAMES_H