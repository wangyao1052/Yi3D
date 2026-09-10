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
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>
#include "snap/SnapSystemBase.h"
#include <wy3dSketch.h>

#include "application/Application.h"
#include "snap/SketchSnapSystem.h"
#include "select/SketchPlaneSelFilter.h"
#include "utils/GuiCommandUtil.h"
#include "utils/MathUtils.h"
#include "environments/sketch/SketchEnvironment.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/Scene.h"


NewSketchGuiCmd::NewSketchGuiCmd()
    : OsgGuiCommand(), _step(Step::Undefined), _plane()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

NewSketchGuiCmd::~NewSketchGuiCmd()
{
}

wyap::CmdExecution::StartResult NewSketchGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化:点选选项
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::DatumPlane | ElementNodeType::Sheet);
    _pointPickOption.selType = wy3d::SelectionType::SolidFace;
    _pointPickOption.pSelFilter = std::make_shared<SketchPlaneSelFilterFunctor>();

    // 初始化:步骤
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    auto extractDatumPlaneFromSelSet = [](const wyap::SelectionSet& ss) -> wydb::ElementId
    {
        if (ss.getCount() != 1) return wydb::ElementId::kNull;
        const wyap::Selection& sel = ss.createIterator().current();
        wydb::ElementId id = sel.getElementId();

        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return wydb::ElementId::kNull;
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(id));
        if (!pDatumPlane) return wydb::ElementId::kNull;
        return id;
    };
    wydb::ElementId id = extractDatumPlaneFromSelSet(ss);
    this->clearSelections();
    if (!id.isNull())
    {
        _pPreview = std::make_shared<SelectPreview>(wyap::Selection(id));
        this->finishStep(Step::SelectDatumPlaneOrFace);
    }
    else
    {
        this->gotoStep(Step::SelectDatumPlaneOrFace);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void NewSketchGuiCmd::onEnd()
{
    GuiCommand::onEnd();

    // 结束非批次渲染
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->endNoBatchRender();
    }

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 进入草图环境
    std::unique_ptr<SketchEnvironment> pSketchEnv = std::make_unique<SketchEnvironment>(_plane);
    wy::ErrorStatus error = Application::instance().getEnvManager()->enterEnvironment(
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
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->endNoBatchRender();
    }

    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
}

void NewSketchGuiCmd::cleanup()
{
    _pPreview = nullptr;
}

bool NewSketchGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        if (!_pPreview)
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

        wyap::Selection sel = _pPreview->getSelection();
        if (this->perform(sel))
        {
            /*
            // 进入草图环境
            std::unique_ptr<SketchEnvironment> pSketchEnv = std::make_unique<SketchEnvironment>(_plane);
            // added by wangyao 2025.04.24 {
            // 当选择的是参照面时对齐草图
            //if (sel.getSelectionType() == static_cast<unsigned int>(wy3d::SelectionType::Element))
            //{
            //    pSketchEnv->setWhetherOrientToSketch(true);
            //}
            // }
            wy::ErrorStatus error = Application::instance().getEnvManager()->enterEnvironment(std::move(pSketchEnv));
            if (wy::ErrorStatus::Ok != error)
            {
                assert(false);
            }
            */

            // exit
            this->requestEnd();
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
    Application::instance().getSnapSystem()->clearSnapResult();

    switch (step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("NewSketch",
            "Select datum plane or planar face."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pPreview = nullptr;
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
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
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
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
        if (_pPreview)
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
    if (_pPreview) return;

    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(id));
    if (!pDatumPlane) return;

    _pPreview = std::make_shared<SelectPreview>(wyap::Selection(id));
    this->finishStep(_step);
}

bool NewSketchGuiCmd::perform(const wyap::Selection& sel)
{
    return GuiCommandUtil::getWorkingPlane(sel, _plane);
}