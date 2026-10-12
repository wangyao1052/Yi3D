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

#include <wy3dApp/SelectPreview.h>
#include <wy3dSelectionType.h>
#include <wy3dApp/Application.h>
#include <wy3dApp/Scene.h>
#include <wy3dApp/ElementNode.h>
#include "scene/nodes/SolidElementNode.h"
#include "scene/nodes/SheetElementNode.h"
#include "scene/nodes/SketchElementNode.h"
#include "scene/nodes/Sketch3DElementNode.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

SelectPreview::SelectPreview()
    : _selection(wydb::ElementId::kNull)
{
}

SelectPreview::~SelectPreview()
{
    this->clear();
}

void SelectPreview::update(const wyap::Selection& selection)
{
    if (_selection == selection) return;

    this->clear();
    _selection = selection;
    this->showSelection(_selection, true);
}

void SelectPreview::clear()
{
    this->showSelection(_selection, false);   // 空选择时自身早退
    _selection = wyap::Selection(wydb::ElementId::kNull);
}

void SelectPreview::showSelection(const wyap::Selection& selection, bool value)
{
    if (selection.getElementId().isNull()) return;

    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return;
    ElementNode* pElemNode = pActiveScene->getElementNode(selection.getElementId());
    if (!pElemNode) return;

    wy3d::SelectionType selType = wy3d::UIntToSelectionType(selection.getSelectionType());
    switch (selType)
    {
    case wy3d::SelectionType::Element:
    {
        pElemNode->preview(value);
    }
    break;

    case wy3d::SelectionType::Face:
    {
        const std::string& subPath = selection.getSubPath();
        if (subPath.empty()) return;
        if (SolidElementNode* pSolidElemNode = dynamic_cast<SolidElementNode*>(pElemNode))
        {
            pSolidElemNode->previewFace(std::stoul(subPath), value);
        }
        else if (SheetElementNode* pSheetElemNode = dynamic_cast<SheetElementNode*>(pElemNode))
        {
            pSheetElemNode->previewFace(std::stoul(subPath), value);
        }
    }
    break;

    case wy3d::SelectionType::Edge:
    {
        const std::string& subPath = selection.getSubPath();
        if (subPath.empty()) return;
        if (SolidElementNode* pSolidElemNode = dynamic_cast<SolidElementNode*>(pElemNode))
        {
            pSolidElemNode->previewEdge(std::stoul(subPath), value);
        }
        else if (SheetElementNode* pSheetElemNode = dynamic_cast<SheetElementNode*>(pElemNode))
        {
            pSheetElemNode->previewEdge(std::stoul(subPath), value);
        }
    }
    break;

    case wy3d::SelectionType::SketchCurve:
    {
        const std::string& subPath = selection.getSubPath();
        if (subPath.empty()) return;
        SketchElementNode* pSketchElemNode = dynamic_cast<SketchElementNode*>(pElemNode);
        if (!pSketchElemNode) return;
        pSketchElemNode->previewCurveById(std::stoul(subPath), value);
    }
    break;

    case wy3d::SelectionType::SketchCurve3D:
    {
        const std::string& subPath = selection.getSubPath();
        if (subPath.empty()) return;
        Sketch3DElementNode* pSketch3DElemNode = dynamic_cast<Sketch3DElementNode*>(pElemNode);
        if (!pSketch3DElemNode) return;
        pSketch3DElemNode->previewCurveById(std::stoul(subPath), value);
    }
    break;

    default:
    {
        assert(false);
        return;
    }
    break;
    }
}

}
