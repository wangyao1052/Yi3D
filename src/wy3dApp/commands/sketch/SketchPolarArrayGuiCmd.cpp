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

#include "SketchPolarArrayGuiCmd.h"

#include <QCoreApplication>
#include <QMessageBox>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include "snap/SnapSystemBase.h"
#include <wyapClipboard.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>
#include <wy3dImpl.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "commands/transient/BasicTransient.h"
#include "utils/MathUtils.h"
#include "commands/sketch/dialogs/SketchPolarArrayDialog.h"
#include "snap/SketchSnapSystem.h"
#include "select/filters/CommonSelFilters.h"


bool SketchPolarArrayGuiCmd::isValidTotalAngleAndCount(double totalAngle, unsigned int count)
{
    if (count < MIN_COUNT) return false;
    if (count > MAX_COUNT) return false;
    if (totalAngle <= 0.0 || totalAngle > wy3d::TWO_PI) return false;

    double minItemAngle = wy3d::TWO_PI / MAX_COUNT;
    double itemAngle(0.0);
    if (std::fabs(totalAngle - wy3d::TWO_PI) <= wy3d::EPS)
    {
        itemAngle = wy3d::TWO_PI / count;
    }
    else
    {
        itemAngle = totalAngle / (count - 1);
    }
    if (itemAngle < minItemAngle) return false;

    return true;
}

SketchPolarArrayGuiCmd::SketchPolarArrayGuiCmd()
    : OsgGuiCommand(), _step(Step::Undefined), _center(), _totalAngle(wy3d::TWO_PI), _count(6), _isCCW(true)
{
    _options.pointSelect = true;
    _options.boxSelect = true;
}

SketchPolarArrayGuiCmd::~SketchPolarArrayGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchPolarArrayGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 初始化
    if (Application::instance().getSelManager()->getSelections().isEmpty())
    {
        this->gotoStep(Step::Step1_SelectElements);
    }
    else
    {
        this->finishStep(Step::Step1_SelectElements);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void SketchPolarArrayGuiCmd::onEnd()
{
    // 取消阵列
    if (_pPolarArray)
    {
        _pPolarArray = nullptr;
    }

    GuiCommand::onEnd();

}
void SketchPolarArrayGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 取消阵列
    if (_pPolarArray)
    {
        _pPolarArray = nullptr;
    }

    GuiCommand::onAbort(cause);

}

void SketchPolarArrayGuiCmd::reset()
{
    if (_pPolarArray) _pPolarArray = nullptr;

    _step = Step::Undefined;
    _ids.clear();
    _center.set(0.0, 0.0);
    _totalAngle = wy3d::TWO_PI;
    _count = 6;
    _isCCW = true;
    _pSnapContext = nullptr;

    this->gotoStep(Step::Step1_SelectElements);
}

bool SketchPolarArrayGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::Step1_SelectElements:
    {
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (ss.isEmpty()) return false;
        _ids.clear();
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            _ids.insert(iter.current().getElementId());
        }
        if (_ids.empty()) return false;

        // next step
        this->gotoStep(Step::Step2_SpecifyCenter);
        return true;
    }
    break;

    case Step::Step2_SpecifyCenter:
    {
        // next step
        this->gotoStep(Step::Step3_SpecifyItems);
        return true;
    }
    break;

    case Step::Step3_SpecifyItems:
    {
        if (_pPolarArray)
        {
            if (!_pPolarArray->perform(_ids, _center, _totalAngle, _count, _isCCW))
            {
                assert(false);
                this->reset(); // 模态对话框点击[确定]按钮后走到这一步,执行失败后需要reset.
                return false;
            }
            _pPolarArray->commit();
            _pPolarArray = nullptr;
        }

        // next step
        //this->gotoStep(Step::Step1_SelectElements);
        this->requestEnd();
        return true;
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return false;
}

void SketchPolarArrayGuiCmd::gotoStep(Step step)
{
    _step = step;

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();
    // 清空草图捕捉结果
    if (_sketchInfo.pSketchSnapSys)
    {
        _sketchInfo.pSketchSnapSys->clearSnapResult();
    }

    switch (step)
    {
    case Step::Step1_SelectElements:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = true;
        selOptions.boxSelect = true;
        selOptions.selectionType = wy3d::SelectionType::Element;
        selOptions.pickMask = static_cast<unsigned int>(ElementNodeType::SketchEntity);
        selOptions.filter = std::make_shared<SingleClassSelFilter>(wy3d::SketchEntity::classInfo());
        selOptions.preview = true;
        selOptions.selectMode = SelectMode::Incremental;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchPolarArray",
            "Select elements to perform polar array; press Enter or Spacebar to confirm; press Esc to cancel."));
        Application::instance().setCursor(CursorType::SelectElements);

        if (_sketchInfo.pSketchSnapSys)
        {
            _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
        }
    }
    break;

    case Step::Step2_SpecifyCenter:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchPolarArray",
            "Specify the center point of polar array; you can directly input the coordinate values."));
        Application::instance().setCursor(CursorType::Locate);

        _pSnapContext = std::make_shared<SketchLocateContext>(wydb::ElementId::kNull);
    }
    break;

    case Step::Step3_SpecifyItems:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);


        SketchPolarArrayDialog::Options options;
        options.minTotalAngle = 0.0;
        options.maxTotalAngle = 360.0;
        options.minCount = 2;
        options.maxCount = 1000;
        SketchPolarArrayDialog dialog(
            QCoreApplication::translate("SketchPolarArray", "Sketch Polar Array"),
            360.0, 6, true, options);
        if (QDialog::Accepted != dialog.exec())
        {
            this->reset();
            return;
        }
        _totalAngle = dialog.getTotalAngle();
        _count = dialog.getCount();
        _isCCW = dialog.isCCW();

        if (!isValidTotalAngleAndCount(_totalAngle, _count))
        {
            QMessageBox::warning(nullptr, QCoreApplication::translate("SketchPolarArray", "Sketch Polar Array"),
                QCoreApplication::translate("SketchPolarArray", "The angle between items is too small!"));
            this->reset();
            return;
        }

        _pPolarArray = std::make_shared<SketchPolarArrayElements>(this, _sketchInfo.sketchPlane, _sketchInfo.sketchId);
        if (!_pPolarArray->init(_ids, _center, _totalAngle, _count, _isCCW))
        {
            _pPolarArray = nullptr;
            this->reset();
            return;
        }

        this->finishStep(_step);
    }
    break;

    default:
    {
        GuiCmdSelectOptions selOptions;
        selOptions.pointSelect = false;
        selOptions.boxSelect = false;
        this->configSelect(selOptions);

        Application::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void SketchPolarArrayGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::Step2_SpecifyCenter:
    {
        wy::Vector2 center = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
    }
    break;

    case Step::Step1_SelectElements:
    case Step::Step3_SpecifyItems:
    default:
    {
    }
    break;
    }

    return;
}

void SketchPolarArrayGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::Step2_SpecifyCenter:
    {
        wy::Vector2 center = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        {
            _center = center;
        }
    }
    break;

    case Step::Step1_SelectElements:
    case Step::Step3_SpecifyItems:
    default:
    {
    }
    break;
    }

    return;
}

void SketchPolarArrayGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (Step::Step2_SpecifyCenter == _step)
    {
        this->finishStep(_step);
    }

    return;
}

void SketchPolarArrayGuiCmd::onEnterKey()
{
    if (Step::Step1_SelectElements == _step)
    {
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (!ss.isEmpty())
        {
            this->finishStep(_step);
        }
    }
}

void SketchPolarArrayGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool SketchPolarArrayGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::Step1_SelectElements == _step;
}

void SketchPolarArrayGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool SketchPolarArrayGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::Step1_SelectElements == _step;
}

void SketchPolarArrayGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::Step1_SelectElements == _step)
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
    }
}

SketchPolarArrayElements::SketchPolarArrayElements(GuiCommand* pGuiCmd, const wy3d::SketchPlane& sketchPlane, wydb::ElementId sketchId) : GuiCmdMakeElement(pGuiCmd), _sketchPlane(sketchPlane), _sketchId(sketchId)
{
}

SketchPolarArrayElements::~SketchPolarArrayElements()
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (pActiveScene)
    {
        for (osg::ref_ptr<osg::MatrixTransform> item : _items)
        {
            pActiveScene->removeTransient(item);
        }
    }
}

bool SketchPolarArrayElements::init(const std::set<wydb::ElementId>& ids,
    const wy::Vector2& center, double totalAngle, unsigned int count, bool isCCW)
{
    if (ids.empty()) return false;

    if (count < 2)
    {
        assert(false);
        return false;
    }
    if (!SketchPolarArrayGuiCmd::isValidTotalAngleAndCount(totalAngle, count))
    {
        assert(false);
        return false;
    }
    double itemAngle(0.0);
    if (std::fabs(totalAngle - wy3d::TWO_PI) <= wy3d::EPS)
        itemAngle = wy3d::TWO_PI / count;
    else
        itemAngle = totalAngle / (count - 1);
    if (!isCCW) itemAngle = -itemAngle;

    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return false;
    size_t num = static_cast<size_t>(count) - 1;
    osg::ref_ptr<osg::MatrixTransform> matrixTransform = new osg::MatrixTransform();
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
        matrixTransform->addChild(new osg::Group(*pElemOsgRoot));
    }

    assert(_pGuiCmd);
    const wy3d::SketchPlane& sketchPlane = _sketchPlane;
    osg::Vec3d normal = MathUtils::toVec3d(sketchPlane.getNormal());
    osg::Vec3d centerPnt3d = MathUtils::toVec3d(sketchPlane.value(center));

    double rotationAngle(0.0);
    for (size_t i = 1; i <= num; ++i)
    {
        rotationAngle = i * itemAngle;
        osg::ref_ptr<osg::MatrixTransform> copy = new osg::MatrixTransform(*matrixTransform);
        copy->setMatrix(
            osg::Matrix::translate(-centerPnt3d)
            * osg::Matrix::rotate(rotationAngle, normal)
            * osg::Matrix::translate(centerPnt3d));
        _items.emplace_back(copy);
        pActiveScene->addTransient(copy);
    }

    return true;
}

bool SketchPolarArrayElements::perform(const std::set<wydb::ElementId>& ids, const wy::Vector2& center,
    double totalAngle, unsigned int count, bool isCCW)
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

    if (count < 2)
    {
        assert(false);
        return false;
    }
    if (!SketchPolarArrayGuiCmd::isValidTotalAngleAndCount(totalAngle, count))
    {
        assert(false);
        return false;
    }
    size_t num = static_cast<size_t>(count) - 1;

    double itemAngle(0.0);
    if (std::fabs(totalAngle - wy3d::TWO_PI) <= wy3d::EPS)
        itemAngle = wy3d::TWO_PI / count;
    else
        itemAngle = totalAngle / (count - 1);
    if (!isCCW) itemAngle = -itemAngle;

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
        double angle;
        ArrayElemItem() : pElem(nullptr), angle(0.0) {}
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
    for (size_t i = 1; i <= num; ++i)
    {
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
            arrayElemItem.angle = i * itemAngle;
            arrayElems.emplace_back(arrayElemItem);
        }
    }

    if (!retCopyElems)
    {
        for (const ArrayElemItem& arrayElemItem : arrayElems)
        {
            assert(arrayElemItem.pElem);
            wydb::deleteElement(arrayElemItem.pElem);
        }
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
            if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchEntity)) goto ABORT_TRANS;            if (wy::ErrorStatus::Ok != pSketchEntity->rotateAround(center, arrayElemItem.angle)) goto ABORT_TRANS;
        }
    }
    _pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    _pDb->getTransactionManager()->abortTransaction();
    for (const ArrayElemItem& arrayElemItem : arrayElems)
    {
        assert(arrayElemItem.pElem);
        wydb::deleteElement(arrayElemItem.pElem);
    }
    return false;
}