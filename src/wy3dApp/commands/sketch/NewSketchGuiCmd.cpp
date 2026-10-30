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

#include "NewSketchGuiCmd.h"

#include <QCoreApplication>
#include <QTimer>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include "snap/SnapSystemBase.h"
#include <wy3dSketch.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "snap/SketchSnapSystem.h"
#include "select/SketchPlaneSelFilter.h"
#include <wy3dApp/GuiCommandUtil.h>
#include "utils/MathUtils.h"
#include <wy3dApp/SketchEnvironment.h>
#include "environments/sketch/SketchEnvironmentImpl.h"
#include <wy3dApp/ElementNodeType.h>
#include "scene/SceneImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

NewSketchGuiCmd::NewSketchGuiCmd()
    : OsgGuiCommand(), _step(Step::Undefined), _plane()
{
}

NewSketchGuiCmd::~NewSketchGuiCmd()
{
}

wyap::CmdExecution::StartResult NewSketchGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化:点选选项
    _pointPickOption.allowedElemTypes = ElementNodeType::Solid | ElementNodeType::Datum | ElementNodeType::Sheet;
    _pointPickOption.selType = wy3d::SelectionType::Face | wy3d::SelectionType::Element;
    _pointPickOption.postFilter = std::make_shared<SketchPlaneSelFilterFunctor>();

    // 初始化:步骤
    const wyap::SelectionSet& ss = ApplicationImpl::instance().getSelManager()->getSelections();
    auto extractDatumPlaneFromSelSet = [](const wyap::SelectionSet& ss) -> wydb::ElementId
    {
        if (ss.getCount() != 1) return wydb::ElementId::kNull;
        const wyap::Selection& sel = ss.createIterator().current();
        wydb::ElementId id = sel.getElementId();

        const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
        if (!pDb) return wydb::ElementId::kNull;
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(id));
        if (!pDatumPlane) return wydb::ElementId::kNull;
        return id;
    };
    wydb::ElementId id = extractDatumPlaneFromSelSet(ss);
    ApplicationImpl::instance().getSelManager()->beginChange();
    ApplicationImpl::instance().getSelManager()->clearSelections();
    ApplicationImpl::instance().getSelManager()->endChange();
    this->gotoStep(Step::SelectDatumPlaneOrFace);

    if (!id.isNull())
    {
        _preview.update(wyap::Selection(id));

        // onStart runs inside executeCommand(), where CmdManager is busy and requestEnd()
        // reports SystemBusy. Defer the confirmation to the next event loop turn, after
        // executeCommand() has returned and the command is running. The command is looked up
        // again rather than captured: until this runs it may have been aborted, and the
        // pointer would then be dangling.
        QTimer::singleShot(0, &ApplicationImpl::instance(), []()
        {
            wyap::CmdManager* pCmdMgr = ApplicationImpl::instance().getCmdManager();
            if (!pCmdMgr) return;
            NewSketchGuiCmd* pCmd = dynamic_cast<NewSketchGuiCmd*>(
                pCmdMgr->getCurrentModalCmdExecution());
            if (!pCmd) return;
            pCmd->finishStep(Step::SelectDatumPlaneOrFace);
        });
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void NewSketchGuiCmd::onEnd()
{
    GuiCommand::onEnd();

    // 结束非批次渲染
    if (SceneImpl* pScene = ApplicationImpl::instance().getActiveSceneImpl())
    {
        pScene->endNoBatchRender();
    }

    // 清空选择集
    ApplicationImpl::instance().getSelManager()->beginChange();
    ApplicationImpl::instance().getSelManager()->clearSelections();
    ApplicationImpl::instance().getSelManager()->endChange();

    // 进入草图环境
    std::unique_ptr<SketchEnvironmentImpl> pSketchEnv = std::make_unique<SketchEnvironmentImpl>(_plane);
    wy::ErrorStatus error = ApplicationImpl::instance().getEnvManager()->enterEnvironment(
        std::move(pSketchEnv),
        wyap::ExecutionMode::Async);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
    }

}
void NewSketchGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

    // 结束非批次渲染
    if (SceneImpl* pScene = ApplicationImpl::instance().getActiveSceneImpl())
    {
        pScene->endNoBatchRender();
    }

    // 清空选择集
    ApplicationImpl::instance().getSelManager()->beginChange();
    ApplicationImpl::instance().getSelManager()->clearSelections();
    ApplicationImpl::instance().getSelManager()->endChange();
}

void NewSketchGuiCmd::cleanUp()
{
    _preview.clear();
}

bool NewSketchGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        if (_preview.isNull())
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        wyap::Selection sel = _preview.getSelection();
        if (this->perform(sel))
        {
            // exit
            wy::ErrorStatus error = this->requestEnd();
            assert(wy::ErrorStatus::Ok == error);
            return true;
        }
        else
        {
            // exit
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
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

void NewSketchGuiCmd::gotoStep(Step step)
{
    _step = step;

    // 清空捕捉结果
    ApplicationImpl::instance().getSnapSystem()->clearSnapResult();

    switch (step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        // 清空选择集
        ApplicationImpl::instance().getSelManager()->beginChange();
        ApplicationImpl::instance().getSelManager()->clearSelections();
        ApplicationImpl::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        ApplicationImpl::instance().getStatusBar()->setTips(QCoreApplication::translate("NewSketch",
            "Select datum plane or planar face."));

        // 鼠标样式
        ApplicationImpl::instance().setCursor(CursorType::SelectElements);

        // 预览
        _preview.clear();
    }
    break;

    default:
    {
        ApplicationImpl::instance().getStatusBar()->setTips("");
        assert(false);
    }
    break;
    }
}

void NewSketchGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        // 点选预览
        wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
        _preview.update(sel);
    }
    break;
    }

    return;
}

void NewSketchGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        if (!_preview.isNull())
        {
            this->finishStep(_step);
        }
    }
    break;
    }

    return;
}

void NewSketchGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectDatumPlaneOrFace != _step) return;
    if (!_preview.isNull()) return;

    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(id));
    if (!pDatumPlane) return;

    _preview.update(wyap::Selection(id));
    this->finishStep(_step);
}

bool NewSketchGuiCmd::perform(const wyap::Selection& sel)
{
    return GuiCommandUtil::getWorkingPlane(sel, _plane);
}

} // namespace wy3dApp
