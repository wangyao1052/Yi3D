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

#ifndef WY3DAPP_ELEMENT_NODE_IMPL_H
#define WY3DAPP_ELEMENT_NODE_IMPL_H

#include <cassert>
#include <osg/Group>

#include <wy3dApp/ElementNodeType.h>
#include <wy3dApp/ElementRenderObject.h>

namespace wy3dApp {

class ElementNodeImpl
{
public:
    ElementNodeImpl() = default;
    ~ElementNodeImpl() = default;
    ElementNodeImpl(const ElementNodeImpl&) = delete;
    ElementNodeImpl& operator=(const ElementNodeImpl&) = delete;
    ElementNodeImpl(ElementNodeImpl&&) = delete;
    ElementNodeImpl& operator=(ElementNodeImpl&&) = delete;

    enum class Status : unsigned int
    {
        Highlighted = 0x00000001, // 高亮
        Inactive    = 0x00000002, // 非活动状态,布尔体的成员为此状态
        Hidden      = 0x00000004, // 隐藏状态
        Previewed   = 0x00000008, // 预览(临时交互态,高亮优先)
    };

    bool isHighlighted() const
    {
        return (_status & static_cast<unsigned int>(Status::Highlighted)) != 0;
    }

    bool isHidden() const
    {
        return (_status & static_cast<unsigned int>(Status::Hidden)) != 0;
    }

    bool isActive() const
    {
        return !(_status & static_cast<unsigned int>(Status::Inactive));
    }

    bool isPreviewed() const
    {
        return (_status & static_cast<unsigned int>(Status::Previewed)) != 0;
    }

    void addStatus(Status s)
    {
        _status |= static_cast<unsigned int>(s);
    }

    void removeStatus(Status s)
    {
        _status &= ~static_cast<unsigned int>(s);
    }

    DisplayState getDisplayState() const
    {
        assert(!(this->isHighlighted() && this->isPreviewed()));
        DisplayState dispState;
        dispState.active = this->isActive();
        dispState.highlighted = this->isHighlighted();
        dispState.previewed = this->isPreviewed();
        return dispState;
    }

    void updateNodeMask(osg::Group* osgNode) const
    {
        if (!osgNode) return;

        if (this->isActive() && !this->isHidden())
        {
            osgNode->setNodeMask(kVisibleNodeMask); // 渲染+可拾取
        }
        else if (this->isHighlighted() || this->isPreviewed())
        {
            osgNode->setNodeMask(kRenderOnlyNodeMask); // 仅渲染
        }
        else
        {
            osgNode->setNodeMask(0);
        }
    }

private:
    static constexpr unsigned int kVisibleNodeMask =
        static_cast<unsigned int>(ElementNodeType::All);
    static constexpr unsigned int kRenderOnlyNodeMask = 0x80000000u;

    unsigned int _status = 0;
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_NODE_IMPL_H
