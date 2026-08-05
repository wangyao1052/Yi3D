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

#include "commands/modeling/solid/primitives/MakePrimitiveGuiCmd.h"

#include <osg/LineSegment>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dImpl.h>
#include <wy3dDatumPlane.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "utils/MathUtils.h"
#include "utils/TopoShapeUtil.h"

#include "application/Application.h"
#include "snap/SnapObject.h"
#include "snap/SketchSnapSystem.h"
#include "select/SketchPlaneSelFilter.h"
#include "utils/MathUtils.h"
#include "utils/TopoShapeUtil.h"
#include "environments/sketch/SketchEnvironment.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/Scene.h"
#include "view/ViewUtil.h"
#include "view/OsgView.h"
#include "commands/CommandNames.h"


bool _getWorkingPlane(const wyap::Selection& sel, wy3d::SketchPlane& workPln)
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
            // commented by wangyao 2025.08.20 {
            // 比如在特征树上选中长方体,再执行绘制圆柱体命令.
            // assert(false);
            // }
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

MakePrimitiveGuiCmd::MakePrimitiveGuiCmd() : OsgGuiCommand(),
    _step(0), _pickOption(), _workPln(), _pWorkPlnPreview(nullptr)
{
    // 禁止点选和框选
    _options.pointSelect = false;
    _options.boxSelect = false;
}

MakePrimitiveGuiCmd::~MakePrimitiveGuiCmd()
{
}

wyap::CmdExecution::StartResult MakePrimitiveGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 点选选项
    // 选择工作平面:基准面or实体面
    _pickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid | ElementNodeType::DatumPlane);
    _pickOption.selType = wy3d::SelectionType::SolidFace;
    _pickOption.pSelFilter = std::make_shared<SketchPlaneSelFilterFunctor>();

    // 初始化
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();   
    auto isValidSS = [](const wyap::SelectionSet& ss) -> bool
    {
        if (ss.getCount() != 1)
        {
            return false;
        }
        const wyap::Selection& sel = ss.createIterator().current();
        wy3d::SketchPlane workPln;
        if (_getWorkingPlane(sel, workPln))
        {
            return true;
        }
        else
        {
            return false;
        }
    };
    if (ss.getCount() == 1 && isValidSS(ss))
    {
        _pWorkPlnPreview = std::make_shared<SelectPreview>(ss.createIterator().current());
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->finishStep(Step::SpecifyWorkingPlane);
    }
    else
    {
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();
        this->gotoStep(Step::SpecifyWorkingPlane);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}
void MakePrimitiveGuiCmd::onEnd()
{
    GuiCommand::onEnd();

    // 隐藏工作平面坐标系
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->hideSketchCSYS();
    }
    else
    {
        assert(false);
    }

    // 移除工作平面原点捕捉对象
    if (_pWorkPlnOriginSnapObject)
    {
        wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
        pSnapSys->beginChange();
        {
            pSnapSys->removeResidentSnapObject(_pWorkPlnOriginSnapObject);
        }
        pSnapSys->endChange();
    }

}
void MakePrimitiveGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    GuiCommand::onAbort(cause);

    // 隐藏工作平面坐标系
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->hideSketchCSYS();
    }
    else
    {
        assert(false);
    }

    // 移除工作平面原点捕捉对象
    if (_pWorkPlnOriginSnapObject)
    {
        wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
        pSnapSys->beginChange();
        {
            pSnapSys->removeResidentSnapObject(_pWorkPlnOriginSnapObject);
        }
        pSnapSys->endChange();
    }

}

void MakePrimitiveGuiCmd::reset()
{
    _step = 0;
    _excludeIds.clear();
    _workPln = wy3d::SketchPlane();
    _pWorkPlnPreview = nullptr;
}

bool MakePrimitiveGuiCmd::finishStep(unsigned int step)
{
    if (1 == step)
    {
        if (!_pWorkPlnPreview)
        {
            return false;
        }
        if (!_getWorkingPlane(_pWorkPlnPreview->getSelection(), _workPln))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pWorkPlnPreview = nullptr;

        // 添加工作平面坐标原点为常驻捕捉对象
        wyap::SnapSystem* pSnapSys = Application::instance().getSnapSystem();
        pSnapSys->beginChange();
        {
            _pWorkPlnOriginSnapObject = std::make_shared<SnapCoordinatePoint>(_workPln.getOrigin());
            pSnapSys->addResidentSnapObject(_pWorkPlnOriginSnapObject);
        }
        pSnapSys->endChange();

        // 显示工作平面坐标系
        if (Scene* pScene = Application::instance().getActiveScene())
        {
            pScene->showSketchCSYS(_workPln);
        }
        else
        {
            assert(false);
        }

        // 下一步
        this->gotoStep(++step);
        return true;
    }
    else
    {
        assert(false);
        return false;
    }
}

void MakePrimitiveGuiCmd::gotoStep(unsigned int step)
{
    _step = step;
    this->gotoStepImpl(step);

}

void MakePrimitiveGuiCmd::gotoStepImpl(unsigned int step)
{
    if (1 == step)
    {
        // 禁用文本输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MakePrimitiveGuiCmd",
            "Select datum plane or solid surface as working plane."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
    }
    else
    {
        assert(false);
    }
}

void MakePrimitiveGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (1 == _step)
    {
        wyap::Selection sel = this->pointPick(event.x, event.y, _pickOption);
        if (_pWorkPlnPreview)
        {
            if (sel.getElementId().isNull())
            {
                _pWorkPlnPreview = nullptr;
            }
            else
            {
                if (!_pWorkPlnPreview->isEqual(sel))
                {
                    _pWorkPlnPreview = std::make_shared<SelectPreview>(sel);
                }
            }
        }
        else
        {
            if (!sel.getElementId().isNull())
            {
                _pWorkPlnPreview = std::make_shared<SelectPreview>(sel);
            }
        }

        return;
    }
    else
    {
        assert(false);
        return;
    }
}

void MakePrimitiveGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    if (1 == _step)
    {
        if (_pWorkPlnPreview)
        {
            this->finishStep(_step);
        }

        return;
    }
    else
    {
        assert(false);
        return;
    }
}

GuiCmdMenu* MakePrimitiveGuiCmd::initContextMenu()
{
    return new MakePrimitiveGuiCmdMenu(this);
}

void MakePrimitiveGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (1 != _step) return;
    if (_pWorkPlnPreview) return;

    const wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return;
    const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(id));
    if (!pDatumPlane) return;
    _pWorkPlnPreview = std::make_shared<SelectPreview>(wyap::Selection(id));
    this->finishStep(_step);
}

bool MakePrimitiveGuiCmdMenu::initCustomHeaderActions(QMenu* menu)
{
    assert(menu);
    assert(_pCmd);
    MakePrimitiveGuiCmd* pCmd = dynamic_cast<MakePrimitiveGuiCmd*>(_pCmd);
    if (!pCmd)
    {
        assert(false);
        return false;
    }

    if (pCmd->getStep() > 1)
    {
        // 正视于工作平面
        QAction* pActionNormalToWorkPln = new QAction(tr("View Normal To Working Plane"), menu);
        pActionNormalToWorkPln->setIcon(QIcon(":/images/View_Normal.svg"));
        menu->addAction(pActionNormalToWorkPln);
        this->connect(pActionNormalToWorkPln, &QAction::triggered, this, &MakePrimitiveGuiCmdMenu::onViewNormalToWorkingPlane);
        return true;
    }

    return false;
}

void MakePrimitiveGuiCmdMenu::onViewNormalToWorkingPlane()
{
    assert(_pCmd);
    MakePrimitiveGuiCmd* pCmd = dynamic_cast<MakePrimitiveGuiCmd*>(_pCmd);
    if (!pCmd)
    {
        assert(false);
        return;
    }

    BaseView* pView = Application::instance().getActiveView();
    if (!pView)
    {
        assert(false);
        return;
    }
    pView->viewToWorkingPlane(pCmd->getWorkingPlane());
}
