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

#include "commands/modeling/solid/modification/ShellGuiCmd.h"
#include <QCoreApplication>
#include <QToolTip>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dSketch.h>
#include <wy3dDefaultChainUpdateFeedback.h>
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
#include "commands/dialogs/DoubleValueInputDialog.h"
#include "commands/modeling/solid/ChamferFilletCmdCommon.h"


// 前置过滤器: 确保只能选择单一主体的面或边
class ShellGuiCmdPreSelFilter : public SelectPreFilterFunctor
{
public:
    ShellGuiCmdPreSelFilter(const wyap::SelectionSet& ss) : _targetElemId(wydb::ElementId::kNull)
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

ShellGuiCmd::ShellGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _thickness(1.0)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

ShellGuiCmd::~ShellGuiCmd()
{
}

wyap::CmdExecution::StartResult ShellGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
    _pointPickOption.selType = wy3d::SelectionType::Face;
    _pointPickOption.acceptElement = false;
    _pSelSetHighlightor = std::make_shared<SelectionSetHighlightor>(wyap::SelectionSet());
    this->gotoStep(Step::SelectFaces);

    return wyap::CmdExecution::StartResult::Succeeded;
}
void ShellGuiCmd::onEnd()
{
    // 基类
    __baseClass::onEnd();

}
void ShellGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 基类
    __baseClass::onAbort(cause);

}

void ShellGuiCmd::reset()
{
    _step = Step::Undefined;
    _sels.clear();
    _thickness = 0.0;

    _pPreview = nullptr;
    _pSelSetHighlightor->clearSelections();
}

bool ShellGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectFaces:
    {
        if (_sels.isEmpty())
        {
            assert(false);
            return false;
        }

        // next step
        this->gotoStep(Step::InputThickness);
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

void ShellGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectFaces:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("ShellGuiCmd",
            "Select solid faces to remove; press Enter or Spacebar to confirm; press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
    }
    break;

    case Step::InputThickness:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("ShellGuiCmd", "Input shell thickness."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Select);

        // 倒角对话框
        DoubleValueInputDialog dialog(1.0,
            QCoreApplication::translate("ShellGuiCmd", "Shell"),
            QCoreApplication::translate("ShellGuiCmd", "Thickness"));
        if (QDialog::Accepted != dialog.exec())
        {
            this->reset(); // 重置数据
            this->requestAbort(AbortCause::UserCancel);  // 退出
            return;
        }
        _thickness = dialog.getValue(); // 对话框逻辑中已经添加了校验数据的合理性

        // 执行倒角
        unsigned int errorCode(0);
        if (!this->createShell(_sels, _thickness, errorCode)) // 无论执行成功与否,后续逻辑都会退出命令
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

void ShellGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectFaces:
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
    }
    break;
    }

    return;
}

void ShellGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    if (Step::SelectFaces == _step)
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
            _pointPickOption.pSelPreFilter = std::make_shared<ShellGuiCmdPreSelFilter>(
                _pSelSetHighlightor->getSelectionSet());

            _pPreview = nullptr;
        }
    }

    return;
}

void ShellGuiCmd::onEnterKey()
{
    if (Step::SelectFaces == _step)
    {
        _sels = _pSelSetHighlightor->getSelectionSet();
        if (!_sels.isEmpty())
        {
            this->finishStep(_step);
        }
    }
}

void ShellGuiCmd::onSpaceKey()
{
    this->onEnterKey();
}

bool ShellGuiCmd::isContextMenuActionVisible_CompleteSelection() const
{
    return Step::SelectFaces == _step;
}

void ShellGuiCmd::onContextMenuAction_CompleteSelection()
{
    this->onEnterKey();
}

bool ShellGuiCmd::isContextMenuActionVisible_ClearSelection() const
{
    return Step::SelectFaces == _step;
}

void ShellGuiCmd::onContextMenuAction_ClearSelection()
{
    if (Step::SelectFaces == _step)
    {
        if (_pSelSetHighlightor)
        {
            _pSelSetHighlightor->clearSelections();
        }
    }
}

bool ShellGuiCmd::createShell(const wyap::SelectionSet& sels, double thickness, unsigned int& errorCode)
{
    errorCode = 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    if (sels.isEmpty()) return false;

    // 执行抽壳的实体
    const wy3d::Solid* pConstSolid(nullptr);
    for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        pConstSolid = wy3d::Solid::cast(pDb->getElement(iter.current().getElementId()));
        break;
    }
    if (!pConstSolid) return false;
    wydb::ElementId solidId = pConstSolid->getId();

    // 根据选择集提取面
    std::vector<unsigned int> faceIndices;
    faceIndices.reserve(10);
    for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wyap::Selection& sel = iter.current();
        if (sel.getElementId() != solidId) // 在选择过滤器中已经确保了只能选择单一主体的面或边
        {
            assert(false);
            return false;
        }

        switch (static_cast<wy3d::SelectionType>(sel.getSelectionType()))
        {
        case wy3d::SelectionType::Face:
        {
            const std::string& subPath = sel.getSubPath();
            if (subPath.empty())
            {
                assert(false);
                return false;
            }
            unsigned int faceIndex = std::stoul(subPath);
            faceIndices.emplace_back(faceIndex);
        }
        break;

        default:
        {
            assert(false);
            return false;
        }
        break;
        }
    }
    assert(!faceIndices.empty());

    // 开启事务创建倒角圆角
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;
    wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(solidId));
    if (!pSolid)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    wy3d::Shell* pShell(nullptr);
    if (wy::ErrorStatus::Ok != wy3d::Shell::create(pTrans, pSolid, faceIndices, thickness, wy3d::ShellDirection::Inward, pShell))
    {
        errorCode = static_cast<unsigned int>(wy3d::ErrorCode::SHELL_CreateShellError);
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    pDb->getTransactionManager()->endTransaction();

    // 圆角已经创建成功但还需要查看有无错误码
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        pDb->getTransactionManager()->getChainUpdateFeedback(pShell->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }

    return true;
}
