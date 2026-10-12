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

#ifndef WY3DAP_SKETCH3D_ENVIRONMENT_H
#define WY3DAP_SKETCH3D_ENVIRONMENT_H

#include <wydbElementId.h>
#include <wyapEnvironment.h>

#include <wy3dApp/EnvironmentBase.h>
#include "environments/ICommandActionStateHost.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class Sketch3DSnapSystem;

class WY3DAPP_EXPORT Sketch3DEnvironment
    : public wyap::TaskEnvironment
    , public EnvironmentBase
    , public ICommandActionStateHost
{
public:
    enum class Operation
    {
        New = 0,
        Edit = 1,
    };

    virtual Operation getOperation() const = 0;
    virtual wydb::ElementId getSketch3dId() const = 0;
    virtual Sketch3DSnapSystem* getSnapSystem() const = 0;

protected:
    Sketch3DEnvironment() = default;
    ~Sketch3DEnvironment();
};

}

#endif // WY3DAP_SKETCH3D_ENVIRONMENT_H
