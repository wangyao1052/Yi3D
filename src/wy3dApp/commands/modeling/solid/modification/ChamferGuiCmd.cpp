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

#include "commands/modeling/solid/modification/ChamferGuiCmd.h"
#include <QCoreApplication>
#include <QToolTip>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/nodes/SolidElementNode.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "utils/TopoShapeUtil.h"
#include "commands/dialogs/ChamferDialog.h"
#include "commands/modeling/solid/ChamferFilletCmdCommon.h"


// 前置过滤器: 确保只能选择单一主体的面或边
class ChamferGuiCmdPreSelFilter : public SelectPreFilterFunctor
{
public:
    ChamferGuiCmdPreSelFilter(const wyap::SelectionSet& ss) : _targetElemId(wydb::ElementId::kNull)
    {
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            _targetElemId = iter.current().getElementId();
            break;
        }
    }

    // 执行函数
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        if (id.isNull()) return SelectFilterStatus::Continue;

        if (_targetElemId.isNull() || id == _targetElemId)
        {
            return SelectFilterStatus::Ok;
        }
        else
        {
            return SelectFilterStatus::Continue;
        }
    }

private:
    wydb::ElementId _targetElemId;
};

ChamferGuiCmd::ChamferGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _distance(5.0)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

ChamferGuiCmd::~ChamferGuiCmd()
{
}

wyap::CmdExecution::StartResult ChamferGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
    _pointPickOption.selType = wy3d::SelectionType::SolidEdge | wy3d::SelectionType::SolidFace;
    _pointPickOption.acceptElement = false;
    this->gotoStep(Step::SelectEdges);

    return wyap::CmdExecution::StartResult::Succeeded;
}
void ChamferGuiCmd::onEnd()
{
    // 基类
    __baseClass::onEnd();

}
void ChamferGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 基类
    __baseClass::onAbort(cause);
}

void ChamferGuiCmd::reset()
{
    _step = Step::Undefined;
    _sels.clear();
    _distance = 0.0;

    _pPreview = nullptr;
    _pSelSetHighlightor = nullptr;
}

bool ChamferGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectEdges:
    {
        if (_sels.isEmpty())
        {
            assert(false);
            return false;
        }

        // next step
        this->gotoStep(Step::InputChamferDistance);
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

void ChamferGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectEdges:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("ChamferGuiCmd", "Select edges or faces; press Enter or Spacebar to confirm; press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
        // 高亮
        _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    }
    break;

    case Step::InputChamferDistance:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("ChamferGuiCmd", "Input chamfer distance."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Select);

        // 倒角对话框
        ChamferDialog dialog(5.0);
        if (QDialog::Accepted != dialog.exec())
        {
            this->reset(); // 重置数据
            this->requestAbort(AbortCause::UserCancel);  // 退出
            return;
        }
        _distance = dialog.getDistance(); // 对话框逻辑中已经添加了校验数据的合理性

        // 执行倒角
        unsigned int errorCode(0);
        if (!this->createChamfer(errorCode)) // 无论执行成功与否,后续逻辑都会退出命令
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
        // 清空提示
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void ChamferGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectEdges:
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
    }
    break;
    }

    return;
}

void ChamferGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (Step::SelectEdges == _step)
    {
        if (_pPreview)
        {
            const wyap::Selection& sel = _pPreview->getSelection();
            if (_pSelSetHighlightor->containsSelection(sel))
            {
                _pSelSetHighlightor->removeSelection(sel);
            }
            else
            {
                _pSelSetHighlightor->addSelection(sel);
            }

            // 过滤器
            _pointPickOption.pSelPreFilter = std::make_shared<ChamferGuiCmdPreSelFilter>(
                _pSelSetHighlightor->getSelectionSet());

            _pPreview = nullptr;
        }
    }

    return;
}

void ChamferGuiCmd::onEnterKey()
{
    if (Step::SelectEdges == _step)
    {
        _sels = _pSelSetHighlightor->getSelectionSet();
        if (!_sels.isEmpty())
        {
            this->finishStep(_step);
        }
    }
}

void ChamferGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool ChamferGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectEdges == _step;
}

void ChamferGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool ChamferGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectEdges == _step;
}

void ChamferGuiCmd::onContextMenuAction_ClearSelection()
{
    // 选择边
    if (Step::SelectEdges == _step)
    {
        if (_pSelSetHighlightor)
        {
            _pSelSetHighlightor->clearSelections();
        }
    }
}

bool ChamferGuiCmd::createChamfer(unsigned int& errorCode)
{
    return ChamferFilletCmdCommon::createChamferOrFillet<wy3d::Chamfer,
        wy3d::ErrorCode::CHAMFER_CreateChamferError>(_sels, _distance, errorCode);
}
