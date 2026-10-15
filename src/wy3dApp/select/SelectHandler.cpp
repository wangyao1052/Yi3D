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

#include "SelectHandler.h"
#include <cassert>
#include <wy3dApp/Application.h>
#include <wyapSelManager.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SelectHandler::SelectHandler() :
    _mouseDownX(0), _mouseDownY(0), _mouseUpX(0), _mouseUpY(0), _isMouseDown(false),
    _supportsPointSelect(true), _supportsBoxSelect(true),
    _allowedElemTypes(0x0000FFFF), _selType(wy3d::SelectionType::Element),
    _pPreSelFilter(nullptr), _pPostSelFilter(nullptr),
    _supportPreview(false), _selMode(SelectMode::Replace)
{
}

SelectHandler::~SelectHandler()
{
}

void SelectHandler::reset()
{
    _mouseDownX = _mouseDownY = _mouseUpX = _mouseUpY = 0;
    _isMouseDown = false;

    _supportsPointSelect = true;
    _supportsBoxSelect = true;

    _allowedElemTypes = 0x0000FFFF;
    _selType = wy3d::SelectionType::Element;
    _pPreSelFilter = nullptr;
    _pPostSelFilter = nullptr;

    _supportPreview = false;
    _preview.clear();
}

void SelectHandler::onMouseDown(float x, float y)
{
    _mouseDownX = x;
    _mouseDownY = y;
    _isMouseDown = true;
}

void SelectHandler::onMouseMove(float x, float y)
{
    if (!_supportPreview) return;

    _preview.update(this->pointPick(x, y));
}

void SelectHandler::onMouseUp(float x, float y, bool isCtrlDown)
{
    _mouseUpX = x;
    _mouseUpY = y;
    if (_isMouseDown)
    {
        this->clearBoxRect();
        this->select(isCtrlDown);
    }
    _isMouseDown = false;
}

void SelectHandler::onMouseDrag(float x, float y)
{
    if (_isMouseDown)
    {
        _mouseUpX = x;
        _mouseUpY = y;
        if (_supportsBoxSelect)
            this->drawBoxRect(_mouseDownX, _mouseDownY, _mouseUpX, _mouseUpY);
    }
}

// ── 选择逻辑 ──

void SelectHandler::select(bool isCtrlDown)
{
    // 点选（移动距离 < 5 像素视为点选）
    if (std::abs(_mouseUpX - _mouseDownX) + std::abs(_mouseUpY - _mouseDownY) < 5)
    {
        if (_supportsPointSelect)
            this->pointSelect(isCtrlDown);
    }
    else // 框选
    {
        if (_supportsBoxSelect)
        {
            bool isCross = _mouseUpX < _mouseDownX;
            this->boxSelect(isCross, isCtrlDown);
        }
    }
}

void SelectHandler::pointSelect(bool isCtrlDown)
{
    wyap::Selection sel = this->pointPick(_mouseDownX, _mouseDownY);
    wyap::SelectionSet ss;
    if (!sel.getElementId().isNull()) ss.add(sel);
    this->updateSelectionSet(ss, isCtrlDown);
}

void SelectHandler::boxSelect(bool isCross, bool isCtrlDown)
{
    // 保持旧行为：不排序坐标，直接透传原始鼠标位置
    float x1 = _mouseDownX;
    float x2 = _mouseUpX;
    float y1 = _mouseDownY;
    float y2 = _mouseUpY;

    wyap::SelectionSet ss;
    if (isCross)
        ss = this->boxPick_cross(x1, y1, x2, y2);
    else
    {
        // 窗口框选需要确保 min < max 传给 Polytope
        float xMin = x1 < x2 ? x1 : x2;
        float xMax = x1 < x2 ? x2 : x1;
        float yMin = y1 < y2 ? y1 : y2;
        float yMax = y1 < y2 ? y2 : y1;
        ss = this->boxPick_window(xMin, yMin, xMax, yMax);
    }

    // 用户自定义过滤
    if (_pPostSelFilter && !ss.isEmpty())
    {
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        assert(pDb);
        wyap::SelectionSet ssFiltered;
        bool skipRemaining(false);
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            wyap::Selection sel = iter.current();
            switch ((*_pPostSelFilter)(pDb, sel, SelectAction::Window))
            {
            case SelectFilterStatus::Ok:
                ssFiltered.add(sel);
                break;
            case SelectFilterStatus::Continue:
                break;
            case SelectFilterStatus::Break:
                skipRemaining = true;
                break;
            default:
                assert(false);
                break;
            }

            if (skipRemaining) break;
        }
        ss.swap(ssFiltered);
    }

    this->updateSelectionSet(ss, isCtrlDown);
}

// ── 选择集管理 ──

void SelectHandler::updateSelectionSet(const wyap::SelectionSet& ss, bool isCtrlDown)
{
    Application::instance().getSelManager()->beginChange();
    switch (_selMode)
    {
    case SelectMode::Replace:
        this->updateSelectionSet_Replace(ss, isCtrlDown);
        break;
    case SelectMode::Add:
        this->updateSelectionSet_Add(ss, isCtrlDown);
        break;
    default:
        this->updateSelectionSet_Replace(ss, isCtrlDown);
        assert(false);
        break;
    }
    Application::instance().getSelManager()->endChange();
}

void SelectHandler::updateSelectionSet_Replace(const wyap::SelectionSet& ss, bool isCtrlDown)
{
    wyap::SelManager* pSelMgr = Application::instance().getSelManager();
    assert(pSelMgr);

    if (isCtrlDown) // 反转状态
    {
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            if (pSelMgr->getSelections().contains(iter.current()))
                pSelMgr->removeSelection(iter.current());
            else
                pSelMgr->addSelection(iter.current());
        }
    }
    else // 替换覆盖
    {
        if (ss.isEmpty())
            pSelMgr->clearSelections();
        else
            pSelMgr->setSelections(ss);
    }
}

void SelectHandler::updateSelectionSet_Add(const wyap::SelectionSet& ss, bool isCtrlDown)
{
    wyap::SelManager* pSelMgr = Application::instance().getSelManager();
    assert(pSelMgr);

    if (isCtrlDown) // 反转状态
    {
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            if (pSelMgr->getSelections().contains(iter.current()))
                pSelMgr->removeSelection(iter.current());
            else
                pSelMgr->addSelection(iter.current());
        }
    }
    else // 添加
    {
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            if (pSelMgr->getSelections().contains(iter.current()))
                continue;
            else
                pSelMgr->addSelection(iter.current());
        }
    }
}

} // namespace wy3dApp
