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

#ifndef WY3DAPP_GATEWAY_ENVIRONMENT_UI_H
#define WY3DAPP_GATEWAY_ENVIRONMENT_UI_H

#include <QMenu>
#include <QToolBar>

class GatewayEnvironment;

class GatewayEnvironmentUI
{
public:
    GatewayEnvironmentUI();
    ~GatewayEnvironmentUI();

    void initialize(GatewayEnvironment* pEnv);
    void teardown(GatewayEnvironment* pEnv);

private:
    void clear();
    void createMenus(GatewayEnvironment* pEnv);
    void createToolBars(GatewayEnvironment* pEnv);

private:
    QMenu* _pMenuFile;
    QMenu* _pMenuTools;
    QMenu* _pMenuHelp;
    QToolBar* _pToolBarFile;
};

#endif // WY3DAPP_GATEWAY_ENVIRONMENT_UI_H
