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

#include "utils/GuiCommandUtil.h"
#include <cassert>
#include <wydbDatabase.h>
#include <wy3dSelectionType.h>
#include <wy3dSolid.h>
#include <wy3dDatumPlane.h>
#include <wy3dPattern.h>
#include <wy3dMirror.h>
#include <wyapSelManager.h>
#include "application/Application.h"
#include "environments/sketch/SketchEnvironment.h"
#include "utils/TopoShapeUtil.h"

bool GuiCommandUtil::getWorkingPlane(const wyap::Selection& sel, wy3d::SketchPlane& workPln)
{
    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    if (wy3d::UIntToSelectionType(sel.getSelectionType()) == wy3d::SelectionType::SolidFace)
    {
        if (sel.getSubPath().empty())
        {
            assert(false);
            return false;
        }
        unsigned int faceIndex = std::stoul(sel.getSubPath());
        if (faceIndex == -1)
        {
            assert(false);
            return false;
        }
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(sel.getElementId()));
        if (!pSolid)
        {
            assert(false);
            return false;
        }
        TopoDS_Shape shape = pSolid->getShape();
        return TopoShapeUtil::getShapeFacePlane(shape, faceIndex, workPln);
    }
    else if (wy3d::UIntToSelectionType(sel.getSelectionType()) == wy3d::SelectionType::Element)
    {
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(sel.getElementId()));
        if (!pDatumPlane)
        {
            assert(false);
            return false;
        }
        workPln = pDatumPlane->getPlane();
        return true;
    }
    else
    {
        assert(false);
        return false;
    }
}

wyap::SelectionSet GuiCommandUtil::filterTopSolidFeaturesFrom(const wyap::SelectionSet& ss)
{
    wyap::SelectionSet filterSS;
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return filterSS;

    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
        {
            continue;
        }
        const wydb::Element* pElem = pDb->getElement(sel.getElementId());
        if (!pElem) continue;
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
        if (!pSolid) continue;
        if (!pSolid->getParent().isNull()) continue;
        filterSS.add(sel);
    }

    return filterSS;
}

wydb::ElementId GuiCommandUtil::filterPatternSourceFrom(const wyap::SelectionSet& ss)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return wydb::ElementId::kNull;
    }

    if (ss.getCount() != 1)
    {
        return wydb::ElementId::kNull;
    }
    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
    {
        return wydb::ElementId::kNull;
    }
    const wydb::Element* pElem = pDb->getElement(sel.getElementId());
    if (!pElem)
    {
        assert(false);
        return wydb::ElementId::kNull;
    }
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
    if (!pSolid)
    {
        return wydb::ElementId::kNull;
    }
    if (pSolid->getParent().isNull() || // 顶层实体特征
        wy3d::Pattern::isValidSource(pSolid))
    {
        return pSolid->getId();
    }
    else
    {
        return wydb::ElementId::kNull;
    }
}

wydb::ElementId GuiCommandUtil::filterMirrorSourceFrom(const wyap::SelectionSet& ss)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return wydb::ElementId::kNull;
    }

    if (ss.getCount() != 1)
    {
        return wydb::ElementId::kNull;
    }
    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
    {
        return wydb::ElementId::kNull;
    }
    const wydb::Element* pElem = pDb->getElement(sel.getElementId());
    if (!pElem)
    {
        assert(false);
        return wydb::ElementId::kNull;
    }
    const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
    if (!pSolid)
    {
        return wydb::ElementId::kNull;
    }

    if (wy3d::Mirror::isValidSource(pSolid))
    {
        return pSolid->getId();
    }
    else
    {
        return wydb::ElementId::kNull;
    }
}

const wy3d::Solid* GuiCommandUtil::autoGetSolidToCut(const wydb::Database* pDb)
{
    if (!pDb) return nullptr;

    unsigned int num(0);
    const wy3d::Solid* pSolidToCut(nullptr);
    for (auto iter = pDb->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(iter.current()));
        if (!pSolid) continue;
        if (pSolid->isCut()) continue;
        if (!pSolid->getParent().isNull()) continue;

        pSolidToCut = pSolid;
        ++num;
        if (num > 1)
        {
            return nullptr;
        }
    }

    return pSolidToCut;
}

void GuiCommandUtil::clearSelections()
{
    if (wyap::SelManager* pSelMgr = Application::instance().getSelManager())
    {
        pSelMgr->beginChange();
        pSelMgr->clearSelections();
        pSelMgr->endChange();
    }
    else
    {
        assert(false);
    }
}

GuiCmdSketchInfo GuiCommandUtil::initSketchInfo()
{
    GuiCmdSketchInfo info;
    wyap::Environment* pEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    if (SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnv))
    {
        info.sketchPlane = pSketchEnv->getSketchPlane();
        info.sketchId = pSketchEnv->getSketchId();
        info.pSketchSnapSys = pSketchEnv->getSnapSystem();
    }
    return info;
}