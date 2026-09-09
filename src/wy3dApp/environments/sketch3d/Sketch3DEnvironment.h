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

#include <memory>

#include <wydbElementId.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapEnvironment.h>
#include <wy3dSketch3D.h>

#include "environments/EnvironmentBase.h"
#include "environments/ICommandActionStateHost.h"

class Sketch3DEnvironmentUI;

class Sketch3DEnvironment
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

public:
    Sketch3DEnvironment();
    explicit Sketch3DEnvironment(const wy3d::Sketch3D* pSketch3D);
    ~Sketch3DEnvironment();

    Operation getOperation() const
    {
        return _op;
    }

    wydb::ElementId getSketch3dId() const
    {
        return _sketch3dId;
    }

    wydb::Transaction* getSketchTransaction() const
    {
        return _pTopTrans;
    }

    bool isTransactionCommited() const
    {
        return _isTransCommitted;
    }

public:
    virtual void onCommandStartFailed(
        wyap::Command* pCmd,
        wyap::CmdExecution::StartResult startResult) override;
    virtual void onCommandStarted(wyap::Command* pCmd) override;
    virtual void onCommandEnded(wyap::Command* pCmd) override;
    virtual void onCommandAborted(
        wyap::Command* pCmd,
        wyap::CmdExecution::AbortCause abortCause) override;

protected:
    virtual void onEnter() override;
    virtual void onExit(ExitCode exitCode) override;
    virtual void onSuspend() override;
    virtual void onResume() override;

    virtual void updateUndoRedoActionStates() override;
    virtual void updateFileActionStates() override;

private:
    void registerCommands();
    void removeCommands();

    wy3d::Sketch3D* newSketch3D(wydb::Database* pDb);
    void commitSketch3D(wydb::Database* pDb, bool ok);

private:
    Operation _op;
    wydb::ElementId _sketch3dId;
    wydb::Transaction* _pTopTrans;
    bool _isTransCommitted;

    std::unique_ptr<Sketch3DEnvironmentUI> _pUI;
};

#endif // WY3DAP_SKETCH3D_ENVIRONMENT_H
