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

#ifndef WY3DAPP_DATUM_ELEMENT_NODE_H
#define WY3DAPP_DATUM_ELEMENT_NODE_H

#include <wy3dApp/ElementNode.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 基准元素节点基类:基准面/基准轴/基准点等
class DatumElementNode : public ElementNode
{
public:
    // 结点类型:所有基准类型共用大类位
    virtual ElementNodeType getNodeType() const override { return ElementNodeType::Datum; }

    // 按实体范围刷新基准对象的可视尺寸;默认不处理
    virtual void updateVisualSize(
        const wydb::Database* pDb, const osg::BoundingBox& bbox) {}

protected:
    explicit DatumElementNode(const wydb::ElementId& id) : ElementNode(id) {}
};

} // namespace wy3dApp

#endif // WY3DAPP_DATUM_ELEMENT_NODE_H
