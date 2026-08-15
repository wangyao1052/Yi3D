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

#ifndef WY3DAPP_GATEWAY_ENVIRONMENT_H
#define WY3DAPP_GATEWAY_ENVIRONMENT_H

#include <memory>

#include <wydbTransaction.h>
#include <wyapEnvironment.h>

#include "environments/EnvironmentBase.h"

class GatewayEnvironmentUI;

class GatewayEnvironment
    : public wyap::GatewayEnvironment
    , public EnvironmentBase
    , public wydb::TransactionReactor
{
public:
    GatewayEnvironment();
    ~GatewayEnvironment();

    // Document manager reactor functions.
    virtual void onDocumentDeactivated(wyap::Document* pDeactivatedDoc) override;
    virtual void onDocumentActivated(wyap::Document* pActivatedDoc) override;

    virtual void onTransactionStarted(wydb::Transaction* pTrans) override;
    virtual void onTransactionEnded(wydb::Transaction* pTrans) override;
    virtual void onTransactionAborted(wydb::Transaction* pTrans) override;

    // Command manager reactor functions.
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

    // Do nothing.
    virtual void processCommandTerminatedOnEventLoop() override {}

    // Update command action states.
    void updateCommandActionStates();
    // Update file action states (New/Open).
    void updateFileActionStates();

private:
    void registerCommands();
    void removeCommands();

private:
    std::unique_ptr<GatewayEnvironmentUI> _pUI;
};

#endif // WY3DAPP_GATEWAY_ENVIRONMENT_H
