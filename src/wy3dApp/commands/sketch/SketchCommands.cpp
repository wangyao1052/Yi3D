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

#include "SketchCommands.h"
#include <string>
#include <cassert>
#include <memory>
#include <QCoreApplication>
#include <QMessageBox>
#include <wyapSelManager.h>
#include <wyapSelection.h>
#include <wyapEnvManager.h>
#include <wy3dExtrusion.h>
#include <wy3dRevolution.h>
#include <wy3dSweep.h>
#include <wy3dLoft.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dHelix.h>

#include "application/Application.h"
#include "widgets/sketch/SketchPlaneDialog.h"
#include "environments/sketch/SketchEnvironment.h"
#include "utils/SketchUtil.h"



int EditSketchCommand::run()
{
    // 当前正在草图环境
    wyap::Environment* pCurrentEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironment* pCurrSketchEnv = dynamic_cast<SketchEnvironment*>(pCurrentEnv);
    if (pCurrSketchEnv)
    {
        assert(false);
        return -1;
    }

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (!pDb || ss.getCount() != 1)
    {
        assert(false);
        return -1;
    }
    wydb::ElementId id = ss.createIterator().current().getElementId();
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(id));
    if (!pSketch)
    {
        assert(false);
        return -1;
    }

    std::unique_ptr<SketchEnvironment> sketchEnv = std::make_unique<SketchEnvironment>(pSketch);
    wy::ErrorStatus error = Application::instance().getEnvManager()->enterEnvironment(
        std::move(sketchEnv),
        wyap::ExecutionMode::Async);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        return -1;
    }

    return 0;
}



static bool canEndEditingSketch(const wydb::ElementId& sketchId)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return true; // 也不需要提示了
    }
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
    if (!pSketch)
    {
        assert(false);
        return true; // 也不需要提示了
    }

    // 草图Owner为空则是自由草图可以任意绘制
    wydb::ElementId sketchOwnerId = pSketch->getParent();
    if (sketchOwnerId.isNull())
    {
        return true;
    }

    // 草图Owner
    const wydb::Element* pSketchOwner = pDb->getElement(sketchOwnerId);
    if (!pSketchOwner)
    {
        assert(false);
        return true; // 也不需要提示了
    }

    // 拉伸体
    QString error;
    if (const wy3d::Extrusion* pExtrusion = wy3d::Extrusion::cast(pSketchOwner))
    {
        if (SketchUtil::isValidExtrusionProfile(*pSketch, error))
        {
            return true;
        }
    }
    // 旋转体（轴可能来自其他草图，只校验 profile，不自动更新轴）
    else if (const wy3d::Revolution* pConstRevolution = wy3d::Revolution::cast(pSketchOwner))
    {
        if (SketchUtil::isValidExtrusionProfile(*pSketch, error))
        {
            return true;
        }
    }
    // 扫描体
    else if (const wy3d::Sweep* pConstSweep = wy3d::Sweep::cast(pSketchOwner))
    {
        // 路径
        if (pConstSweep->getPath() == sketchId)
        {
            if (SketchUtil::isValidSweepPath(*pSketch, error))
            {
                return true;
            }
        }
        // 轮廓
        else
        {
            if (SketchUtil::isValidSweepProfile(*pSketch, error))
            {
                return true;
            }
        }
    }
    // 放样体
    else if (const wy3d::Loft* pConstLoft = wy3d::Loft::cast(pSketchOwner))
    {
        if (SketchUtil::isValidLoftProfile(*pSketch, error))
        {
            return true;
        }
    }
    // 螺旋线
    else if (const wy3d::Helix* pHelix = wy3d::Helix::cast(pSketchOwner))
    {
        if (SketchUtil::isValidHelixProfile(*pSketch, error))
        {
            return true;
        }
    }
    else
    {
        assert(false);
        return true;
    }

    if (!error.isEmpty())
    {
        QMessageBox::warning(nullptr, QCoreApplication::translate("SketchCommand", "Yi3D"), error);
    }
    return false;
}

int EndSketchCommand::run()
{
    wyap::Environment* pCurrentEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pCurrentEnv);
    if (!pSketchEnv)
    {
        assert(false);
        return -1;
    }

    int* pA = new int(5);
    std::unique_ptr<int> pp(pA);

    // 编辑草图
    if (pSketchEnv->getOperation() == SketchEnvironment::Operation::Edit)
    {
        wydb::ElementId sketchId = pSketchEnv->getSketchId();
        if (!canEndEditingSketch(sketchId))
        {
            return 0;
        }
    }

    // 退出草图环境
    if (wy::ErrorStatus::Ok != Application::instance().getEnvManager()->exitActiveEnvironment(
        wyap::Environment::ExitCode::Ok,
        wyap::ExecutionMode::Async))
    {
        assert(false);
        return -1;
    }

    return 0;
}


int CancelSketchCommand::run()
{
    wyap::Environment* pCurrentEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pCurrentEnv);
    if (!pSketchEnv)
    {
        assert(false);
        return -1;
    }

    // 退出草图环境
    if (wy::ErrorStatus::Ok != Application::instance().getEnvManager()->exitActiveEnvironment(
        wyap::Environment::ExitCode::Cancel,
        wyap::ExecutionMode::Async))
    {
        assert(false);
        return -1;
    }

    return 0;
}