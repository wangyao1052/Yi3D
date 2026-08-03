///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include <wy3dParamNames.h>

NS_WY3D_BEG

// Box
const char* ParamNames::BOX_PARAM_LENGTH = "Length";
const char* ParamNames::BOX_PARAM_WIDTH  = "Width";
const char* ParamNames::BOX_PARAM_HEIGHT = "Height";

// Cylinder
const char* ParamNames::CYLINDER_PARAM_RADIUS = "Radius";
const char* ParamNames::CYLINDER_PARAM_HEIGHT = "Height";

// Sphere
const char* ParamNames::SPHERE_PARAM_RADIUS = "Radius";

// Cone
const char* ParamNames::CONE_PARAM_RADIUS = "Radius";
const char* ParamNames::CONE_PARAM_HEIGHT = "Height";

// Torus
const char* ParamNames::TORUS_PARAM_MAJOR_RADIUS = "MajorRadius";
const char* ParamNames::TORUS_PARAM_MINOR_RADIUS = "MinorRadius";

// Tube
const char* ParamNames::TUBE_PARAM_OUTER_RADIUS = "OuterRadius";
const char* ParamNames::TUBE_PARAM_INNER_RADIUS = "InnerRadius";
const char* ParamNames::TUBE_PARAM_HEIGHT = "Height";

// Extrusion
const char* ParamNames::EXTRUSION_PARAM_DEPTH = "Depth";
const char* ParamNames::EXTRUSION_PARAM_START_OFFSET = "StartOffset";

// Revolution
const char* ParamNames::REVOLUTION_PARAM_START_ANGLE = "StartAngle";
const char* ParamNames::REVOLUTION_PARAM_END_ANGLE = "EndAngle";
const char* ParamNames::REVOLUTION_PARAM_AXIS = "Axis";

// Imported Solid
const char* ParamNames::IMPORTED_SOLID_PARAM_FILE_PATH = "FilePath";

// Chamfer
const char* ParamNames::CHAMFER_DISTANCE1 = "Distance";

// Fillet
const char* ParamNames::FILLET_RADIUS = "Radius";

// Shell
const char* ParamNames::SHELL_THICKNESS = "Thickness";
const char* ParamNames::SHELL_DIRECTION = "Direction";
const char* ParamNames::SHELL_JOIN_TYPE = "JoinType";
const char* ParamNames::SHELL_OFFSET_MODE = "OffsetMode";
const char* ParamNames::SHELL_INTERSECTION = "Intersection";

// Draft
const char* ParamNames::DRAFT_ANGLE = "Angle";

// Move
const char* ParamNames::MOVE_VECTOR_X = "VectorX";
const char* ParamNames::MOVE_VECTOR_Y = "VectorY";
const char* ParamNames::MOVE_VECTOR_Z = "VectorZ";

// Rotate
const char* ParamNames::ROTATE_CENTER_X = "CenterX";
const char* ParamNames::ROTATE_CENTER_Y = "CenterY";
const char* ParamNames::ROTATE_CENTER_Z = "CenterZ";
const char* ParamNames::ROTATE_AXIS_DIRECTION_X = "AxisDirectionX";
const char* ParamNames::ROTATE_AXIS_DIRECTION_Y = "AxisDirectionY";
const char* ParamNames::ROTATE_AXIS_DIRECTION_Z = "AxisDirectionZ";
const char* ParamNames::ROTATE_ANGLE = "Angle";

// Mirror
const char* ParamNames::MIRROR_SOURCE = "Source";

// Pattern
const char* ParamNames::PATTERN_SOURCE = "Source";

// Linear Pattern
const char* ParamNames::LINEAR_PATTERN_DIRECTION_1ST_X = "Direction1stX";
const char* ParamNames::LINEAR_PATTERN_DIRECTION_1ST_Y = "Direction1stY";
const char* ParamNames::LINEAR_PATTERN_DIRECTION_1ST_Z = "Direction1stZ";
const char* ParamNames::LINEAR_PATTERN_COUNT_1ST = "Count1st";
const char* ParamNames::LINEAR_PATTERN_SPACING_1ST = "Spacing1st";
const char* ParamNames::LINEAR_PATTERN_DIRECTION_2ND_X = "Direction2ndX";;
const char* ParamNames::LINEAR_PATTERN_DIRECTION_2ND_Y = "Direction2ndY";
const char* ParamNames::LINEAR_PATTERN_DIRECTION_2ND_Z = "Direction2ndZ";;
const char* ParamNames::LINEAR_PATTERN_COUNT_2ND = "Count2nd";
const char* ParamNames::LINEAR_PATTERN_SPACING_2ND = "Spacing2nd";

// Circular Pattern
const char* ParamNames::CIRCULAR_PATTERN_CENTER_POINT_X = "CenterPointX";
const char* ParamNames::CIRCULAR_PATTERN_CENTER_POINT_Y = "CenterPointY";
const char* ParamNames::CIRCULAR_PATTERN_CENTER_POINT_Z = "CenterPointZ";
const char* ParamNames::CIRCULAR_PATTERN_AXIS_DIRECTION_X = "AxisDirectionX";
const char* ParamNames::CIRCULAR_PATTERN_AXIS_DIRECTION_Y = "AxisDirectionY";
const char* ParamNames::CIRCULAR_PATTERN_AXIS_DIRECTION_Z = "AxisDirectionZ";
const char* ParamNames::CIRCULAR_PATTERN_TOTAL_ANGLE = "TotalAngle";
const char* ParamNames::CIRCULAR_PATTERN_INSTANCE_COUNT = "InstanceCount";
const char* ParamNames::CIRCULAR_PATTERN_IS_CLOCKWISE = "IsClockWise";

// Helix
const char* ParamNames::HELIX_PITCH = "Pitch";
const char* ParamNames::HELIX_TURNS = "Turns";
const char* ParamNames::HELIX_START_ANGLE = "StartAngle";
const char* ParamNames::HELIX_IS_CLOCKWISE = "IsClockWise";
const char* ParamNames::HELIX_IS_REVERSED = "IsReversed";

// Datum Plane
const char* ParamNames::DATUM_PLANE_PARAM_PLANE = "Plane";

// Mirror
const char* ParamNames::MIRROR_PARAM_PLANE = "Plane";

// Solid
const char* ParamNames::SOLID_PARAM_COLOR = "Color";

NS_WY3D_END