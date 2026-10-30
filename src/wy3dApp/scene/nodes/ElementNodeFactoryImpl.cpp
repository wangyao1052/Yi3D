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

#include "scene/nodes/ElementNodeFactoryImpl.h"

#include <cassert>
#include <utility>

#include <osg/Vec4>

#include <wyrxClassInfo.h>
#include <wydbDatabase.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dDatumPlane.h>
#include <wy3dBodyModification.h>
#include <wy3dCurve.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dBoolean.h>

#include "scene/nodes/SolidElementNode.h"
#include "scene/nodes/SheetElementNode.h"
#include "scene/nodes/SketchElementNode.h"
#include "scene/nodes/SketchEntityElementNode.h"
#include "scene/nodes/DatumPlaneElementNode.h"
#include "scene/nodes/BodyModificationElementNode.h"
#include "scene/nodes/CurveElementNode.h"
#include "scene/nodes/Sketch3DElementNode.h"
#include "scene/nodes/SketchEntity3DElementNode.h"
#include <wy3dApp/SketchEnvironment.h>
#include <wy3dApp/Sketch3DEnvironment.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace
{

osg::Vec4 toOsgColor(const wy3d::Color& color)
{
    constexpr float inv255 = 1.0f / 255.0f;
    return osg::Vec4(
        static_cast<float>(color.red) * inv255,
        static_cast<float>(color.green) * inv255,
        static_cast<float>(color.blue) * inv255,
        1.0f);
}

ElementNode* newElementNodeOfSolid(const wy3d::Solid* pSolid)
{
    assert(pSolid);
    wydb::ElementId id = pSolid->getId();
    auto newSolidNode = [id](const osg::Vec4& faceColor) -> SolidElementNode*
    {
        return new SolidElementNode(id, faceColor);
    };
    osg::Vec4 faceColor = toOsgColor(pSolid->getColor());

    if (pSolid->getParent().isNull())
    {
        return newSolidNode(faceColor);
    }

    if (pSolid->isCut())
    {
        if (pSolid->getParent().isNull())
        {
            return newSolidNode(faceColor);
        }
        else
        {
            return new BodyModificationElementNode(id);
        }
    }
    else
    {
        const wy3d::Boolean* pBoolean = wy3d::Boolean::cast(pSolid->getDatabase()->getElement(pSolid->getParent()));
        if (pBoolean)
        {
            return newSolidNode(faceColor);
        }
        else // merge
        {
            return new BodyModificationElementNode(id);
        }
    }
}

} // namespace

ElementNodeFactoryImpl::ElementNodeFactoryImpl()
{
    const wyrx::ClassInfo* pSketchEnvClassInfo = SketchEnvironment::classInfo();
    const wyrx::ClassInfo* pSketch3DEnvClassInfo = Sketch3DEnvironment::classInfo();

    this->registerCreator(wy3d::Solid::classInfo(),
        [](const wydb::Element* pElem, const ElementNodeCreateContext&) -> ElementNode*
        {
            const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
            if (!pSolid) return nullptr;
            return newElementNodeOfSolid(pSolid);
        });

    this->registerCreator(wy3d::Sheet::classInfo(),
        [](const wydb::Element* pElem, const ElementNodeCreateContext&) -> ElementNode*
        {
            const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem);
            if (!pSheet) return nullptr;
            return new SheetElementNode(pSheet->getId(), toOsgColor(pSheet->getColor()));
        });

    this->registerCreator(wy3d::BodyModification::classInfo(),
        [](const wydb::Element* pElem, const ElementNodeCreateContext&) -> ElementNode*
        {
            const wy3d::BodyModification* pBodyModification = wy3d::BodyModification::cast(pElem);
            if (!pBodyModification) return nullptr;
            return new BodyModificationElementNode(pBodyModification->getId());
        });

    this->registerCreator(wy3d::Curve::classInfo(),
        [](const wydb::Element* pElem, const ElementNodeCreateContext&) -> ElementNode*
        {
            const wy3d::Curve* pCurve = wy3d::Curve::cast(pElem);
            if (!pCurve) return nullptr;
            return new CurveElementNode(pCurve->getId());
        });

    this->registerCreator(wy3d::DatumPlane::classInfo(),
        [](const wydb::Element* pElem, const ElementNodeCreateContext&) -> ElementNode*
        {
            const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElem);
            if (!pDatumPlane) return nullptr;
            return new DatumPlaneElementNode(pDatumPlane->getId());
        });

    this->registerCreator(wy3d::Sketch::classInfo(),
        [](const wydb::Element* pElem, const ElementNodeCreateContext&) -> ElementNode*
        {
            const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem);
            if (!pSketch) return nullptr;
            return new SketchElementNode(pSketch->getId());
        });

    this->registerCreator(wy3d::SketchEntity::classInfo(),
        [pSketchEnvClassInfo](
            const wydb::Element* pElem, const ElementNodeCreateContext& context) -> ElementNode*
        {
            if (context.pEnvironmentClassInfo != pSketchEnvClassInfo)
            {
                return nullptr;
            }
            const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
            if (!pSketchEntity) return nullptr;
            return new SketchEntityElementNode(pSketchEntity->getId());
        });

    this->registerCreator(wy3d::Sketch3D::classInfo(),
        [](const wydb::Element* pElem, const ElementNodeCreateContext&) -> ElementNode*
        {
            const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pElem);
            if (!pSketch3D) return nullptr;
            return new Sketch3DElementNode(pSketch3D->getId());
        });

    this->registerCreator(wy3d::SketchEntity3D::classInfo(),
        [pSketch3DEnvClassInfo](
            const wydb::Element* pElem, const ElementNodeCreateContext& context) -> ElementNode*
        {
            if (context.pEnvironmentClassInfo != pSketch3DEnvClassInfo)
            {
                return nullptr;
            }
            const wy3d::SketchEntity3D* pSketchEntity3D = wy3d::SketchEntity3D::cast(pElem);
            if (!pSketchEntity3D) return nullptr;
            return new SketchEntity3DElementNode(pSketchEntity3D->getId());
        });
}

ElementNodeFactoryImpl::~ElementNodeFactoryImpl() = default;

bool ElementNodeFactoryImpl::registerCreator(
    wyrx::ClassInfo* pElemClassInfo, ElementNodeCreator creator)
{
    if (!pElemClassInfo || !creator) return false;

    auto ret = _creators.emplace(pElemClassInfo, std::move(creator));
    if (ret.second) _creatorCache.clear();
    return ret.second;
}

bool ElementNodeFactoryImpl::unregisterCreator(wyrx::ClassInfo* pElemClassInfo)
{
    if (!pElemClassInfo) return false;

    const bool erased = _creators.erase(pElemClassInfo) > 0;
    if (erased) _creatorCache.clear();
    return erased;
}

ElementNode* ElementNodeFactoryImpl::create(
    const wydb::Element* pElem,
    const ElementNodeCreateContext& context) const
{
    if (!pElem) return nullptr;

    wyrx::ClassInfo* pElemClassInfo = pElem->getClassInfo();

    auto cacheIter = _creatorCache.find(pElemClassInfo);
    if (cacheIter != _creatorCache.cend())
    {
        const ElementNodeCreator* pCreator = cacheIter->second;
        return pCreator ? (*pCreator)(pElem, context) : nullptr;
    }

    for (wyrx::ClassInfo* pClassInfo = pElemClassInfo;
        pClassInfo;
        pClassInfo = pClassInfo->parent())
    {
        auto iter = _creators.find(pClassInfo);
        if (iter == _creators.cend()) continue;

        const ElementNodeCreator* pCreator = &iter->second;
        _creatorCache.emplace(pElemClassInfo, pCreator);
        return (*pCreator)(pElem, context);
    }

    _creatorCache.emplace(pElemClassInfo, nullptr);
    assert(false);
    return nullptr;
}

// ElementNodeFactory 接口的构造/析构
ElementNodeFactory::ElementNodeFactory() = default;

ElementNodeFactory::~ElementNodeFactory() = default;

} // namespace wy3dApp
