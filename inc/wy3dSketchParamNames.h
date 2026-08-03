///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SKETCH_PARAM_NAMES_H
#define WY3D_SKETCH_PARAM_NAMES_H

#include <wy3dDefs.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchParamNames
{
public:
    // Sketch Point
    static const char* SKETCH_POINT_PARAM_POSITION_X;
    static const char* SKETCH_POINT_PARAM_POSITION_Y;

    // Sketch Entity
    static const char* SKETCH_ENTITY_ID;

    // Sketch Curve
    static const char* SKETCH_CURVE_IS_CONSTRUCTION;

    // Sketch Line
    static const char* SKETCH_LINE_PARAM_START_X;
    static const char* SKETCH_LINE_PARAM_START_Y;
    static const char* SKETCH_LINE_PARAM_END_X;
    static const char* SKETCH_LINE_PARAM_END_Y;
    static const char* SKETCH_LINE_PARAM_LENGTH;
    static const char* SKETCH_LINE_PARAM_ANGLE;

    // Sketch Circle
    static const char* SKETCH_CIRCLE_PARAM_CENTER_X;
    static const char* SKETCH_CIRCLE_PARAM_CENTER_Y;
    static const char* SKETCH_CIRCLE_PARAM_RADIUS;
    static const char* SKETCH_CIRCLE_PARAM_DIAMETER;
    static const char* SKETCH_CIRCLE_PARAM_PERIMETER;
    static const char* SKETCH_CIRCLE_PARAM_AREA;

    // Sketch Arc
    static const char* SKETCH_ARC_PARAM_CENTER_X;
    static const char* SKETCH_ARC_PARAM_CENTER_Y;
    static const char* SKETCH_ARC_PARAM_RADIUS;
    static const char* SKETCH_ARC_PARAM_START_ANGLE;
    static const char* SKETCH_ARC_PARAM_END_ANGLE;
    static const char* SKETCH_ARC_PARAM_TOTAL_ANGLE;
    static const char* SKETCH_ARC_PARAM_LENGTH;

    // Sketch Ellipse
    static const char* SKETCH_ELLIPSE_PARAM_CENTER_X;
    static const char* SKETCH_ELLIPSE_PARAM_CENTER_Y;
    static const char* SKETCH_ELLIPSE_PARAM_MAJOR_RADIUS;
    static const char* SKETCH_ELLIPSE_PARAM_MINOR_RADIUS;
    static const char* SKETCH_ELLIPSE_PARAM_RADIUS_RATIO;
    static const char* SKETCH_ELLIPSE_PARAM_MAJOR_AXIS_ANGLE;
    static const char* SKETCH_ELLIPSE_PARAM_PERIMETER;
    static const char* SKETCH_ELLIPSE_PARAM_AREA;

    // Sketch Ellipse Arc
    static const char* SKETCH_ELLIPSE_ARC_PARAM_CENTER_X;
    static const char* SKETCH_ELLIPSE_ARC_PARAM_CENTER_Y;
    static const char* SKETCH_ELLIPSE_ARC_PARAM_MAJOR_RADIUS;
    static const char* SKETCH_ELLIPSE_ARC_PARAM_MINOR_RADIUS;
    static const char* SKETCH_ELLIPSE_ARC_PARAM_RADIUS_RATIO;
    static const char* SKETCH_ELLIPSE_ARC_PARAM_MAJOR_AXIS_ANGLE;
    static const char* SKETCH_ELLIPSE_ARC_PARAM_START_ANGLE;
    static const char* SKETCH_ELLIPSE_ARC_PARAM_END_ANGLE;
    static const char* SKETCH_ELLIPSE_ARC_PARAM_PERIMETER;

    // Sketch Center Line
    static const char* SKETCH_CENTER_LINE_PARAM_START_X;
    static const char* SKETCH_CENTER_LINE_PARAM_START_Y;
    static const char* SKETCH_CENTER_LINE_PARAM_END_X;
    static const char* SKETCH_CENTER_LINE_PARAM_END_Y;

    // Sketch Spline
    static const char* SKETCH_SPLINE_ORDER;
};

NS_WY3D_END

#endif // WY3D_SKETCH_PARAM_NAMES_H