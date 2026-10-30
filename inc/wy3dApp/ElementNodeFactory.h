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

#ifndef WY3DAPP_ELEMENT_NODE_FACTORY_H
#define WY3DAPP_ELEMENT_NODE_FACTORY_H

#include <functional>

#include <wy3dApp/wy3dAppDefs.h>

namespace wyrx { class ClassInfo; }
namespace wydb { class Element; }

namespace wy3dApp {

class ElementNode;

struct ElementNodeCreateContext
{
    const wyrx::ClassInfo* pEnvironmentClassInfo = nullptr;
};

using ElementNodeCreator = std::function<ElementNode*(
    const wydb::Element* pElem, const ElementNodeCreateContext& context)>;

class WY3DAPP_EXPORT ElementNodeFactory
{
public:
    virtual ~ElementNodeFactory();

    virtual bool registerCreator(
        wyrx::ClassInfo* pElemClassInfo, ElementNodeCreator creator) = 0;
    virtual bool unregisterCreator(wyrx::ClassInfo* pElemClassInfo) = 0;

    virtual ElementNode* create(
        const wydb::Element* pElem,
        const ElementNodeCreateContext& context) const = 0;

protected:
    ElementNodeFactory();
    ElementNodeFactory(const ElementNodeFactory&) = delete;
    ElementNodeFactory& operator=(const ElementNodeFactory&) = delete;
    ElementNodeFactory(ElementNodeFactory&&) = delete;
    ElementNodeFactory& operator=(ElementNodeFactory&&) = delete;
};

} // namespace wy3dApp

#endif // WY3DAPP_ELEMENT_NODE_FACTORY_H
