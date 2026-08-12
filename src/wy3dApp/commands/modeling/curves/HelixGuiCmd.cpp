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

#include "HelixGuiCmd.h"

#include <QCoreApplication>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dImpl.h>
#include <wy3dSketchProfile.h>
#include <wy3dErrorCode.h>
#include <wy3dSolid.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "utils/GuiCommandUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "select/filters/SolidToCutSelFilter.h"
#include "commands/dialogs/HelixDialog.h"


HelixGuiCmd::HelixGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _sketchId(wydb::ElementId::kNull)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

HelixGuiCmd::~HelixGuiCmd()
{
}

wyap::CmdExecution::StartResult HelixGuiCmd::onStart()
{
    // 是否有可用的草图
    if (!SketchUtil::hasUnusedSketch(Application::instance().getActiveDatabase()))
    {
        MessageBoxUtil::showInformation_NoAvailableSketches();
        return wyap::CmdExecution::StartResult::Rejected;
    }

    // 基类
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化:点选选项
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Sketch);
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.pSelFilter = std::make_shared<SingleClassSelFilter>(wy3d::Sketch::classInfo());
    
    // 初始化:步骤
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    wydb::ElementId sketchId(wydb::ElementId::kNull);
    if (this->isValidSketchSelectionSet(ss, sketchId) && !sketchId.isNull())
    {
        _sketchId = sketchId;
        this->finishStep(Step::SelectSketch);
    }
    else
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->gotoStep(Step::SelectSketch);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

void HelixGuiCmd::reset()
{
    _step = Step::Undefined;
    _sketchId = wydb::ElementId::kNull;
    _pValidSketch = nullptr;
    _pInvalidSketchTooltip = nullptr;
}

bool HelixGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSketch:
    {
        if (_sketchId.isNull())
        {
            assert(false);
            return false;
        }

        // 下一步
        this->gotoStep(Step::InputHelixData);
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

void HelixGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectSketch:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("HelixGuiCmd",
            "Select a sketch that contains a single circle."));

        // 设置鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    break;

    case Step::InputHelixData:
    {
        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("HelixGuiCmd", "Input helix data."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Select);

        // 倒角对话框
        HelixDialog dialog(5.0, 5.0, 0.0, false);
        if (QDialog::Accepted != dialog.exec())
        {
            this->reset(); // 重置数据
            this->requestAbort(AbortCause::UserCancel);  // 退出
            return;
        }
        double pitch = dialog.getPitch();
        double turns = dialog.getTurns();
        double startAngle = dialog.getStartAngle();
        bool isClockWise = dialog.isClockWise();

        // 执行倒角
        unsigned int errorCode(0);
        if (!this->createHelix(_sketchId, pitch, turns, startAngle, isClockWise, errorCode)) // 无论执行成功与否,后续逻辑都会退出命令
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
        }

        // 重置数据
        this->reset();

        // exit
        this->requestEnd();
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);

        assert(false);
    }
    break;
    }
}

void HelixGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        std::pair<wydb::ElementId, wy::Vector3> pickRet = this->pointPickElement(event.x, event.y, _pointPickOption);
        wydb::ElementId pickedSketchId = pickRet.first;

        preview(pickedSketchId);

        if (!pickedSketchId.isNull() && !_pValidSketch)
        {
            if (!_pInvalidSketchTooltip || _pInvalidSketchTooltip->getSketchId() != pickedSketchId)
            {
                _pInvalidSketchTooltip = std::make_shared<InvalidSketchToolTip>(pickedSketchId,
                    _sketchId2ValidInfo[pickedSketchId].error);
            }
            Application::instance().setCursor(CursorType::Forbid);
        }
        else
        {
            _pInvalidSketchTooltip = nullptr;
            Application::instance().setCursor(CursorType::SelectElements);
        }
    }

    return;
}

void HelixGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (_step == Step::SelectSketch)
    {
        if (_pValidSketch)
        {
            _sketchId = _pValidSketch->getSketchId();
            this->finishStep(_step);
        }
    }

    return;
}

void HelixGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectSketch != _step) return;
    if (!_sketchId.isNull()) return;

    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(id));
    if (!pSketch) return;

    QString error;
    if (this->isValidSketch(id, error))
    {
        _sketchId = id;
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->addSelection(wyap::Selection(id));
        Application::instance().getSelManager()->endChange();
        this->finishStep(Step::SelectSketch);
    }
    else
    {
        MessageBoxUtil::showWarning(error);
    }
}

bool HelixGuiCmd::isValidSketchSelectionSet(const wyap::SelectionSet& ss, wydb::ElementId& sketchId)
{
    sketchId = wydb::ElementId::kNull;
    if (ss.getCount() != 1)
    {
        return false;
    }
    const wyap::Selection& sel = ss.createIterator().current();
    if (sel.getSelectionType() != static_cast<unsigned int>(wy3d::SelectionType::Element))
    {
        return false;
    }
    wydb::ElementId id = sel.getElementId();
    QString error;
    if (this->isValidSketch(id, error))
    {
        sketchId = id;
        return true;
    }
    else
    {
        return false;
    }
}

bool HelixGuiCmd::isValidSketch(const wydb::ElementId& sketchId, QString& error)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pSketch) return false;
    if (!pSketch->getParent().isNull()) return false;

    return SketchUtil::isValidHelixProfile(*pSketch, error);
}

void HelixGuiCmd::preview(wydb::ElementId sketchId)
{
    if (wydb::ElementId::kNull == sketchId)
    {
        _pValidSketch = nullptr;
        return;
    }

    if (_pValidSketch && _pValidSketch->getSketchId() == sketchId)
    {
        return;
    }
    _pValidSketch = nullptr;

    auto iter = _sketchId2ValidInfo.find(sketchId);
    if (iter != _sketchId2ValidInfo.cend())
    {
        if (iter->second.valid)
        {
            _pValidSketch = std::make_shared<ValidSketchTransient>(sketchId);
        }
    }
    else
    {
        QString error;
        SketchValidInfo info;
        if (this->isValidSketch(sketchId, error))
        {
            _pValidSketch = std::make_shared<ValidSketchTransient>(sketchId);
            info.valid = true;
        }
        else
        {
            info.valid = false;
            info.error = error;
        }
        _sketchId2ValidInfo[sketchId] = info;
    }
}

bool HelixGuiCmd::createHelix(
    const wydb::ElementId& sketchId,
    double pitch,
    double turns,
    double startAngle,
    bool isClockWise,
    unsigned int& errorCode)
{
    errorCode = 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    if (sketchId.isNull())
    {
        assert(false);
        return false;
    }

    // 获取草图
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pConstSketch) return false;
    if (!pConstSketch->getParent().isNull())
    {
        assert(false);
        return false;
    }

    // 创建螺旋线
    wy3d::Helix* pHelix = nullptr;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch)
    {
        assert(false);
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != wy3d::Helix::create(pTrans, pSketch, pitch, turns, startAngle, pHelix) || !pHelix)
    {
        assert(false);
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pHelix->setClockWise(isClockWise))
    {
        assert(false);
        goto ABORT_TRANS;
    }
    pDb->getTransactionManager()->endTransaction();
    // added by wangyao 2025.04.16 {
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        pDb->getTransactionManager()->getChainUpdateFeedback(pHelix->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }
    // }
    // 刷新基准面的显示
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->updateDatumPlaneVisualSize(pDb);
    }
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    return false;
}
