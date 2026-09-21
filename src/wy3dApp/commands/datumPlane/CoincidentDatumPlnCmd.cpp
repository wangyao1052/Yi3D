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

#include "commands/datumPlane/CoincidentDatumPlnCmd.h"
#include "application/Application.h"
#include "select/SketchPlaneSelFilter.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"


CoincidentDatumPlnCmd::CoincidentDatumPlnCmd()
    : OsgGuiCommand(), _step(Step::Undefined), _plane()
{
    // 禁止点选和框选
    _options.pointSelect = false;
    _options.boxSelect = false;
}

CoincidentDatumPlnCmd::~CoincidentDatumPlnCmd()
{
}

wyap::CmdExecution::StartResult CoincidentDatumPlnCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::DatumPlane);
    _pointPickOption.selType = wy3d::SelectionType::Face;
    _pointPickOption.pSelFilter = std::make_shared<SketchPlaneSelFilterFunctor>();
    this->gotoStep(Step::SelectDatumPlaneOrFace);

    return wyap::CmdExecution::StartResult::Succeeded;
}
void CoincidentDatumPlnCmd::onEnd()
{
    GuiCommand::onEnd();

}
void CoincidentDatumPlnCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

}

bool CoincidentDatumPlnCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        _pMakeDatumPlane = std::make_shared<MakeDatumPlane>(this);
        if (!_pMakeDatumPlane->create(_plane))
        {
            assert(false);
            _pMakeDatumPlane = nullptr;
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMakeDatumPlane->commit();
        _pMakeDatumPlane = nullptr;

        // exit
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

void CoincidentDatumPlnCmd::gotoStep(Step step)
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
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Select datum plane or solid plane surface."));

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

void CoincidentDatumPlnCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        // 点选预览
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pPreview);
        return;
    }
    break;
    }

    return;
}

void CoincidentDatumPlnCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectDatumPlaneOrFace:
    {
        if (_pPreview && !_pPreview->getSelection().getElementId().isNull())
        {
            wyap::Selection sel = _pPreview->getSelection();
            _pPreview = nullptr;

            // 获取平面
            if (!MakeDatumPlane::getSketchPlane(sel, _plane))
            {
                assert(false);
                return;
            }

            // finish step
            this->finishStep(_step);
            return;
        }
    }
    break;
    }

    return;
}