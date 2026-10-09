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

#ifndef WY3DAP_SKETCH_ENVIRONMENT_H
#define WY3DAP_SKETCH_ENVIRONMENT_H

#include <wydbElementId.h>
#include <wyapEnvironment.h>
#include <wy3dSketch.h>

#include "environments/EnvironmentBase.h"
#include "environments/ICommandActionStateHost.h"
#include "wy3dAppDefs.h"

class SketchSnapSystem;

class WY3DAPP_EXPORT SketchEnvironment
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
    virtual wydb::ElementId getSketchId() const = 0;
    virtual const wy3d::SketchPlane& getSketchPlane() const = 0;
    virtual SketchSnapSystem* getSnapSystem() const = 0;

protected:
    SketchEnvironment() = default;
    ~SketchEnvironment();
};

#endif // WY3DAP_SKETCH_ENVIRONMENT_H
