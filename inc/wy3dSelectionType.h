///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_ELEMENT_TYPE_H
#define WY3D_ELEMENT_TYPE_H

#include <wy3dDefs.h>

NS_WY3D_BEG

enum class SelectionType : std::uint32_t
{
    Element       = 0,
    SolidFace     = 0x00000020,
    SolidEdge     = 0x00000040,
    SolidVertex   = 0x00000080,
    SketchCurve   = 0x00000100,
    SketchCurve3D = 0x00000200,
};

// 按位或(组合类型)
constexpr SelectionType operator|(SelectionType a, SelectionType b) noexcept
{
    using Underlying = std::underlying_type_t<SelectionType>;
    return static_cast<SelectionType>(static_cast<Underlying>(a) | static_cast<Underlying>(b));
}

// 按位与(检查类型)
constexpr SelectionType operator&(SelectionType a, SelectionType b) noexcept
{
    using Underlying = std::underlying_type_t<SelectionType>;
    return static_cast<SelectionType>(static_cast<Underlying>(a) & static_cast<Underlying>(b));
}

// 按位取反
constexpr SelectionType operator~(SelectionType flags) noexcept
{
    using Underlying = std::underlying_type_t<SelectionType>;
    return static_cast<SelectionType>(~static_cast<Underlying>(flags));
}

class SelectionTypeUtil
{
public:
    // 包含性检查(判断flags是否包含target)
    static inline constexpr bool HasValue(SelectionType flags, SelectionType target) noexcept
    {
        return (flags & target) == target;
    }

    // 移除值
    static inline constexpr SelectionType RemoveValues(SelectionType flags, SelectionType flags_to_remove) noexcept
    {
        using Underlying = std::underlying_type_t<SelectionType>;
        return static_cast<SelectionType>(
            static_cast<Underlying>(flags) & ~static_cast<Underlying>(flags_to_remove));
    }
};


constexpr SelectionType UIntToSelectionType(std::uint32_t value) noexcept
{
    return static_cast<SelectionType>(value);
}

NS_WY3D_END

#endif // WY3D_ELEMENT_TYPE_H