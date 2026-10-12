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

#ifndef WY3DAPP_THROUGH_3_POINTS_DATUM_PLN_CMD_H
#define WY3DAPP_THROUGH_3_POINTS_DATUM_PLN_CMD_H

#include <wyVector3.h>
#include <wy3dPlane.h>
#include <wy3dDatumPlane.h>
#include <wy3dApp/OsgGuiCommand.h>
#include "commands/datumPlane/MakeDatumPlane.h"
#include "commands/transient/BasicTransient.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class Through3PointsDatumPlnCmd : public OsgGuiCommand
{
    WYRX_DECLARE_MEMBERS(Through3PointsDatumPlnCmd, wy3dApp::Through3PointsDatumPlnCmd, OsgGuiCommand)
public:
    Through3PointsDatumPlnCmd();
    ~Through3PointsDatumPlnCmd();

protected:
    virtual wyap::CmdExecution::StartResult onStart() override;

protected:
    enum class Step
    {
        Undefined = 0,
        SpecifyPoint1st = 1,
        SpecifyPoint2nd = 2,
        SpecifyPoint3rd = 3,
    };
    bool finishStep(Step step);
    void gotoStep(Step step);

    virtual void onMouseMove(const MouseEvent& event) override;
    virtual void onLeftMouseDown(const MouseEvent& event) override;

private:
    bool setPoint(const wy::Vector3& pnt, QString& error);

protected:
    Step _step;
    wy3d::Plane _plane;
    wy::Vector3 _pnt1st;
    wy::Vector3 _pnt2nd;
    wy::Vector3 _pnt3rd;

    // 点临时显示对象
    PointTransientSPtr _pPntTransient1st;
    PointTransientSPtr _pPntTransient2nd;
    PointTransientSPtr _pPntTransient3rd;

    // 创建基准面
    std::shared_ptr<MakeDatumPlane> _pMakeDatumPlane;
};

}

#endif // WY3DAPP_THROUGH_3_POINTS_DATUM_PLN_CMD_H
