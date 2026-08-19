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

#include "PasteElements.h"
#include <cassert>
#include <osg/Group>
#include <osg/ref_ptr>
#include <wy3dSolid.h>
#include <wy3dPrimitive.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchCurve.h>
#include <wyapClipboard.h>
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/RenderConst.h"
#include "utils/CopyPasteUtil.h"

PasteElements::PasteElements(wydb::Database* pDb, GuiCommand* pGuiCmd, GuiCmdEnvType mode, wydb::ElementId sketchId, const wy3d::SketchPlane& sketchPlane)
    : GuiCmdMakeElement(pGuiCmd), _mode(mode), _sketchId(sketchId), _sketchPlane(sketchPlane)
{
    _pat = new osg::PositionAttitudeTransform();
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->addTransient(_pat);
    }
}

PasteElements::~PasteElements()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->removeTransient(_pat);

        // show hidden element node
        for (const wydb::ElementId& hiddenElemId : _hiddenElemIds)
        {
            ElementNode* pHiddenElemNode = pActiveScene->getElementNode(hiddenElemId);
            if (pHiddenElemNode)
            {
                pHiddenElemNode->recomputeNodeMask();
            }
        }
    }
}

// 建模环境下粘贴元素是否支持重新设置新的位置
// 如果粘贴的对象全部都是顶层的基础形体,而且这些基础形体没有下属的实体修改元素,则支持重定位.
bool PasteElements::whetherSupportsRelocating_Modeling(const std::vector<wydb::Element*>& copyElements)
{
    bool ret(true);
    if (copyElements.empty())
    {
        ret = false;
    }

    for (wydb::Element* pElem : copyElements)
    {
        assert(pElem);

        // 是否是基本形体
        wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pElem);
        if (!pPrimitive)
        {
            ret = false;
            break;
        }

        // Owner是否为空
        if (!pPrimitive->getParent().isNull()) // 不为空不支持重定位
        {
            ret = false;
            break;
        }

        // 是否有下属的实体修改元素
        if (!pPrimitive->getModifications().empty())
        {
            ret = false;
            break;
        }
    }

    return ret;
}

PasteElements::InitRet PasteElements::init(const wy::Vector3& pos)
{
    if (!_pDb || !_pTopTrans || !_copyElements.empty() || _isFinished)
    {
        return InitRet::Failed;
    }

    // 剪贴板中没有复制元素
    if (!CopyPasteUtil::canPaste())
    {
        return InitRet::Failed;
    }

    // 生成拷贝元素
    std::shared_ptr<const wyap::ClipData> pClipData = Application::instance().getClipboard()->getClipData();
    const wyap::ElementsClipData* pElementsClipData = dynamic_cast<const wyap::ElementsClipData*>(pClipData.get());
    if (!pElementsClipData)
    {
        assert(false);
        return InitRet::Failed;
    }

    // 用一个独立临时事务来创建剪贴板元素
    // 草图环境下同时将图元加入草图, 避免提交孤儿图元导致 Scene 回调
    // 在 SketchEntityElementNode::generateRenderDataImpl 中因找不到父草图而 assert.
    {
        wydb::TransactionOption tempOpt;
        if (GuiCmdEnvType::Sketching == _mode)
            tempOpt.chainUpdateScope = wydb::ChainUpdateScope::Local;
        wydb::Transaction* pTempTrans = _pDb->getTransactionManager()->startTransaction("", tempOpt);
        if (!pTempTrans)
        {
            assert(false);
            return InitRet::Failed;
        }
        std::vector<wydb::Element*> copyElements;
        if (wy::ErrorStatus::Ok != wyap::Clipboard::createElements(pTempTrans, *pElementsClipData, copyElements))
        {
            assert(false);
            _pDb->getTransactionManager()->abortTransaction();
            return InitRet::Failed;
        }
        if (copyElements.empty())
        {
            assert(false);
            _pDb->getTransactionManager()->abortTransaction();
            return InitRet::Failed;
        }
        // 草图环境下在同一事务中将图元加入草图
        if (GuiCmdEnvType::Sketching == _mode)
        {
            wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTempTrans->getElementForWrite(_sketchId));
            if (!pSketch)
            {
                assert(false);
                _pDb->getTransactionManager()->abortTransaction();
                return InitRet::Failed;
            }
            for (wydb::Element* pElem : copyElements)
            {
                if (!pElem) continue;
                wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
                if (!pSketchEntity) { assert(false); continue; }
                pSketch->addEntity(pSketchEntity);
            }
        }
        _pDb->getTransactionManager()->endTransaction();

        // 拷贝数据
        for (wydb::Element* pElem : copyElements)
        {
            assert(pElem);
            CopyElement copyElem;
            copyElem.pElem = pElem;
            copyElem.initPosition.set(0.0, 0.0, 0.0);
            _copyElements.emplace_back(copyElem);
            if (pElem) { _newlyCreatedIds.insert(pElem->getId()); }
        }

        // 建模环境下, 不支持重定位则直接结束(无需交互预览)
        if (GuiCmdEnvType::Modeling == _mode)
        {
            if (!whetherSupportsRelocating_Modeling(copyElements))
                return InitRet::Success_End;
        }
    }

    // 计算初始原点
    if (GuiCmdEnvType::Sketching == _mode)
    {
        bool originInit(false);
        double xMin(0.0), yMin(0.0);
        for (CopyElement& copyElem : _copyElements)
        {
            const wydb::Element* pElem = copyElem.pElem;
            if (!pElem) continue;
            const wy3d::SketchCurve* pSketchCurve = wy3d::SketchCurve::cast(pElem);
            if (!pSketchCurve) continue;
            wy3d::BoundingBox2 bbox = pSketchCurve->getBoundingBox();
            if (bbox.isEmpty()) continue;
            if (!originInit)
            {
                xMin = bbox.min().x();
                yMin = bbox.min().y();
                originInit = true;
            }
            else
            {
                if (bbox.min().x() < xMin) xMin = bbox.min().x();
                if (bbox.min().y() < yMin) yMin = bbox.min().y();
            }
        }
        _initOrigin.set(xMin, yMin, 0.0);
    }
    else
    {
        bool originInit(false);
        double xMin(0.0), yMin(0.0), zMin(0.0);
        for (CopyElement& copyElem : _copyElements)
        {
            const wydb::Element* pElem = copyElem.pElem;
            if (!pElem) continue;
            const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
            if (!pSolid) continue;
            wy::Vector3 pos(0.0, 0.0, 0.0);
            if (const wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pSolid))
            {
                pos = pPrimitive->getPosition();
            }
            //added by wangyao 2025.02.20 {
            // TODO 对于拉伸&旋转&扫掠&放样需要根据草图计算实际的位置
            // }
            copyElem.initPosition = pos;
            if (!originInit)
            {
                xMin = pos.x();
                yMin = pos.y();
                zMin = pos.z();
                originInit = true;
            }
            else
            {
                if (pos.x() < xMin) xMin = pos.x();
                if (pos.y() < yMin) yMin = pos.y();
                if (pos.z() < zMin) zMin = pos.z();
            }
        }
        _initOrigin.set(xMin, yMin, zMin);
    }

    // 草图环境下的图元已在临时事务中加入草图, 此处仅建模环境需要开启事务.
    if (GuiCmdEnvType::Modeling == _mode)
    {
        wydb::TransactionOption option;
        option.chainUpdateScope = wydb::ChainUpdateScope::Cascade;
        wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
        if (!pTrans)
        {
            freeCopyElements();
            return InitRet::Failed;
        }
        for (const CopyElement& copyElem : _copyElements)
        {
            if (!copyElem.pElem) continue;
            pTrans->addNewlyCreatedElement(copyElem.pElem);
        }
        _pDb->getTransactionManager()->endTransaction();
    }

    // 生成跟随鼠标移动的渲染节点
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene)
    {
        assert(false);
        return InitRet::Success_End;
    }
    for (const CopyElement& copyElem : _copyElements)
    {
        if (!copyElem.pElem) continue;
        ElementNode* pElemNode = pActiveScene->getElementNode(copyElem.pElem->getId());
        if (!pElemNode)
        {
            if (GuiCmdEnvType::Sketching == _mode)
            {
                assert(false);
            }
            else // 在建模环境下复制草图时会复制草图图元,此时草图图元是没有ElementNode的
            {
                assert(dynamic_cast<wy3d::SketchEntity*>(copyElem.pElem));
            }
            continue;
        }

        // added by wangyao 2025.02.20 {
        // 在建模环境下排除草图对象,目前设计的产品是不允许移动草图的
        if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(copyElem.pElem))
        {
            continue;
        }
        // }
        
        osg::Group* pElemOsgRoot = pElemNode->getOsgNode();
        if (!pElemOsgRoot) continue;
        osg::ref_ptr<osg::Group> copy = new osg::Group(*pElemOsgRoot);
        copy->setNodeMask(~PICK_MASK); // added by wangyao 2025.04.28 不可PICK
        _pat->addChild(copy); // 浅拷贝

        // 暂时隐藏元素节点
        pElemOsgRoot->setNodeMask(0);
        _hiddenElemIds.emplace_back(copyElem.pElem->getId());
    }

    this->update(pos);

    return InitRet::Success_Continue;
}

bool PasteElements::update(const wy::Vector3& pos)
{
    if (GuiCmdEnvType::Sketching == _mode) // 草图模式
    {
        wy::Vector3 spnt = _sketchPlane.value(_initOrigin.x(), _initOrigin.y());
        wy::Vector3 epnt = _sketchPlane.value(pos.x(), pos.y());
        _pat->setPosition(osg::Vec3d(epnt.x() - spnt.x(), epnt.y() - spnt.y(), epnt.z() - spnt.z()));
    }
    else
    {
        wy::Vector3 retPos = (pos - _initOrigin);
        _pat->setPosition(osg::Vec3d(retPos.x(), retPos.y(), retPos.z()));
    }
    return true;
}

bool PasteElements::perform(const wy::Vector3& pos)
{
    if (!_pDb || !_pTopTrans || _copyElements.empty() || _isFinished)
    {
        return false;
    }
    wy::Vector3 translate = pos - _initOrigin;

    wydb::TransactionOption option;
    option.chainUpdateScope = GuiCmdEnvType::Sketching == _mode ? wydb::ChainUpdateScope::Local : wydb::ChainUpdateScope::Cascade;
    wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return false;
    }
    if (GuiCmdEnvType::Sketching == _mode)
    {
        for (const CopyElement& copyElem : _copyElements)
        {
            wydb::Element* pElem = copyElem.pElem;
            if (!pElem) continue;
            pElem->upgradeForWrite();
            wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
            if (!pSketchEntity) continue;
            pSketchEntity->translate(wy::Vector2(translate.x(), translate.y()));
        }
    }
    else
    {
        for (const CopyElement& copyElem : _copyElements)
        {
            wydb::Element* pElem = copyElem.pElem;
            if (!pElem)
            {
                assert(false);
                continue;
            }
            wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pElem);
            if (!pPrimitive)
            {
                continue;
            }
            pPrimitive->upgradeForWrite();
            pPrimitive->setPosition(copyElem.initPosition + translate);
        }
    }
    _pDb->getTransactionManager()->endTransaction();

    // 提交
    this->commit();

    return true;
}

void PasteElements::freeCopyElements()
{
    _copyElements.clear();
    _newlyCreatedIds.clear();
}
