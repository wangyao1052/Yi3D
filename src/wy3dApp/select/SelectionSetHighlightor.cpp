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

#include "SelectionSetHighlightor.h"
#include <wy3dSelectionType.h>
#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "scene/nodes/SolidElementNode.h"
#include "scene/nodes/SheetElementNode.h"
#include "scene/nodes/SketchElementNode.h"

SelectionSetHighlightor::SelectionSetHighlightor(const wyap::SelectionSet& ss)
    : _ss(ss), _useCustomColor(false)
{
    this->showSelectionSet(_ss, true);
}

SelectionSetHighlightor::SelectionSetHighlightor(const wyap::SelectionSet& ss, const osg::Vec4& color)
    : _ss(ss), _useCustomColor(true), _color(color)
{
    this->showSelectionSet(_ss, true);
}

SelectionSetHighlightor::~SelectionSetHighlightor()
{
    this->showSelectionSet(_ss, false);
}

bool SelectionSetHighlightor::addSelection(const wyap::Selection& sel)
{
    if (_ss.add(sel))
    {
        this->showSelection(sel, true);
        return true;
    }
    else
    {
        return false;
    }
}

bool SelectionSetHighlightor::removeSelection(const wyap::Selection& sel)
{
    if (_ss.remove(sel))
    {
        this->showSelection(sel, false);
        return true;
    }
    else
    {
        return false;
    }
}

void SelectionSetHighlightor::clearSelections()
{
    this->showSelectionSet(_ss, false);
    _ss.clear();
}

bool SelectionSetHighlightor::containsSelection(const wyap::Selection& sel) const
{
    return _ss.contains(sel);
}

void SelectionSetHighlightor::showSelectionSet(const wyap::SelectionSet& ss, bool value)
{
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        this->showSelection(iter.current(), value);
    }
}

void SelectionSetHighlightor::showSelection(const wyap::Selection& sel, bool value)
{
    Scene* pActiveScene = Application::instance().getActiveScene();
    if (!pActiveScene) return;
    ElementNode* pElemNode = pActiveScene->getElementNode(sel.getElementId());
    if (!pElemNode) return;

    wy3d::SelectionType selType = wy3d::UIntToSelectionType(sel.getSelectionType());
    switch (selType)
    {
    case wy3d::SelectionType::Element:
    {
        pElemNode->highlight(value);
    }
    break;

    case wy3d::SelectionType::SolidFace:
    {
        const std::string& subPath = sel.getSubPath();
        if (subPath.empty()) return;
        if (SolidElementNode* pSolidElemNode = dynamic_cast<SolidElementNode*>(pElemNode))
        {
            if (value && _useCustomColor)
                pSolidElemNode->highlightFace(std::stoul(subPath), _color);
            else
                pSolidElemNode->highlightFace(std::stoul(subPath), value);
        }
        else if (SheetElementNode* pSheetElemNode = dynamic_cast<SheetElementNode*>(pElemNode))
        {
            if (value && _useCustomColor)
                pSheetElemNode->highlightFace(std::stoul(subPath), _color);
            else
                pSheetElemNode->highlightFace(std::stoul(subPath), value);
        }
    }
    break;

    case wy3d::SelectionType::SolidEdge:
    {
        const std::string& subPath = sel.getSubPath();
        if (subPath.empty()) return;
        if (SolidElementNode* pSolidElemNode = dynamic_cast<SolidElementNode*>(pElemNode))
        {
            pSolidElemNode->highlightEdge(std::stoul(subPath), value);
        }
        else if (SheetElementNode* pSheetElemNode = dynamic_cast<SheetElementNode*>(pElemNode))
        {
            pSheetElemNode->highlightEdge(std::stoul(subPath), value);
        }
    }
    break;

    case wy3d::SelectionType::SketchCurve:
    {
        const std::string& subPath = sel.getSubPath();
        if (subPath.empty()) return;
        SketchElementNode* pSketchElemNode = dynamic_cast<SketchElementNode*>(pElemNode);
        if (!pSketchElemNode) return;
        pSketchElemNode->highlightCurveById(std::stoul(subPath), value);
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