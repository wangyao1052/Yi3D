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

#include <wy3dApp/ParamNamesTranslation.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dCone.h>
#include <wy3dTorus.h>
#include <wy3dTube.h>
#include <wy3dExtrusion.h>
#include <wy3dRevolution.h>
#include <wy3dImportedSolid.h>
#include <wy3dImportedSheet.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dRevolvedSheet.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wy3dChamfer.h>
#include <wy3dFillet.h>
#include <wy3dShell.h>
#include <wy3dDraft.h>
#include <wy3dMove.h>
#include <wy3dRotate.h>
#include <wy3dPattern.h>
#include <wy3dLinearPattern.h>
#include <wy3dCircularPattern.h>

#include <wy3dSketchPoint.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <wy3dSketchArc3D.h>
#include <wy3dSketchEllipse3D.h>
#include <wy3dSketchEllipseArc3D.h>
#include <wy3dSketchSpline3D.h>

#include <wy3dHelix.h>
#include <wy3dThicken.h>
#include <wy3dSolidify.h>
#include <wy3dOffsetSheet.h>
#include <wy3dSewnSheet.h>

#include <wy3dDatumPlane.h>
#include <wy3dMirror.h>
#include <wy3dApp/wy3dAppDefs.h>
#include <wy3dSketchEntity.h>

namespace wy3dApp {

static inline std::string globalName(const std::string& className, const std::string& paramName)
{
    return className + "::" + paramName;
}

ParamNamesTranslation& ParamNamesTranslation::instance()
{
    static ParamNamesTranslation instance(nullptr);
    return instance;
}

ParamNamesTranslation::ParamNamesTranslation(QObject* parent) : QObject(parent)
{
    // Box
    {
        const std::string& className = wy3d::Box::classInfo()->className();
        _paramName2DisplayName[globalName(className, wy3d::Box::PARAM_LENGTH)] = tr("Box Length");
        _paramName2DisplayName[globalName(className, wy3d::Box::PARAM_WIDTH)] = tr("Box Width");
        _paramName2DisplayName[globalName(className, wy3d::Box::PARAM_HEIGHT)] = tr("Box Height");
    }
    // Cylinder
    {
        const std::string& className = wy3d::Cylinder::classInfo()->className();
        _paramName2DisplayName[globalName(className, wy3d::Cylinder::PARAM_RADIUS)] = tr("Cylinder Radius");
        _paramName2DisplayName[globalName(className, wy3d::Cylinder::PARAM_HEIGHT)] = tr("Cylinder Height");
    }
    // Sphere
    {
        const std::string& className = wy3d::Sphere::classInfo()->className();
        _paramName2DisplayName[globalName(className, wy3d::Sphere::PARAM_RADIUS)] = tr("Sphere Radius");
    }
    // Cone
    {
        const std::string& className = wy3d::Cone::classInfo()->className();
        _paramName2DisplayName[globalName(className, wy3d::Cone::PARAM_RADIUS)] = tr("Cone Radius");
        _paramName2DisplayName[globalName(className, wy3d::Cone::PARAM_HEIGHT)] = tr("Cone Height");
    }
    // Torus
    {
        const std::string& className = wy3d::Torus::className();
        _paramName2DisplayName[globalName(className, wy3d::Torus::PARAM_MAJOR_RADIUS)] = tr("Torus Major Radius");
        _paramName2DisplayName[globalName(className, wy3d::Torus::PARAM_MINOR_RADIUS)] = tr("Torus Minor Radius");
    }
    // Tube
    {
        const std::string& className = wy3d::Tube::className();
        _paramName2DisplayName[globalName(className, wy3d::Tube::PARAM_OUTER_RADIUS)] = tr("Tube Outer Radius");
        _paramName2DisplayName[globalName(className, wy3d::Tube::PARAM_INNER_RADIUS)] = tr("Tube Inner Radius");
        _paramName2DisplayName[globalName(className, wy3d::Tube::PARAM_HEIGHT)] = tr("Tube Height");
    }
    // Extrusion
    {
        const std::string& className = wy3d::Extrusion::className();
        _paramName2DisplayName[globalName(className, wy3d::Extrusion::PARAM_DEPTH)] = tr("Depth");
        _paramName2DisplayName[globalName(className, wy3d::Extrusion::PARAM_START_OFFSET)] = tr("Extrusion Start Offset");
        _paramName2DisplayName[globalName(className, wy3d::Extrusion::PARAM_DIRECTION)] = tr("Direction");
    }
    // Revolution
    {
        const std::string& className = wy3d::Revolution::className();
        _paramName2DisplayName[globalName(className, wy3d::Revolution::PARAM_AXIS)] = tr("Axis", "wy3d::Revolution");
        _paramName2DisplayName[globalName(className, wy3d::Revolution::PARAM_START_ANGLE)] = tr("Start Angle", "wy3d::Revolution");
        _paramName2DisplayName[globalName(className, wy3d::Revolution::PARAM_END_ANGLE)] = tr("End Angle", "wy3d::Revolution");
    }
    // ExtrudedSheet
    {
        const std::string& className = wy3d::ExtrudedSheet::className();
        _paramName2DisplayName[globalName(className, wy3d::Extrusion::PARAM_DEPTH)] = tr("Depth");
        _paramName2DisplayName[globalName(className, wy3d::Extrusion::PARAM_START_OFFSET)] = tr("Extrusion Start Offset");
        _paramName2DisplayName[globalName(className, wy3d::Extrusion::PARAM_DIRECTION)] = tr("Direction");
    }
    // RevolvedSheet
    {
        const std::string& className = wy3d::RevolvedSheet::className();
        _paramName2DisplayName[globalName(className, wy3d::Revolution::PARAM_START_ANGLE)] = tr("Start Angle", "wy3d::RevolvedSheet");
        _paramName2DisplayName[globalName(className, wy3d::Revolution::PARAM_END_ANGLE)] = tr("End Angle", "wy3d::RevolvedSheet");
    }
    // Imported Solid
    {
        const std::string& className = wy3d::ImportedSolid::className();
        _paramName2DisplayName[globalName(className, wy3d::ImportedSolid::PARAM_FILE_PATH)] = tr("File Path", "wy3d::ImportedSolid");
    }
    // Imported Sheet
    {
        const std::string& className = wy3d::ImportedSheet::className();
        _paramName2DisplayName[globalName(className, wy3d::ImportedSolid::PARAM_FILE_PATH)] = tr("File Path", "wy3d::ImportedSolid");
    }
    // Chamfer
    {
        const std::string& className = wy3d::Chamfer::className();
        _paramName2DisplayName[globalName(className, wy3d::Chamfer::PARAM_DISTANCE1)] = tr("Distance 1", "wy3d::Chamfer");
        _paramName2DisplayName[globalName(className, wy3d::Chamfer::PARAM_DISTANCE2)] = tr("Distance 2", "wy3d::Chamfer");
        _paramName2DisplayName[globalName(className, wy3d::Chamfer::PARAM_ANGLE)] = tr("Angle", "wy3d::Chamfer");
        _paramName2DisplayName[globalName(className, wy3d::Chamfer::PARAM_TYPE)] = tr("Chamfer Type", "wy3d::Chamfer");
        _paramName2DisplayName[globalName(className, wy3d::Chamfer::PARAM_IS_FLIPPED)] = tr("Flip Direction", "wy3d::Chamfer");
    }
    // Fillet
    {
        const std::string& className = wy3d::Fillet::className();
        _paramName2DisplayName[globalName(className, wy3d::Fillet::PARAM_RADIUS)] = tr("Radius", "wy3d::Fillet");
    }
    // Shell
    {
        const std::string& className = wy3d::Shell::className();
        _paramName2DisplayName[globalName(className, wy3d::Shell::PARAM_THICKNESS)] = tr("Thickness", "wy3d::Shell");
        _paramName2DisplayName[globalName(className, wy3d::Shell::PARAM_DIRECTION)] = tr("Inward Offset", "wy3d::Shell");
        _paramName2DisplayName[globalName(className, wy3d::Shell::PARAM_JOIN_TYPE)] = tr("Join Type", "wy3d::Shell");
        _paramName2DisplayName[globalName(className, wy3d::Shell::PARAM_OFFSET_MODE)] = tr("Offset Mode", "wy3d::Shell");
        _paramName2DisplayName[globalName(className, wy3d::Shell::PARAM_INTERSECTION)] = tr("Intersection", "wy3d::Shell");
    }
    // Draft
    {
        const std::string& className = wy3d::Draft::className();
        _paramName2DisplayName[globalName(className, wy3d::Draft::PARAM_ANGLE)] = tr("Angle", "wy3d::Draft");
    }
    // Move
    {
        const std::string& className = wy3d::Move::className();
        _paramName2DisplayName[globalName(className, wy3d::Move::PARAM_VECTOR_X)] = tr("X", "wy3d::Move");
        _paramName2DisplayName[globalName(className, wy3d::Move::PARAM_VECTOR_Y)] = tr("Y", "wy3d::Move");
        _paramName2DisplayName[globalName(className, wy3d::Move::PARAM_VECTOR_Z)] = tr("Z", "wy3d::Move");
    }
    // Rotate
    {
        const std::string& className = wy3d::Rotate::className();
        _paramName2DisplayName[globalName(className, wy3d::Rotate::PARAM_CENTER_X)] = tr("CenterX", "wy3d::Rotate");
        _paramName2DisplayName[globalName(className, wy3d::Rotate::PARAM_CENTER_Y)] = tr("CenterY", "wy3d::Rotate");
        _paramName2DisplayName[globalName(className, wy3d::Rotate::PARAM_CENTER_Z)] = tr("CenterZ", "wy3d::Rotate");
        _paramName2DisplayName[globalName(className, wy3d::Rotate::PARAM_AXIS_DIRECTION_X)] = tr("DirectionX", "wy3d::Rotate");
        _paramName2DisplayName[globalName(className, wy3d::Rotate::PARAM_AXIS_DIRECTION_Y)] = tr("DirectionY", "wy3d::Rotate");
        _paramName2DisplayName[globalName(className, wy3d::Rotate::PARAM_AXIS_DIRECTION_Z)] = tr("DirectionZ", "wy3d::Rotate");
        _paramName2DisplayName[globalName(className, wy3d::Rotate::PARAM_ANGLE)] = tr("Angle", "wy3d::Rotate");
    }
    // Pattern
    {
        const std::string& className = wy3d::Pattern::className();
        _paramName2DisplayName[globalName(className, wy3d::Pattern::PARAM_SOURCE)] = tr("Source", "wy3d::Pattern");
    }
    // Linear Pattern
    {
        const std::string& className = wy3d::LinearPattern::className();
        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_DIRECTION_1ST_X)] = tr("Direction1 X", "wy3d::LinearPattern");
        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_DIRECTION_1ST_Y)] = tr("Direction1 Y", "wy3d::LinearPattern");
        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_DIRECTION_1ST_Z)] = tr("Direction1 Z", "wy3d::LinearPattern");
        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_COUNT_1ST)] = tr("Count1", "wy3d::LinearPattern");
        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_SPACING_1ST)] = tr("Spacing1", "wy3d::LinearPattern");

        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_DIRECTION_2ND_X)] = tr("Direction2 X", "wy3d::LinearPattern");
        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_DIRECTION_2ND_Y)] = tr("Direction2 Y", "wy3d::LinearPattern");
        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_DIRECTION_2ND_Z)] = tr("Direction2 Z", "wy3d::LinearPattern");
        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_COUNT_2ND)] = tr("Count2", "wy3d::LinearPattern");
        _paramName2DisplayName[globalName(className, wy3d::LinearPattern::PARAM_SPACING_2ND)] = tr("Spacing2", "wy3d::LinearPattern");
    }
    // Circular Pattern
    {
        const std::string& className = wy3d::CircularPattern::className();
        _paramName2DisplayName[globalName(className, wy3d::CircularPattern::PARAM_CENTER_POINT_X)] = tr("Center Point X", "wy3d::CircularPattern");
        _paramName2DisplayName[globalName(className, wy3d::CircularPattern::PARAM_CENTER_POINT_Y)] = tr("Center Point Y", "wy3d::CircularPattern");
        _paramName2DisplayName[globalName(className, wy3d::CircularPattern::PARAM_CENTER_POINT_Z)] = tr("Center Point Z", "wy3d::CircularPattern");
        _paramName2DisplayName[globalName(className, wy3d::CircularPattern::PARAM_AXIS_DIRECTION_X)] = tr("Axis Direction X", "wy3d::CircularPattern");
        _paramName2DisplayName[globalName(className, wy3d::CircularPattern::PARAM_AXIS_DIRECTION_Y)] = tr("Axis Direction Y", "wy3d::CircularPattern");
        _paramName2DisplayName[globalName(className, wy3d::CircularPattern::PARAM_AXIS_DIRECTION_Z)] = tr("Axis Direction Z", "wy3d::CircularPattern");
        _paramName2DisplayName[globalName(className, wy3d::CircularPattern::PARAM_TOTAL_ANGLE)] = tr("TotalAngle", "wy3d::CircularPattern");
        _paramName2DisplayName[globalName(className, wy3d::CircularPattern::PARAM_INSTANCE_COUNT)] = tr("Instance Count", "wy3d::CircularPattern");
        _paramName2DisplayName[globalName(className, wy3d::CircularPattern::PARAM_IS_CLOCKWISE)] = tr("Is ClockWise", "wy3d::CircularPattern");
    }
    // Sketch Point
    {
        const std::string& className = wy3d::SketchPoint::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchEntity::PARAM_ID)] = tr("ID", "wy3d::SketchEntity");
        _paramName2DisplayName[globalName(className, wy3d::SketchPoint::PARAM_POSITION_X)] = tr("Position X", "wy3d::SketchPoint");
        _paramName2DisplayName[globalName(className, wy3d::SketchPoint::PARAM_POSITION_Y)] = tr("Position Y", "wy3d::SketchPoint");
    }
    // Sketch Curve
    // Keys are declaring classes, so inherited params are registered here once, not in subclasses.
    {
        const std::string& className = wy3d::SketchCurve::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchEntity::PARAM_ID)] = tr("ID", "wy3d::SketchEntity");
        _paramName2DisplayName[globalName(className, wy3d::SketchCurve::PARAM_IS_CONSTRUCTION)] = tr("Is Construction", "wy3d::SketchCurve");
    }
    // Sketch Line
    {
        const std::string& className = wy3d::SketchLine::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchLine::PARAM_START_X)] = tr("Start Point X", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine::PARAM_START_Y)] = tr("Start Point Y", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine::PARAM_END_X)] = tr("End Point X", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine::PARAM_END_Y)] = tr("End Point Y", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine::PARAM_LENGTH)] = tr("Length", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine::PARAM_ANGLE)] = tr("Angle", "wy3d::SketchLine");
    }
    // Sketch Center Line
    {
        const std::string& className = wy3d::SketchCenterLine::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchCenterLine::PARAM_START_X)] = tr("Start Point X", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchCenterLine::PARAM_START_Y)] = tr("Start Point Y", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchCenterLine::PARAM_END_X)] = tr("End Point X", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchCenterLine::PARAM_END_Y)] = tr("End Point Y", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine::PARAM_LENGTH)] = tr("Length", "wy3d::SketchLine");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine::PARAM_ANGLE)] = tr("Angle", "wy3d::SketchLine");
    }
    // Sketch Circle
    {
        const std::string& className = wy3d::SketchCircle::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle::PARAM_CENTER_X)] = tr("Center Point X", "wy3d::SketchCircle");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle::PARAM_CENTER_Y)] = tr("Center Point Y", "wy3d::SketchCircle");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle::PARAM_RADIUS)] = tr("Radius", "wy3d::SketchCircle");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle::PARAM_DIAMETER)] = tr("Diameter", "wy3d::SketchCircle");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle::PARAM_PERIMETER)] = tr("Perimeter", "wy3d::SketchCircle");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle::PARAM_AREA)] = tr("Area", "wy3d::SketchCircle");
    }
    // Sketch Arc
    {
        const std::string& className = wy3d::SketchArc::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchArc::PARAM_CENTER_X)] = tr("Center Point X", "wy3d::SketchArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc::PARAM_CENTER_Y)] = tr("Center Point Y", "wy3d::SketchArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc::PARAM_RADIUS)] = tr("Radius", "wy3d::SketchArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc::PARAM_START_ANGLE)] = tr("Start Angle", "wy3d::SketchArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc::PARAM_END_ANGLE)] = tr("End Angle", "wy3d::SketchArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc::PARAM_TOTAL_ANGLE)] = tr("Total Angle", "wy3d::SketchArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc::PARAM_LENGTH)] = tr("Arc Length", "wy3d::SketchArc");
    }
    // Sketch Ellipse
    {
        const std::string& className = wy3d::SketchEllipse::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse::PARAM_CENTER_X)] = tr("Center Point X", "wy3d::SketchEllipse");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse::PARAM_CENTER_Y)] = tr("Center Point Y", "wy3d::SketchEllipse");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse::PARAM_MAJOR_RADIUS)] = tr("Major Radius", "wy3d::SketchEllipse");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse::PARAM_MINOR_RADIUS)] = tr("Minor Radius", "wy3d::SketchEllipse");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse::PARAM_RADIUS_RATIO)] = tr("Radius Ratio", "wy3d::SketchEllipse");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse::PARAM_MAJOR_AXIS_ANGLE)] = tr("Major Axis Angle", "wy3d::SketchEllipse");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse::PARAM_PERIMETER)] = tr("Perimeter", "wy3d::SketchEllipse");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse::PARAM_AREA)] = tr("Area", "wy3d::SketchEllipse");
    }
    // Sketch Ellipse Arc
    {
        const std::string& className = wy3d::SketchEllipseArc::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc::PARAM_CENTER_X)] = tr("Center Point X", "wy3d::SketchEllipseArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc::PARAM_CENTER_Y)] = tr("Center Point Y", "wy3d::SketchEllipseArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc::PARAM_MAJOR_RADIUS)] = tr("Major Radius", "wy3d::SketchEllipseArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc::PARAM_MINOR_RADIUS)] = tr("Minor Radius", "wy3d::SketchEllipseArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc::PARAM_RADIUS_RATIO)] = tr("Radius Ratio", "wy3d::SketchEllipseArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc::PARAM_MAJOR_AXIS_ANGLE)] = tr("Major Axis Angle", "wy3d::SketchEllipseArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc::PARAM_START_ANGLE)] = tr("Start Angle", "wy3d::SketchEllipseArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc::PARAM_END_ANGLE)] = tr("End Angle", "wy3d::SketchEllipseArc");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc::PARAM_PERIMETER)] = tr("Perimeter", "wy3d::SketchEllipseArc");
    }
    // Sketch Spline
    {
        const std::string& className = wy3d::SketchSpline::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchSpline::PARAM_ORDER)] = tr("Order", "wy3d::SketchSpline");
    }
    // SketchCurve3D
    {
        const std::string& className = wy3d::SketchCurve3D::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchCurve3D::PARAM_ID)] = tr("ID", "wy3d::SketchCurve3D");
    }
    // SketchLine3D
    {
        const std::string& className = wy3d::SketchLine3D::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchLine3D::PARAM_START_X)] = tr("Start Point X", "wy3d::SketchLine3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine3D::PARAM_START_Y)] = tr("Start Point Y", "wy3d::SketchLine3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine3D::PARAM_START_Z)] = tr("Start Point Z", "wy3d::SketchLine3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine3D::PARAM_END_X)] = tr("End Point X", "wy3d::SketchLine3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine3D::PARAM_END_Y)] = tr("End Point Y", "wy3d::SketchLine3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine3D::PARAM_END_Z)] = tr("End Point Z", "wy3d::SketchLine3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchLine3D::PARAM_LENGTH)] = tr("Length", "wy3d::SketchLine3D");
    }
    // SketchCircle3D
    {
        const std::string& className = wy3d::SketchCircle3D::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_CENTER_X)] = tr("Center Point X", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_CENTER_Y)] = tr("Center Point Y", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_CENTER_Z)] = tr("Center Point Z", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_NORMAL_X)] = tr("Normal X", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_NORMAL_Y)] = tr("Normal Y", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_NORMAL_Z)] = tr("Normal Z", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_XDIR_X)] = tr("Start Direction X", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_XDIR_Y)] = tr("Start Direction Y", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_XDIR_Z)] = tr("Start Direction Z", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_RADIUS)] = tr("Radius", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_DIAMETER)] = tr("Diameter", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_PERIMETER)] = tr("Perimeter", "wy3d::SketchCircle3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchCircle3D::PARAM_AREA)] = tr("Area", "wy3d::SketchCircle3D");
    }
    // SketchArc3D
    {
        const std::string& className = wy3d::SketchArc3D::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_CENTER_X)] = tr("Center Point X", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_CENTER_Y)] = tr("Center Point Y", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_CENTER_Z)] = tr("Center Point Z", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_NORMAL_X)] = tr("Normal X", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_NORMAL_Y)] = tr("Normal Y", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_NORMAL_Z)] = tr("Normal Z", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_XDIR_X)] = tr("Start Direction X", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_XDIR_Y)] = tr("Start Direction Y", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_XDIR_Z)] = tr("Start Direction Z", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_RADIUS)] = tr("Radius", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_START_ANGLE)] = tr("Start Angle", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_END_ANGLE)] = tr("End Angle", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_TOTAL_ANGLE)] = tr("Total Angle", "wy3d::SketchArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchArc3D::PARAM_LENGTH)] = tr("Arc Length", "wy3d::SketchArc3D");
    }
    // SketchEllipse3D
    {
        const std::string& className = wy3d::SketchEllipse3D::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_CENTER_X)] = tr("Center Point X", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_CENTER_Y)] = tr("Center Point Y", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_CENTER_Z)] = tr("Center Point Z", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_NORMAL_X)] = tr("Normal X", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_NORMAL_Y)] = tr("Normal Y", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_NORMAL_Z)] = tr("Normal Z", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_XDIR_X)] = tr("Major Axis X", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_XDIR_Y)] = tr("Major Axis Y", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_XDIR_Z)] = tr("Major Axis Z", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_MAJOR_RADIUS)] = tr("Major Radius", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_MINOR_RADIUS)] = tr("Minor Radius", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_RADIUS_RATIO)] = tr("Radius Ratio", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_PERIMETER)] = tr("Perimeter", "wy3d::SketchEllipse3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipse3D::PARAM_AREA)] = tr("Area", "wy3d::SketchEllipse3D");
    }
    // SketchEllipseArc3D
    {
        const std::string& className = wy3d::SketchEllipseArc3D::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_CENTER_X)] = tr("Center Point X", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_CENTER_Y)] = tr("Center Point Y", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_CENTER_Z)] = tr("Center Point Z", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_NORMAL_X)] = tr("Normal X", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_NORMAL_Y)] = tr("Normal Y", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_NORMAL_Z)] = tr("Normal Z", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_XDIR_X)] = tr("Major Axis X", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_XDIR_Y)] = tr("Major Axis Y", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_XDIR_Z)] = tr("Major Axis Z", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_MAJOR_RADIUS)] = tr("Major Radius", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_MINOR_RADIUS)] = tr("Minor Radius", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_RADIUS_RATIO)] = tr("Radius Ratio", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_START_ANGLE)] = tr("Start Angle", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_END_ANGLE)] = tr("End Angle", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_TOTAL_ANGLE)] = tr("Total Angle", "wy3d::SketchEllipseArc3D");
        _paramName2DisplayName[globalName(className, wy3d::SketchEllipseArc3D::PARAM_LENGTH)] = tr("Arc Length", "wy3d::SketchEllipseArc3D");
    }
    // SketchSpline3D
    {
        const std::string& className = wy3d::SketchSpline3D::className();
        _paramName2DisplayName[globalName(className, wy3d::SketchSpline3D::PARAM_ORDER)] = tr("Order", "wy3d::SketchSpline3D");
    }
    // DatumPlane
    {
        const std::string& className = wy3d::DatumPlane::className();
        _paramName2DisplayName[globalName(className, wy3d::DatumPlane::PARAM_PLANE)] = tr("Plane", "wy3d::DatumPlane");
    }
    // Mirror
    {
        const std::string& className = wy3d::Mirror::className();
        _paramName2DisplayName[globalName(className, wy3d::Mirror::PARAM_SOURCE)] = tr("Source", "wy3d::Mirror");
        _paramName2DisplayName[globalName(className, wy3d::Mirror::PARAM_PLANE)] = tr("Plane", "wy3d::Mirror");
    }
    // Solid
    {
        const std::string& className = wy3d::Solid::className();
        _paramName2DisplayName[globalName(className, wy3d::Solid::PARAM_COLOR)] = tr("Color", "wy3d::Solid");
    }
    // Sheet (base: color param inherited by all Sheet subclasses)
    {
        const std::string& className = wy3d::Sheet::className();
        _paramName2DisplayName[globalName(className, wy3d::Solid::PARAM_COLOR)] = tr("Color", "wy3d::Solid");
    }
    // Thicken
    {
        const std::string& className = wy3d::Thicken::className();
        _paramName2DisplayName[globalName(className, wy3d::Thicken::PARAM_SOURCE)] = tr("Source", "wy3d::Thicken");
        _paramName2DisplayName[globalName(className, wy3d::Thicken::PARAM_THICKNESS)] = tr("Thickness", "wy3d::Thicken");
        _paramName2DisplayName[globalName(className, wy3d::Thicken::PARAM_DIRECTION)] = tr("Direction", "wy3d::Thicken");
    }
    // Solidify
    {
        const std::string& className = wy3d::Solidify::className();
        _paramName2DisplayName[globalName(className, wy3d::Solidify::PARAM_SOURCE)] = tr("Source", "wy3d::Solidify");
    }
    // OffsetSheet
    {
        const std::string& className = wy3d::OffsetSheet::className();
        _paramName2DisplayName[globalName(className, wy3d::OffsetSheet::PARAM_TARGET)] = tr("Target", "wy3d::OffsetSheet");
        _paramName2DisplayName[globalName(className, wy3d::OffsetSheet::PARAM_OFFSET)] = tr("Offset", "wy3d::OffsetSheet");
    }
    // Sewn Sheet
    {
        const std::string& className = wy3d::SewnSheet::className();
        _paramName2DisplayName[globalName(className, wy3d::SewnSheet::PARAM_TOLERANCE)] = tr("Tolerance", "wy3d::SewnSheet");
    }
    // helix
    {
        const std::string& className = wy3d::Helix::className();
        _paramName2DisplayName[globalName(className, wy3d::Helix::PARAM_PITCH)] = tr("Pitch", "wy3d::Helix");
        _paramName2DisplayName[globalName(className, wy3d::Helix::PARAM_TURNS)] = tr("Turns", "wy3d::Helix");
        _paramName2DisplayName[globalName(className, wy3d::Helix::PARAM_START_ANGLE)] = tr("Start Angle", "wy3d::Helix");
        _paramName2DisplayName[globalName(className, wy3d::Helix::PARAM_IS_CLOCKWISE)] = tr("ClockWise", "wy3d::Helix");
        _paramName2DisplayName[globalName(className, wy3d::Helix::PARAM_IS_REVERSED)] = tr("Reversed", "wy3d::Helix");
    }
}

ParamNamesTranslation::~ParamNamesTranslation()
{
}

QString ParamNamesTranslation::getParamDisplayName(const std::string& className, const std::string& paramName) const
{
    auto iter = _paramName2DisplayName.find(globalName(className, paramName));
    if (iter != _paramName2DisplayName.cend())
    {
        return iter->second;
    }
    else
    {
        return QString::fromStdString(paramName);
    }
}

} // namespace wy3dApp
