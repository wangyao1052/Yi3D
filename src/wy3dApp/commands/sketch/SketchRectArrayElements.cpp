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

#include "SketchRectArrayElements.h"
#include "SketchRectArrayGuiCmd.h"
#include <cassert>
#include <osg/Group>
#include <osg/ref_ptr>
#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wyapClipboard.h>
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/RenderConst.h"
#include "utils/MathUtils.h"

SketchRectArrayElements::SketchRectArrayElements(GuiCommand* pGuiCmd, const wy3d::SketchPlane& sketchPlane, wydb::ElementId sketchId) : GuiCmdMakeElement(pGuiCmd), _sketchPlane(sketchPlane), _sketchId(sketchId), _cols(0), _rows(0)
{
}

SketchRectArrayElements::~SketchRectArrayElements()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        for (osg::ref_ptr<osg::PositionAttitudeTransform> pat : _pats)
        {
            pActiveScene->removeTransient(pat);
        }
    }
}

bool SketchRectArrayElements::init(const std::set<wydb::ElementId>& ids, unsigned int cols, unsigned int rows,
    double colSpacing, double rowSpacing)
{
    if (ids.empty()) return false;

    if (!SketchRectArrayGuiCmd::isValidRowsCols(cols, rows))
    {
        assert(false);
        return false;
    }
    _cols = cols;
    _rows = rows;

    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return false;

    size_t num = static_cast<size_t>(cols) * static_cast<size_t>(rows) - 1;
    _pats.reserve(num);
    osg::ref_ptr<osg::PositionAttitudeTransform> pat = new osg::PositionAttitudeTransform();
    for (const wydb::ElementId& id : ids)
    {
        ElementNode* pElemNode = pActiveScene->getElementNode(id);
        if (!pElemNode)
        {
            assert(false);
            continue;
        }
        osg::Group* pElemOsgRoot = pElemNode->getOsgNode();
        if (!pElemOsgRoot) continue;
        osg::ref_ptr<osg::Group> copy = new osg::Group(*pElemOsgRoot);
        copy->setNodeMask(~PICK_MASK); // 设置不被点选
        pat->addChild(copy);
    }

    assert(_pGuiCmd);
    const wy3d::SketchPlane& sketchPlane = _sketchPlane;
    wy::Vector3 xDir = sketchPlane.getXDir();
    wy::Vector3 yDir = sketchPlane.getYDir();

    size_t colIndex(0), rowIndex(0);
    for (size_t i = 1; i <= num; ++i)
    {
        colIndex = i % cols;
        rowIndex = i / cols;
        wy::Vector3 vec = xDir * colIndex * colSpacing + yDir * rowIndex * rowSpacing;
        osg::ref_ptr<osg::PositionAttitudeTransform> copy = new osg::PositionAttitudeTransform(*pat);
        copy->setPosition(MathUtils::toVec3d(vec));
        _pats.emplace_back(copy);
        pActiveScene->addTransient(copy);
    }

    return true;
}

bool SketchRectArrayElements::update(double colSpacing, double rowSpacing)
{
    assert(_pGuiCmd);
    const wy3d::SketchPlane& sketchPlane = _sketchPlane;
    wy::Vector3 xDir = sketchPlane.getXDir();
    wy::Vector3 yDir = sketchPlane.getYDir();

    size_t num = _pats.size();
    size_t colIndex(0), rowIndex(0);
    for (size_t i = 1; i <= num; ++i)
    {
        colIndex = i % _cols;
        rowIndex = i / _cols;
        wy::Vector3 vec = xDir * colIndex * colSpacing + yDir * rowIndex * rowSpacing;
        _pats[i-1]->setPosition(MathUtils::toVec3d(vec));
    }

    return true;
}

bool SketchRectArrayElements::perform(const std::set<wydb::ElementId>& ids,
    unsigned int cols, unsigned int rows, double colSpacing, double rowSpacing)
{
    if (!_pDb)
    {
        assert(false);
        return false;
    }
    if (!_pGuiCmd)
    {
        assert(false);
        return false;
    }

    if (!SketchRectArrayGuiCmd::isValidRowsCols(cols, rows))
    {
        assert(false);
        return false;
    }
    size_t num = static_cast<size_t>(cols) * static_cast<size_t>(rows) - 1;

    std::vector<wydb::ElementId> elemIds;
    for (const wydb::ElementId& id : ids)
    {
        const wydb::Element* pElem = _pDb->getElement(id);
        if (!pElem) continue;
        if (const wy3d::SketchEntity* pSketchEnt = wy3d::SketchEntity::cast(pElem))
        {
            elemIds.push_back(id);
        }
    }
    if (elemIds.empty()) return false;

    std::shared_ptr<wyap::ElementsClipData> pClipData = wyap::Clipboard::newElementsClipData(_pDb, elemIds);
    if (!pClipData) return false;

    struct ArrayElemItem
    {
        wydb::Element* pElem;
        size_t colIndex;
        size_t rowIndex;
        ArrayElemItem() : pElem(nullptr), colIndex(0), rowIndex(0) {}
    };
    std::list<ArrayElemItem> arrayElems;

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return false;
    }

    bool retCopyElems(true);
    size_t colIndex(0), rowIndex(0);
    for (size_t i = 1; i <= num; ++i)
    {
        colIndex = i % _cols;
        rowIndex = i / _cols;

        std::vector<wydb::Element*> copyedElems;
        if (wy::ErrorStatus::Ok != wyap::Clipboard::createElements(pTrans, *pClipData, copyedElems))
        {
            assert(copyedElems.empty());
            retCopyElems = false;
            break;
        }
        if (copyedElems.empty())
        {
            assert(false);
            retCopyElems = false;
            break;
        }

        for (wydb::Element* pCopyedElem : copyedElems)
        {
            ArrayElemItem arrayElemItem;
            arrayElemItem.pElem = pCopyedElem;
            arrayElemItem.colIndex = colIndex;
            arrayElemItem.rowIndex = rowIndex;
            arrayElems.emplace_back(arrayElemItem);
        }
    }

    if (!retCopyElems)
    {
        return false;
    }

    {
        wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(_sketchId));
        if (!pSketch) goto ABORT_TRANS;
        for (const ArrayElemItem& arrayElemItem : arrayElems)
        {
            assert(arrayElemItem.pElem);
            wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(arrayElemItem.pElem);
            if (!pSketchEntity)
            {
                assert(false);
                continue;
            }
            if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchEntity)) goto ABORT_TRANS;
            wy::Vector2 vec(arrayElemItem.colIndex * colSpacing, arrayElemItem.rowIndex * rowSpacing);
            if (wy::ErrorStatus::Ok != pSketchEntity->translate(vec)) goto ABORT_TRANS;
        }
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    return false;
}
