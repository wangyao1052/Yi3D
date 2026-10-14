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

#ifndef WY3DAP_SKETCH3D_ENVIRONMENT_IMPL_H
#define WY3DAP_SKETCH3D_ENVIRONMENT_IMPL_H

#include <memory>

#include <wydbElementId.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelection.h>
#include <wy3dSketch3D.h>
#include <wy3dPlane.h>

#include "snap3d/Sketch3DSnapSystem.h"
#include <wy3dApp/Sketch3DEnvironment.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class Sketch3DEnvironmentUI;

class Sketch3DEnvironmentImpl : public Sketch3DEnvironment
{
public:
    Sketch3DEnvironmentImpl();
    explicit Sketch3DEnvironmentImpl(const wy3d::Sketch3D* pSketch3D);
    ~Sketch3DEnvironmentImpl();

public:
    virtual Operation getOperation() const override;
    virtual wydb::ElementId getSketch3dId() const override;
    virtual Sketch3DSnapSystem* getSnapSystem() const override;
    virtual bool addContextMenuActions(ContextMenu& menu) override;

    wydb::Transaction* getSketchTransaction() const;
    bool isTransactionCommited() const;

    // Sketch plane picked in the Select command, together with the picked face itself
    // (the drawing command needs the latter to carry the highlight over). Consumed by the
    // next drawing command, so take() reads and clears in one step.
    void setPendingWorkPlane(const wy3d::Plane& plane, const wyap::Selection& sel);
    bool takePendingWorkPlane(wy3d::Plane& plane, wyap::Selection& sel);
    void clearPendingWorkPlane();

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
    std::unique_ptr<Sketch3DSnapSystem> _pSnapSys;

    // kInvalid is the empty state: the default constructed Plane is a valid one.
    wy3d::Plane _pendingWorkPlane;
    wyap::Selection _pendingPlaneSelection;
};

} // namespace wy3dApp

#endif // WY3DAP_SKETCH3D_ENVIRONMENT_IMPL_H
