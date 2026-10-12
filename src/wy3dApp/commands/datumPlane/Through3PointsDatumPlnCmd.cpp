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

#include "commands/datumPlane/Through3PointsDatumPlnCmd.h"
#include <QToolTip>
#include <wyVector3.h>
#include <wyapSelManager.h>
#include <wy3dApp/Application.h>
#include "snap/SnapContext.h"
#include "scene/Colors.h"
#include "snap/SnapConsts.h"
#include "utils/MessageBoxUtil.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

Through3PointsDatumPlnCmd::Through3PointsDatumPlnCmd()
    : OsgGuiCommand(), _step(Step::Undefined), _plane(), _pnt1st(), _pnt2nd(), _pnt3rd()
{
    // 禁止点选和框选
    _options.pointSelect = false;
    _options.boxSelect = false;
}

Through3PointsDatumPlnCmd::~Through3PointsDatumPlnCmd()
{
}

wyap::CmdExecution::StartResult Through3PointsDatumPlnCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 初始化
    this->gotoStep(Step::SpecifyPoint1st);

    return wyap::CmdExecution::StartResult::Succeeded;
}

bool Through3PointsDatumPlnCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyPoint1st:
    {
        this->gotoStep(Step::SpecifyPoint2nd);
        return true;
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        this->gotoStep(Step::SpecifyPoint3rd);
        return true;
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        // 创建基准面
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

void Through3PointsDatumPlnCmd::gotoStep(Step step)
{
    _step = step;

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();

    switch (step)
    {
    case Step::SpecifyPoint1st:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Specify the first point."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Specify the second point."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("DatumPlnCmd",
            "Specify the third point."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Locate);
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

void Through3PointsDatumPlnCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SpecifyPoint1st:
    case Step::SpecifyPoint2nd:
    case Step::SpecifyPoint3rd:
    {
        wyap::SnapResultSPtr pSnapResult = Application::instance().getSnapSystem()->snap(event.x, event.y,
            std::make_shared<LocateContext>(), _snapExcludeIds);
        return;
    }
    break;

    default:
    {
        return;
    }
    break;
    }

    return;
}

void Through3PointsDatumPlnCmd::onLeftMouseDown(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SpecifyPoint1st:
    case Step::SpecifyPoint2nd:
    case Step::SpecifyPoint3rd:
    {
        wyap::SnapResultSPtr pSnapResult = Application::instance().getSnapSystem()->snap(event.x, event.y,
            std::make_shared<LocateContext>(), _snapExcludeIds);
        if (pSnapResult)
        {
            QString error;
            this->setPoint(pSnapResult->getPosition(), error);
        }
        return;
    }
    break;

    default:
    {
        return;
    }
    break;
    }

    return;
}

bool Through3PointsDatumPlnCmd::setPoint(const wy::Vector3& pnt, QString& error)
{
    switch (_step)
    {
    case Step::SpecifyPoint1st:
    {
        _pnt1st = pnt;
        _pPntTransient1st = std::make_shared<PointTransient>(_pnt1st,
            Colors::kEdge_Highlight, SnapConsts::PickSize + 1);
        return this->finishStep(_step);
    }
    break;

    case Step::SpecifyPoint2nd:
    {
        _pnt2nd = pnt;
        if ((_pnt2nd - _pnt1st).length() <= wy3d::TOL)
        {
            return false;
        }
        _pPntTransient2nd = std::make_shared<PointTransient>(_pnt2nd,
            Colors::kEdge_Highlight, SnapConsts::PickSize + 1);
        return this->finishStep(_step);
    }
    break;

    case Step::SpecifyPoint3rd:
    {
        _pnt3rd = pnt;
        // 第三点与第一或二点重合
        if ((_pnt3rd - _pnt1st).length() <= wy3d::TOL || (_pnt3rd - _pnt2nd).length() <= wy3d::TOL)
        {
            error = QCoreApplication::translate("DatumPlnCmd", "The three points are collinear and cannot define a unique plane.");
            return false;
        }
        // 三点共线
        if ((_pnt3rd - _pnt1st).cross(_pnt3rd - _pnt2nd).length() <= wy3d::TOL)
        {
            error = QCoreApplication::translate("DatumPlnCmd", "The three points are collinear and cannot define a unique plane.");
            return false;
        }
        // 通过三点计算平面
        if (!MakeDatumPlane::computeThrough3PointsPlane(_pnt1st, _pnt2nd, _pnt3rd, _plane))
        {
            error = QCoreApplication::translate("DatumPlnCmd", "The three points are collinear and cannot define a unique plane.");
            return false;
        }
        _pPntTransient3rd = std::make_shared<PointTransient>(_pnt3rd,
            Colors::kEdge_Highlight, SnapConsts::PickSize + 1);
        return this->finishStep(_step);
    }
    break;

    default:
    {
        assert(false);
        return false;
    }
    break;
    }

    return false;
}

}
