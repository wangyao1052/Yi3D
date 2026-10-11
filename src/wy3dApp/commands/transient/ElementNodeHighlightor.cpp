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

#include "ElementNodeHighlightor.h"
#include <wyapSelection.h>
#include <wy3dApp/Application.h>
#include <wy3dApp/Scene.h>
#include "scene/nodes/SolidElementNode.h"

ElementNodeHighlightor::ElementNodeHighlightor(const wydb::ElementId& id)
    : _id(id)
{
    this->highlightImpl(_id, true);
}

ElementNodeHighlightor::~ElementNodeHighlightor()
{
    if (!_id.isNull())
    {
        const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
        if (!ss.contains(wyap::Selection(_id)))
        {
            this->highlightImpl(_id, false);
        }
    }
}

void ElementNodeHighlightor::highlightImpl(const wydb::ElementId& id, bool flag)
{
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene) return;
    SolidElementNode* pSolidNode = dynamic_cast<SolidElementNode*>(pScene->getElementNode(id));
    if (!pSolidNode) return;
    pSolidNode->highlight(flag, false);
}