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

#ifndef WY3DAPP_ELEMENT_NODE_TYPE_H
#define WY3DAPP_ELEMENT_NODE_TYPE_H

enum class ElementNodeType
{
    Undefined         = 0,
    Solid             = 0x00000001, // 实体
    Sketch            = 0x00000002, // 草图
    SketchEntity      = 0x00000004, // 草图图元
    DatumPlane        = 0x00000008, // 基准面
    SolidModification = 0x00000010, // 实体修改
    Curve             = 0x00000020, // 曲线
    Sheet             = 0x00000040, // 曲面
};

// 按位或(组合类型)
constexpr ElementNodeType operator|(ElementNodeType a, ElementNodeType b) noexcept
{
    using Underlying = std::underlying_type_t<ElementNodeType>;
    return static_cast<ElementNodeType>(static_cast<Underlying>(a) | static_cast<Underlying>(b));
}

// 按位与(检查类型)
constexpr ElementNodeType operator&(ElementNodeType a, ElementNodeType b) noexcept
{
    using Underlying = std::underlying_type_t<ElementNodeType>;
    return static_cast<ElementNodeType>(static_cast<Underlying>(a) & static_cast<Underlying>(b));
}

// 按位取反
constexpr ElementNodeType operator~(ElementNodeType flags) noexcept
{
    using Underlying = std::underlying_type_t<ElementNodeType>;
    return static_cast<ElementNodeType>(~static_cast<Underlying>(flags));
}

#endif // WY3DAPP_ELEMENT_NODE_TYPE_H