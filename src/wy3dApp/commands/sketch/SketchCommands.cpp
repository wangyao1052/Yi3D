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
#include <wy3dPlanarSheet.h>
#include <wy3dFilledSheet.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dRevolvedSheet.h>
#include <wy3dSweptSheet.h>
#include <wy3dLoftedSheet.h>
#include <wy3dSketchProfileForSheet.h>
#include "translation/ErrorCodeTranslation.h"
#include <wy3dSweep.h>
#include <wy3dLoft.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dHelix.h>
#include <wy3dSplitFace.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "widgets/sketch/SketchPlaneDialog.h"
#include <wy3dApp/SketchEnvironment.h>
#include "environments/sketch/SketchEnvironmentImpl.h"
#include "utils/SketchUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

int EditSketchCommand::run()
{
    // 当前正在草图环境
    wyap::Environment* pCurrentEnv = ApplicationImpl::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironment* pCurrSketchEnv = dynamic_cast<SketchEnvironment*>(pCurrentEnv);
    if (pCurrSketchEnv)
    {
        assert(false);
        return -1;
    }

    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    const wyap::SelectionSet& ss = ApplicationImpl::instance().getSelManager()->getSelections();
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

    std::unique_ptr<SketchEnvironmentImpl> sketchEnv = std::make_unique<SketchEnvironmentImpl>(pSketch);
    wy::ErrorStatus error = ApplicationImpl::instance().getEnvManager()->enterEnvironment(
        std::move(sketchEnv),
        wyap::ExecutionMode::Async);
    if (wy::ErrorStatus::Ok != error)
    {
        assert(false);
        return -1;
    }

    return 0;
}


int EndSketchCommand::run()
{
    wyap::Environment* pCurrentEnv =
        ApplicationImpl::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pCurrentEnv);
    if (!pSketchEnv)
    {
        assert(false);
        return -1;
    }

    pSketchEnv->onConfirm();
    return 0;
}


int CancelSketchCommand::run()
{
    wyap::Environment* pCurrentEnv =
        ApplicationImpl::instance().getEnvManager()->getActiveEnvironment();
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pCurrentEnv);
    if (!pSketchEnv)
    {
        assert(false);
        return -1;
    }

    pSketchEnv->onCancel();
    return 0;
}

} // namespace wy3dApp
