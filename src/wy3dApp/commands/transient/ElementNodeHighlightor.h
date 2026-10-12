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

#ifndef WY3DAPP_ELEMENT_NODE_HIGHLIGHTOR_H
#define WY3DAPP_ELEMENT_NODE_HIGHLIGHTOR_H

#include <memory>
#include <wydbElementId.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class ElementNodeHighlightor
{
public:
    explicit ElementNodeHighlightor(const wydb::ElementId& id);
    ~ElementNodeHighlightor();

    void highlight() { this->highlightImpl(_id, true); }

private:
    void highlightImpl(const wydb::ElementId& id, bool flag);

private:
    wydb::ElementId _id;
};
typedef std::shared_ptr<ElementNodeHighlightor> ElementNodeHighlightorSPtr;

}

#endif // WY3DAPP_ELEMENT_NODE_HIGHLIGHTOR_H
