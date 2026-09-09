///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "Sketch3DCommands.h"

#include <cassert>
#include <memory>

#include <wyapSelManager.h>
#include <wyapSelection.h>
#include <wyapEnvManager.h>
#include <wy3dSketch3D.h>

#include "application/Application.h"
#include "environments/sketch3d/Sketch3DEnvironment.h"

int NewSketch3DCommand::run()
{
    // 当前正在3D草图环境
    wyap::Environment* pCurrentEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    Sketch3DEnvironment* pCurrSketch3DEnv = dynamic_cast<Sketch3DEnvironment*>(pCurrentEnv);
    if (pCurrSketch3DEnv)
    {
        assert(false);
        return -1;
    }

    std::unique_ptr<Sketch3DEnvironment> sketch3DEnv = std::make_unique<Sketch3DEnvironment>();
    wy::ErrorStatus error = Application::instance().getEnvManager()->enterEnvironment(
        std::move(sketch3DEnv),
        wyap::ExecutionMode::Async);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        return -1;
    }

    return 0;
}

int EditSketch3DCommand::run()
{
    // 当前正在3D草图环境
    wyap::Environment* pCurrentEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    Sketch3DEnvironment* pCurrSketch3DEnv = dynamic_cast<Sketch3DEnvironment*>(pCurrentEnv);
    if (pCurrSketch3DEnv)
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
    const wy3d::Sketch3D* pSketch3D = wy3d::Sketch3D::cast(pDb->getElement(id));
    if (!pSketch3D)
    {
        assert(false);
        return -1;
    }

    std::unique_ptr<Sketch3DEnvironment> sketch3DEnv = std::make_unique<Sketch3DEnvironment>(pSketch3D);
    wy::ErrorStatus error = Application::instance().getEnvManager()->enterEnvironment(
        std::move(sketch3DEnv),
        wyap::ExecutionMode::Async);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        return -1;
    }

    return 0;
}

int EndSketch3DCommand::run()
{
    wyap::Environment* pCurrentEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    Sketch3DEnvironment* pSketch3DEnv = dynamic_cast<Sketch3DEnvironment*>(pCurrentEnv);
    if (!pSketch3DEnv)
    {
        assert(false);
        return -1;
    }

    // 退出3D草图环境
    if (wy::ErrorStatus::Ok != Application::instance().getEnvManager()->exitActiveEnvironment(
        wyap::Environment::ExitCode::Ok,
        wyap::ExecutionMode::Async))
    {
        assert(false);
        return -1;
    }

    return 0;
}

int CancelSketch3DCommand::run()
{
    wyap::Environment* pCurrentEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    Sketch3DEnvironment* pSketch3DEnv = dynamic_cast<Sketch3DEnvironment*>(pCurrentEnv);
    if (!pSketch3DEnv)
    {
        assert(false);
        return -1;
    }

    // 退出3D草图环境
    if (wy::ErrorStatus::Ok != Application::instance().getEnvManager()->exitActiveEnvironment(
        wyap::Environment::ExitCode::Cancel,
        wyap::ExecutionMode::Async))
    {
        assert(false);
        return -1;
    }

    return 0;
}
