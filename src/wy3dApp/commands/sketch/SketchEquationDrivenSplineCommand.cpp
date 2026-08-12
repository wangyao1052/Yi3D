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

#include "commands/sketch/SketchEquationDrivenSplineCommand.h"
#include <muParser.h>
#include <wyVector2.h>
#include <wyapEnvironment.h>
#include <wy3dSketchSpline.h>
#include <wy3dSketchSpline.h>
#include "application/Application.h"
#include "environments/sketch/SketchEnvironment.h"
#include "commands/dialogs/EquationDrivenSplineDialog.h"

int SketchEquationDrivenSplineCommand::run()
{
    // 环境信息
    wyap::Environment* pEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    if (SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pEnv))
    {
        _sketchPlane = pSketchEnv->getSketchPlane();
        _sketchId = pSketchEnv->getSketchId();
    }
    else
    {
        assert(false);
        return 0;
    }

    // 参数驱动的样条曲线对话框
    EquationDrivenSplineDialog dlg;
    if (QDialog::Accepted != dlg.exec())
    {
        return 0;
    }
    const std::vector<wy::Vector2>& points = dlg.getPoints();
    if (points.empty())
    {
        assert(false);
        return 0;
    }

    // 创建样条曲线
    this->makeSpline(_sketchId, points);
    return 0;
}

bool SketchEquationDrivenSplineCommand::makeSpline(
    const wydb::ElementId& sketchId,
    const std::vector<wy::Vector2>& fitPoints)
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return false;
    }

    wydb::TransactionOption option;
    option.chainUpdateScope = wydb::ChainUpdateScope::Local;
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction("", option);
    if (!pTrans)
    {
        assert(false);
        return false;
    }

    wy3d::SketchSpline* pSketchSpline(nullptr);
    wy3d::Sketch* pSketch = wy3d::Sketch::cast(pTrans->getElementForWrite(sketchId));
    if (!pSketch) goto ABORT_TRANS;
    if (wy::ErrorStatus::Ok != wy3d::SketchSpline::create(pTrans, fitPoints, pSketchSpline) || !pSketchSpline)
    {
        goto ABORT_TRANS;
    }
    if (wy::ErrorStatus::Ok != pSketch->addEntity(pSketchSpline))
    {
        goto ABORT_TRANS;
    }
    pDb->getTransactionManager()->endTransaction();
    return true;

ABORT_TRANS:
    assert(false);
    pDb->getTransactionManager()->abortTransaction();
    pSketchSpline = nullptr;
    return false;
}