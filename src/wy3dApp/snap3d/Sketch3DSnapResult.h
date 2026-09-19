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

#ifndef WY3DAPP_SKETCH3D_SNAP_RESULT_H
#define WY3DAPP_SKETCH3D_SNAP_RESULT_H

#include <memory>
#include <vector>

#include <wyVector3.h>

#include "snap3d/Sketch3DSnapObject.h"
#include "snap/SnapTipWidget.h"

class SketchSnapTipMouseFollow;
class GuiCmdTransient;
using GuiCmdTransientSPtr = std::shared_ptr<GuiCmdTransient>;

class Sketch3DSnapResult
{
public:
    explicit Sketch3DSnapResult(const wy::Vector3& position);
    ~Sketch3DSnapResult();

    struct Item
    {
        std::shared_ptr<Sketch3DSnapObject> pSnapObject;
        std::vector<GuiCmdTransientSPtr> transients;
        SnapTipWidget* pSnapTipWidget;

        Item() : pSnapObject(nullptr), pSnapTipWidget(nullptr) {}
    };

    void addItem(Item&& item);

    wy::Vector3 getPosition() const
    {
        return _position;
    }
    void setPosition(const wy::Vector3& position)
    {
        _position = position;
    }

    void show();
    void hide();

    bool isLooseEqual(const Sketch3DSnapResult& other) const;

private:
    std::vector<Item> _items;
    wy::Vector3 _position;
    std::shared_ptr<SketchSnapTipMouseFollow> _pMouseFollow;
};

using Sketch3DSnapResultSPtr = std::shared_ptr<Sketch3DSnapResult>;

#endif // WY3DAPP_SKETCH3D_SNAP_RESULT_H