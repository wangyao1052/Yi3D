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

namespace wy3dApp {

// ElementNode 的内部实现。当前刻意保持为空，后续逐步迁入
// 状态机、NodeMask 与 id 等实现细节。
class ElementNodeImpl
{
public:
    ElementNodeImpl();
    ~ElementNodeImpl();

    ElementNodeImpl(const ElementNodeImpl&) = delete;
    ElementNodeImpl& operator=(const ElementNodeImpl&) = delete;
    ElementNodeImpl(ElementNodeImpl&&) = delete;
    ElementNodeImpl& operator=(ElementNodeImpl&&) = delete;
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_NODE_IMPL_H
