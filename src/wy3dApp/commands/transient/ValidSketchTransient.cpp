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

#include "ValidSketchTransient.h"
#include <QToolTip>
#include <wy3dApp/Application.h>
#include <wy3dApp/Scene.h>
#include "scene/nodes/SketchElementNode.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

ValidSketchTransient::ValidSketchTransient(const wydb::ElementId& sketchId) : _sketchId(sketchId)
{
    this->highlight(_sketchId, true);
}

ValidSketchTransient::~ValidSketchTransient()
{
    this->highlight(_sketchId, false);
}

void ValidSketchTransient::highlight(const wydb::ElementId& id, bool flag)
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return;
    ElementNode* pNode = pScene->getElementNode(id);
    if (!pNode) return;
    pNode->highlight(flag);
}

InvalidSketchToolTip::InvalidSketchToolTip(const wydb::ElementId& sketchId, const QString& error)
    : _sketchId(sketchId)
{
    QToolTip::showText(QCursor::pos(), error, nullptr, QRect(), 3000); // 3s
}

InvalidSketchToolTip::~InvalidSketchToolTip()
{
    QToolTip::hideText();
}

}
