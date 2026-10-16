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

#include "commands/sketch/SketchRelocateCsysGuiCmd.h"
#include <wyVector2.h>
#include <wyVector3.h>
#include <wyapSelManager.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchEntity.h>
#include <wy3dApp/Application.h>
#include "snap/SketchSnapSystem.h"
#include <wy3dApp/Scene.h>
#include <wy3dApp/SketchEnvironment.h>
#include "environments/sketch/SketchEnvironmentImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchRelocateCsysGuiCmd::SketchRelocateCsysGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _origin(), _angle(0.0)
{
}

SketchRelocateCsysGuiCmd::~SketchRelocateCsysGuiCmd()
{
}

wyap::CmdExecution::StartResult SketchRelocateCsysGuiCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = GuiCommand::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);
 
    _sketchInfo = GuiCommandUtil::initSketchInfo();
    if (_sketchInfo.pSketchSnapSys) _sketchInfo.pSketchSnapSys->clearSnapResult();

    // 更新草图捕捉系统
    if (_sketchInfo.pSketchSnapSys)
    {
        _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
    }

    // 鼠标样式
    Application::instance().setCursor(CursorType::Locate);

    // 坐标系
    _pCsysTransient = std::make_shared<SketchCsysTransient>(_sketchInfo.sketchPlane);
    _pCsysTransient->show();

    // 初始化
    this->gotoStep(Step::SpecifyOrigin);

    return wyap::CmdExecution::StartResult::Succeeded;
}

bool SketchRelocateCsysGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SpecifyOrigin:
    {
        // 刷新临时坐标系显示
        _pCsysTransient->update(_origin, 0.0);

        // next step
        this->gotoStep(Step::SpecifyRotationAngle);
        return true;
    }
    break;

    case Step::SpecifyRotationAngle:
    {
        // 重定位坐标系
        if (!this->relocateSketchCsys(_sketchInfo.sketchPlane, _origin, _angle))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }

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

void SketchRelocateCsysGuiCmd::gotoStep(Step step)
{
    _step = step;

    // 清空捕捉结果
    Application::instance().getSnapSystem()->clearSnapResult();

    switch (step)
    {
    case Step::SpecifyOrigin:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 允许输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchRelocateCsysGuiCmd",
            "Specify the origin point."));

        // 捕捉上下文
        _pSnapContext = std::make_shared<Locate2dContext>();
    }
    break;

    case Step::SpecifyRotationAngle:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 允许输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("SketchRelocateCsysGuiCmd",
            "Specify the X axis."));

        // 捕捉上下文
        _pSnapContext = std::make_shared<SketchDrawLineContext>(_origin);
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

void SketchRelocateCsysGuiCmd::onMouseMove(const MouseEvent& event)
{
    if (_step == Step::SpecifyOrigin)
    {
        wy::Vector2 origin = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _pCsysTransient->update(origin, 0.0);
        return;
    }
    else if (_step == Step::SpecifyRotationAngle)
    {
        wy::Vector2 pos = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        double angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, pos - _origin);
        _pCsysTransient->update(_origin, angle);
        return;
    }
    else
    {
        assert(false);
        return;
    }
}

void SketchRelocateCsysGuiCmd::onLeftMouseDown(const MouseEvent& event)
{
    if (_step == Step::SpecifyOrigin)
    {
        _origin = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        this->finishStep(_step);
        return;
    }
    else if (_step == Step::SpecifyRotationAngle)
    {
        wy::Vector2 position = this->computePosition2d(event.x, event.y, _sketchInfo.sketchPlane, {}, _pSnapContext, _sketchInfo.pSketchSnapSys);
        _angle = wy::Vector2::rotationAngle(wy::Vector2::kXAxis, position - _origin);
        this->finishStep(_step);
        return;
    }
    else
    {
        assert(false);
        return;
    }
}

bool SketchRelocateCsysGuiCmd::relocateSketchCsys(const wy3d::Plane& plane, const wy::Vector2& origin2d, double angle)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }
    const wy3d::Sketch* pConstSketch = wy3d::Sketch::cast(pDb->getElement(_sketchInfo.sketchId));
    if (!pConstSketch)
    {
        assert(false);
        return false;
    }

    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return false;
    }
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(_sketchInfo.sketchId));
    if (!pSketch)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    // 设置新的草图平面
    wy::Vector3 origin = plane.value(origin2d);
    wy::Vector3 xDir = plane.value(std::cos(angle), std::sin(angle)) - plane.getOrigin();
    xDir.normalize();
    wy3d::Plane newPlane(origin, plane.getNormal(), xDir);
    if (!newPlane.isValid())
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    if (wy::ErrorStatus::Ok != pSketch->setPlane(newPlane))
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    // 调整草图中已有图元的方位以保持在世界坐标系中的位置不变
    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(
            pTrans->getElementForWrite(iter.current()));
        if (!pSketchEntity)
        {
            assert(false);
            continue;
        }        error = pSketchEntity->translate(-origin2d);
        assert(wy::ErrorStatus::Ok == error);
        error = pSketchEntity->rotateAround(wy::Vector2::kZero, -angle);
        assert(wy::ErrorStatus::Ok == error);
    }

    // 提交事务
    pDb->getTransactionManager()->endTransaction();
    
    // 刷新草图坐标系的显示
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->showSketchCSYS(newPlane);
    }

    // 刷新草图环境中的草图平面
    wyap::Environment* pCurEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironmentImpl* pSketchEnv = dynamic_cast<SketchEnvironmentImpl*>(pCurEnv);
    if (pSketchEnv)
    {
        pSketchEnv->setSketchPlane(newPlane);
    }
    else
    {
        assert(false);
    }

    // 刷新当前命令的草图平面信息
    _sketchInfo.sketchPlane = newPlane;

    // 更新草图捕捉系统
    if (_sketchInfo.pSketchSnapSys)
    {
        _sketchInfo.pSketchSnapSys->partiallyUpdate(Application::instance().getActiveDatabase());
    }

    return true;
}

} // namespace wy3dApp
