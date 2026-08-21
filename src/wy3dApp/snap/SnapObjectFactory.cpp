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

#include "SnapObjectFactory.h"

#include <wy3dDatumPlane.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dCone.h>
#include <wy3dTorus.h>
#include <wy3dTube.h>
#include <wy3dBoolean.h>
#include <wy3dUnion.h>
#include <wy3dDifference.h>
#include <wy3dIntersection.h>
#include <wy3dExtrusion.h>
#include <wy3dRevolution.h>
#include <wy3dSweep.h>
#include <wy3dLoft.h>
#include <wy3dSketch.h>
#include <wy3dSketchPoint.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <wy3dHelix.h>
#include <wy3dImportedSolid.h>
#include <wy3dNonParametricSolid.h>
#include <wy3dSolidify.h>

#include "elements/BoxSnapObjectCreator.h"
#include "elements/CylinderSnapObjectCreator.h"
#include "elements/SphereSnapObjectCreator.h"
#include "elements/ConeSnapObjectCreator.h"
#include "elements/TorusSnapObjectCreator.h"
#include "elements/TubeSnapObjectCreator.h"
#include "elements/BooleanSnapObjectCreator.h"
#include "elements/TopoShapeSnapObjectCreator.h"
#include "elements/SketchSnapObjectCreator.h"
#include "elements/SketchPointSnapObjectCreator.h"
#include "elements/SketchLineSnapObjectCreator.h"
#include "elements/SketchCenterLineSnapObjectCreator.h"
#include "elements/SketchCircleSnapObjectCreator.h"
#include "elements/SketchArcSnapObjectCreator.h"
#include "elements/SketchEllipseSnapObjectCreator.h"
#include "elements/SketchEllipseArcSnapObjectCreator.h"
#include "elements/SketchSplineSnapObjectCreator.h"
#include "elements/HelixSnapObjectCreator.h"
#include <wy3dExtrudedSheet.h>
#include <wy3dRevolvedSheet.h>
#include <wy3dSweptSheet.h>
#include <wy3dLoftedSheet.h>
#include <wy3dImportedSheet.h>
#include <wy3dThicken.h>
#include <wy3dOffsetSheet.h>
#include <wy3dPlanarSheet.h>
#include <wy3dSewnSheet.h>
#include <wy3dNonParametricSheet.h>
#define REGISTER_CREATOR(CLASS, SNAP_OBJ_CREATOR) \
    { \
        static_assert(std::is_base_of_v<ElemSnapObjectCreator, SNAP_OBJ_CREATOR>, \
                 "SNAP_OBJ_CREATOR must inherit from ElemSnapObjectCreator"); \
        assert(!_className2SnapObjCreator.count(CLASS::className())); \
        _className2SnapObjCreator[CLASS::className()] = std::make_unique<SNAP_OBJ_CREATOR>(); \
    }

SnapObjectFactory::SnapObjectFactory()
{
    // 参照面
    REGISTER_CREATOR(wy3d::DatumPlane, EmptySnapObjectCreator);

    // 基本几何体
    REGISTER_CREATOR(wy3d::Box, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::Cylinder, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::Sphere, SphereSnapObjectCreator);
    REGISTER_CREATOR(wy3d::Cone, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::Torus, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::Tube, TopoShapeSnapObjectCreator);

    // 布尔运算
    REGISTER_CREATOR(wy3d::Boolean, BooleanSnapObjectCreator);
    REGISTER_CREATOR(wy3d::Union, BooleanSnapObjectCreator);
    REGISTER_CREATOR(wy3d::Difference, BooleanSnapObjectCreator);
    REGISTER_CREATOR(wy3d::Intersection, BooleanSnapObjectCreator);

    // 拉伸特征
    REGISTER_CREATOR(wy3d::Extrusion, TopoShapeSnapObjectCreator);
    // 旋转特征
    REGISTER_CREATOR(wy3d::Revolution, TopoShapeSnapObjectCreator);
    // 扫描特征
    REGISTER_CREATOR(wy3d::Sweep, TopoShapeSnapObjectCreator);
    // 放样特征
    REGISTER_CREATOR(wy3d::Loft, TopoShapeSnapObjectCreator);
    // 导入实体特征
    REGISTER_CREATOR(wy3d::ImportedSolid, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::ImportedSheet, TopoShapeSnapObjectCreator);

    REGISTER_CREATOR(wy3d::Thicken, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::Solidify, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::NonParametricSolid, TopoShapeSnapObjectCreator);

    // 草图相关
    REGISTER_CREATOR(wy3d::Sketch, SketchSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SketchPoint, SketchPointSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SketchLine, SketchLineSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SketchCenterLine, SketchCenterLineSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SketchCircle, SketchCircleSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SketchArc, SketchArcSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SketchEllipse, SketchEllipseSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SketchEllipseArc, SketchEllipseArcSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SketchSpline, SketchSplineSnapObjectCreator);

    // 螺旋线
    REGISTER_CREATOR(wy3d::Helix, HelixSnapObjectCreator);

    // 曲面
    REGISTER_CREATOR(wy3d::ExtrudedSheet, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::RevolvedSheet, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SweptSheet, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::LoftedSheet, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::OffsetSheet, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::PlanarSheet, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::SewnSheet, TopoShapeSnapObjectCreator);
    REGISTER_CREATOR(wy3d::NonParametricSheet, TopoShapeSnapObjectCreator);
}

std::list<wyap::SnapObjectSPtr> SnapObjectFactory::createSnapObjects(const wydb::Element* pElem)
{
    if (!pElem)
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    auto iter = _className2SnapObjCreator.find(pElem->getClassInfo()->className());
    if (iter == _className2SnapObjCreator.cend())
    {
        assert(false);
        return std::list<wyap::SnapObjectSPtr>();
    }

    return iter->second->createSnapObjects(pElem);
}