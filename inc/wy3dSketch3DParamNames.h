///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH3D_PARAM_NAMES_H
#define WY3D_SKETCH3D_PARAM_NAMES_H

#include <wy3dDefs.h>

NS_WY3D_BEG

class WY3D_EXPORT Sketch3DParamNames
{
public:
    // Sketch Curve 3D
    static const char* SKETCH_CURVE3D_ID;

    // Sketch Line 3D
    static const char* SKETCH_LINE3D_PARAM_START_X;
    static const char* SKETCH_LINE3D_PARAM_START_Y;
    static const char* SKETCH_LINE3D_PARAM_START_Z;
    static const char* SKETCH_LINE3D_PARAM_END_X;
    static const char* SKETCH_LINE3D_PARAM_END_Y;
    static const char* SKETCH_LINE3D_PARAM_END_Z;
    static const char* SKETCH_LINE3D_PARAM_LENGTH;

    // Sketch Circle 3D
    static const char* SKETCH_CIRCLE3D_PARAM_CENTER_X;
    static const char* SKETCH_CIRCLE3D_PARAM_CENTER_Y;
    static const char* SKETCH_CIRCLE3D_PARAM_CENTER_Z;
    static const char* SKETCH_CIRCLE3D_PARAM_NORMAL_X;
    static const char* SKETCH_CIRCLE3D_PARAM_NORMAL_Y;
    static const char* SKETCH_CIRCLE3D_PARAM_NORMAL_Z;
    static const char* SKETCH_CIRCLE3D_PARAM_XDIR_X;
    static const char* SKETCH_CIRCLE3D_PARAM_XDIR_Y;
    static const char* SKETCH_CIRCLE3D_PARAM_XDIR_Z;
    static const char* SKETCH_CIRCLE3D_PARAM_RADIUS;
    static const char* SKETCH_CIRCLE3D_PARAM_DIAMETER;
    static const char* SKETCH_CIRCLE3D_PARAM_PERIMETER;
    static const char* SKETCH_CIRCLE3D_PARAM_AREA;

    // Sketch Arc 3D
    static const char* SKETCH_ARC3D_PARAM_CENTER_X;
    static const char* SKETCH_ARC3D_PARAM_CENTER_Y;
    static const char* SKETCH_ARC3D_PARAM_CENTER_Z;
    static const char* SKETCH_ARC3D_PARAM_NORMAL_X;
    static const char* SKETCH_ARC3D_PARAM_NORMAL_Y;
    static const char* SKETCH_ARC3D_PARAM_NORMAL_Z;
    static const char* SKETCH_ARC3D_PARAM_XDIR_X;
    static const char* SKETCH_ARC3D_PARAM_XDIR_Y;
    static const char* SKETCH_ARC3D_PARAM_XDIR_Z;
    static const char* SKETCH_ARC3D_PARAM_RADIUS;
    static const char* SKETCH_ARC3D_PARAM_START_ANGLE;
    static const char* SKETCH_ARC3D_PARAM_END_ANGLE;
    static const char* SKETCH_ARC3D_PARAM_TOTAL_ANGLE;
    static const char* SKETCH_ARC3D_PARAM_LENGTH;
};

NS_WY3D_END

#endif // WY3D_SKETCH3D_PARAM_NAMES_H
