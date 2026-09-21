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

#ifndef WY3DAPP_SELECT_HANDLER_H
#define WY3DAPP_SELECT_HANDLER_H

#include <wyapSelection.h>
#include <wy3dSelectionType.h>
#include "SelectFilterFunctor.h"
#include "SelectPreview.h"
#include "select/SelectMode.h"

// 选择处理器基类（渲染引擎无关）
class SelectHandler
{
public:
    SelectHandler();
    virtual ~SelectHandler();

    // 重置
    void reset();

    // 配置方法
    void enablePointSelect(bool flag)   { _supportsPointSelect = flag; }
    bool isSupportsPointSelect() const  { return _supportsPointSelect; }

    void enableBoxSelect(bool flag)     { _supportsBoxSelect = flag; }
    bool isSupportsBoxSelect() const    { return _supportsBoxSelect; }

    void setPickMask(unsigned int mask) { _pickMask = mask; }
    unsigned int getPickMask() const    { return _pickMask; }

    void setSelectionType(wy3d::SelectionType t) { _selType = t; }
    wy3d::SelectionType getSelectionType() const { return _selType; }

    void setPreFilterFunctor(SelectPreFilterFunctorSPtr p) { _pPreSelFilter = p; }
    void setFilterFunctor(SelectFilterFunctorSPtr p)       { _pSelFilter = p; }

    void enablePreview(bool flag)       { _supportPreview = flag; }
    bool isSupportPreview() const       { return _supportPreview; }

    void setSelectMode(SelectMode m)    { _selMode = m; }
    SelectMode getSelectMode() const    { return _selMode; }

    // What a click at this position would select, without writing the selection set.
    // Lets a command decide who a click belongs to, using this handler's own criteria.
    wyap::Selection querySelectionAt(float x, float y) { return this->pointPick(x, y); }

protected:
    // ── 事件回调（由子类从渲染引擎事件中提取数据后调用）──
    void onMouseDown(float x, float y);
    void onMouseMove(float x, float y);
    void onMouseUp(float x, float y, bool isCtrlDown);
    void onMouseDrag(float x, float y);

    // ── 拾取接口（由子类实现）──
    virtual wyap::Selection pointPick(float x, float y) = 0;
    virtual wyap::SelectionSet boxPick_cross(float xMin, float yMin,
                                              float xMax, float yMax) = 0;
    virtual wyap::SelectionSet boxPick_window(float xMin, float yMin,
                                               float xMax, float yMax) = 0;

    // ── 框选矩形绘制（由子类实现）──
    virtual void drawBoxRect(float xMin, float yMin,
                              float xMax, float yMax) = 0;
    virtual void clearBoxRect() = 0;

private:
    // 选择逻辑
    void select(bool isCtrlDown);
    void pointSelect(bool isCtrlDown);
    void boxSelect(bool isCross, bool isCtrlDown);

    // 更新选择集
    void updateSelectionSet(const wyap::SelectionSet& ss, bool isCtrlDown);
    void updateSelectionSet_Full(const wyap::SelectionSet& ss, bool isCtrlDown);
    void updateSelectionSet_Incremental(const wyap::SelectionSet& ss, bool isCtrlDown);

protected:
    // 鼠标坐标
    float _mouseDownX, _mouseDownY;
    float _mouseUpX, _mouseUpY;
    bool _isMouseDown;

    // 配置状态
    bool _supportsPointSelect;
    bool _supportsBoxSelect;
    unsigned int _pickMask;
    wy3d::SelectionType _selType;
    SelectPreFilterFunctorSPtr _pPreSelFilter;
    SelectFilterFunctorSPtr _pSelFilter;
    bool _supportPreview;
    SelectPreviewSPtr _pPreview;
    SelectMode _selMode;
};

#endif // WY3DAPP_SELECT_HANDLER_H
