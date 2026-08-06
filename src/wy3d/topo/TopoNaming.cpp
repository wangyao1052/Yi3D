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

#include <wy3dTopoNaming.h>
#include <algorithm>
#include <sstream>
#include <cassert>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>
#include "topo/TopoShapeComparer.h"

NS_WY3D_BEG

bool TopoNaming::contains(const TopoDS_Shape& shape) const
{
    return _nameMap.find(shape) != _nameMap.cend();
}

bool TopoNaming::erase(const TopoDS_Shape& shape)
{
    auto iter = _nameMap.find(shape);
    if (iter == _nameMap.cend())
    {
        return false;
    }
    _nameMap.erase(iter);
    return true;
}

void TopoNaming::setName(const TopoDS_Shape& shape, const TopoName& name)
{
    assert(!shape.IsNull());
    const bool validName = TopoNameCodec::isValid(name);
    assert(validName);
    if (!shape.IsNull() && validName)
    {
        _nameMap[shape] = name;
    }
}

void TopoNaming::setName(const TopoDS_Shape& shape, TopoName&& name)
{
    assert(!shape.IsNull());
    const bool validName = TopoNameCodec::isValid(name);
    assert(validName);
    if (!shape.IsNull() && validName)
    {
        _nameMap[shape] = std::move(name);
    }
}

TopoName TopoNaming::getTopoName(const TopoDS_Shape& shape) const
{
    auto iter = _nameMap.find(shape);
    if (iter != _nameMap.cend())
    {
        return iter->second;
    }
    return {};
}

bool TopoNaming::getName(const TopoDS_Shape& shape, TopoName& name) const
{
    auto iter = _nameMap.find(shape);
    if (iter == _nameMap.cend())
    {
        return false;
    }

    name = iter->second;
    return true;
}

TopoDS_Shape TopoNaming::smartFind(TopAbs_ShapeEnum shapeType, const TopoName& name) const
{
    if (name.empty())
    {
        assert(false);
        return TopoDS_Shape();
    }
    return this->find(shapeType, name);
}

TopoDS_Shape TopoNaming::find(TopAbs_ShapeEnum shapeType, const TopoName& name) const
{
    for (const auto& kvp : _nameMap)
    {
        if (kvp.first.ShapeType() != shapeType)
        {
            continue;
        }

        if (kvp.second == name)
        {
            return kvp.first;
        }
    }

    return TopoDS_Shape();
}

void TopoNaming::add(const ShapeDelta& delta, unsigned int updateId, unsigned int recursionLevel)
{
    TopoName name;

    for (const auto& kvp : delta.addedSingle)
    {
        // kvp.first  --- new
        // kvp.second --- source information
        const ShapeDelta::SingleSourceInfo& sourceInfo = kvp.second;
        const TopoDS_Shape& sourceShape = sourceInfo.source;
        assert(!sourceShape.IsNull());

        if (!this->getName(sourceShape, name))
        {
            assert(false);
            continue;
        }
        if (name.empty())
        {
            assert(false);
            continue;
        }

        switch (sourceInfo.evolution)
        {
        case ShapeEvolution::Generated:
            name = TopoNameBuilder(name).generated(updateId).build();
            break;

        case ShapeEvolution::GeneratedMultiple:
            assert(sourceInfo.resultIndex > 0);
            name = TopoNameBuilder(name).generated(updateId).index(sourceInfo.resultIndex).build();
            break;

        case ShapeEvolution::Split:
            assert(sourceInfo.resultIndex > 0);
            name = TopoNameBuilder(name).split(sourceInfo.resultIndex).build();
            break;

        default:
            assert(false);
            continue;
        }

        this->setName(kvp.first, name);
    }

    std::unordered_map<TopoDS_Shape, ShapeDelta::DoubleSourceInfo, ShapeHasher, ShapeEqual> todoAddedDouble;
    for (const auto& kvp : delta.addedDouble)
    {
        // kvp.first  --- new
        // kvp.second --- source
        const ShapeDelta::DoubleSourceInfo& source = kvp.second;
        assert(!source.source1.IsNull());

        if (!this->getName(source.source1, name))
        {
            assert(false);
            continue;
        }
        assert(!source.source2.IsNull());
        TopoName sourceName2;
        if (!this->getName(source.source2, sourceName2)) // source2也是新增的,暂时存储下来,下次迭代再处理;
        {
            todoAddedDouble[kvp.first] = kvp.second;
            continue;
        }
        if (name.empty())
        {
            assert(false);
            continue;
        }
        TopoNameBuilder builder(name);
        builder.source(sourceName2).generated(updateId);
        if (0 != source.index)
        {
            builder.index(source.index);
        }
        this->setName(kvp.first, builder.build());
    }

    std::unordered_map<TopoDS_Shape, ShapeDelta::MultiSourceInfo, ShapeHasher, ShapeEqual> todoAddedMulti;
    for (const auto& kvp : delta.addedMulti)
    {
        // kvp.first  --- new
        // kvp.second --- source
        const TopoShapeSet& sources = kvp.second.sources;

        TopoNameList sourceNames;
        sourceNames.reserve(sources.size());
        bool success(true);
        for (const TopoDS_Shape& source : sources)
        {
            if (source.IsNull())
            {
                assert(false);
                continue;
            }
            TopoName sourceName;
            if (!this->getName(source, sourceName)) // source也是新增的
            {
                success = false;
                break;
            }
            sourceNames.emplace_back(std::move(sourceName));
        }
        if (!success)
        {
            todoAddedMulti[kvp.first] = kvp.second;
            continue;
        }

        if (sourceNames.empty())
        {
            assert(false);
            continue;
        }

        std::sort(sourceNames.begin(), sourceNames.end());
        TopoNameBuilder builder(sourceNames.front());
        for (size_t i = 1; i < sourceNames.size(); ++i)
        {
            builder.source(sourceNames[i]);
        }
        builder.generated(updateId);
        if (0 != kvp.second.index)
        {
            assert(false); // 先增加assert因为还没实际想到具体的case added by wangyao 2025.08.16
            builder.index(kvp.second.index);
        }
        this->setName(kvp.first, builder.build());
    }

    if (!todoAddedDouble.empty() || !todoAddedMulti.empty())
    {
        // 防止死循环
        if (recursionLevel >= 10)
        {
            assert(false);
            return;
        }
        ShapeDelta newDelta;
        newDelta.addedDouble.swap(todoAddedDouble);
        newDelta.addedMulti.swap(todoAddedMulti);
        this->add(newDelta, updateId, ++recursionLevel);
    }
}

void TopoNaming::modify(const ShapeDelta& delta)
{
    TopoName name;
    for (const auto& kvp : delta.modified)
    {
        // kvp.first  --- old
        // kvp.second --- new
        name.clear();
        if (!this->getName(kvp.first, name))
        {
            assert(false);
            continue;
        }

        this->setName(kvp.second, name);
    }
}

void TopoNaming::erase(const ShapeDelta& delta)
{
    for (const TopoDS_Shape& shape : delta.deleted)
    {
        this->erase(shape);
    }
}

void TopoNaming::clearModify(const ShapeDelta& delta)
{
    for (const auto& kvp : delta.modified)
    {
        // kvp.first  --- old
        // kvp.second --- new
        this->erase(kvp.first);
    }
}

void TopoNaming::update(const TopoShapeComparer* pShapeComparer, unsigned int updateId)
{
    assert(updateId != 0);
    if (!pShapeComparer)
    {
        return;
    }
    const TopoShapeComparer& shapeComparer = *pShapeComparer;

    // 变化
    const ShapeDelta& edgeDelta = shapeComparer.getEdgeDelta();
    const ShapeDelta& faceDelta = shapeComparer.getFaceDelta();

    // 修改
    this->modify(faceDelta);
    this->modify(edgeDelta);

    // 新增
    this->add(faceDelta, updateId);
    this->add(edgeDelta, updateId);

    // 删除
    this->erase(faceDelta);
    this->erase(edgeDelta);

    // 清理修改
    this->clearModify(faceDelta);
    this->clearModify(edgeDelta);
}

bool TopoNaming::set(const TopoNaming& rhs, const TopoDS_Shape& shape, const TopoDS_Shape& originalShape)
{
    if (shape.IsEqual(originalShape))
    {
        *this = rhs;
        return true;
    }

    TopTools_IndexedMapOfShape faceMap0;
    TopExp::MapShapes(originalShape, TopAbs_ShapeEnum::TopAbs_FACE, faceMap0);
    TopTools_IndexedMapOfShape edgeMap0;
    TopExp::MapShapes(originalShape, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap0);

    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faceMap);
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);

    if (faceMap0.Extent() != faceMap.Extent()) return false;
    if (edgeMap0.Extent() != edgeMap.Extent()) return false;

    NameMap nameMap;

    auto updateValue = [&rhs, &nameMap](
        const TopTools_IndexedMapOfShape& map0, const TopTools_IndexedMapOfShape& map)
    {
        for (int i = 1; i <= map0.Extent(); ++i)
        {
            const TopoDS_Shape& shape0 = map0(i);
            const TopoDS_Shape& shape = map(i);

            auto iter = rhs._nameMap.find(shape0);
            if (iter != rhs._nameMap.cend())
            {
                nameMap[shape] = iter->second;
                continue;
            }

            assert(false);
        }
    };
    
    updateValue(faceMap0, faceMap);
    updateValue(edgeMap0, edgeMap);
    _nameMap.swap(nameMap);

    return true;
}

bool TopoNaming::merge(const TopoNaming& rhs, const TopoDS_Shape& shape, const TopoDS_Shape& originalShape)
{
    if (shape.IsEqual(originalShape))
    {
        _nameMap.insert(rhs._nameMap.cbegin(), rhs._nameMap.cend());
        return true;
    }

    TopTools_IndexedMapOfShape faceMap0;
    TopExp::MapShapes(originalShape, TopAbs_ShapeEnum::TopAbs_FACE, faceMap0);
    TopTools_IndexedMapOfShape edgeMap0;
    TopExp::MapShapes(originalShape, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap0);

    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_FACE, faceMap);
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_EDGE, edgeMap);

    if (faceMap0.Extent() != faceMap.Extent()) return false;
    if (edgeMap0.Extent() != edgeMap.Extent()) return false;

    auto appendValue = [&rhs, this](
        const TopTools_IndexedMapOfShape& map0, const TopTools_IndexedMapOfShape& map)
    {
        for (int i = 1; i <= map0.Extent(); ++i)
        {
            const TopoDS_Shape& shape0 = map0(i);
            const TopoDS_Shape& shape = map(i);

            auto iter = rhs._nameMap.find(shape0);
            if (iter != rhs._nameMap.cend())
            {
                _nameMap[shape] = iter->second;
                continue;
            }

            assert(false);
        }
    };

    appendValue(faceMap0, faceMap);
    appendValue(edgeMap0, edgeMap);

    return true;
}

static const char* _shapeTypeToString(TopAbs_ShapeEnum type)
{
    switch (type)
    {
    case TopAbs_COMPOUND:  return "COMPOUND";
    case TopAbs_COMPSOLID: return "COMPSOLID";
    case TopAbs_SOLID:     return "SOLID";
    case TopAbs_SHELL:     return "SHELL";
    case TopAbs_FACE:      return "FACE";
    case TopAbs_WIRE:      return "WIRE";
    case TopAbs_EDGE:      return "EDGE";
    case TopAbs_VERTEX:    return "VERTEX";
    case TopAbs_SHAPE:     return "SHAPE";
    default:               return "UNKNOWN";
    }
};

bool TopoNaming::print(const std::string& fileFullPath, const TopoDS_Shape& topShape) const
{
#define _CRT_SECURE_NO_WARNINGS
    FILE* fp = fopen(fileFullPath.c_str(), "w");
    if (!fp)
    {
        return false;
    }

    // 写入标题
    fprintf(fp, "---------------------------------------------------------------\n");
    fprintf(fp, "%-12s | %-40s | %s\n", "Shape Type", "Shape Hash", "Name");
    fprintf(fp, "---------------------------------------------------------------\n");

    // 写入内容
    std::vector<TopAbs_ShapeEnum> shapeTypes = { TopAbs_EDGE, TopAbs_FACE };
    size_t num = shapeTypes.size();
    for (int i = 0; i < num; ++i)
    {
        TopAbs_ShapeEnum shapeType = shapeTypes[i];

        for (const auto& kv : _nameMap) {
            if (kv.first.IsNull()) continue;
            if (kv.first.ShapeType() != shapeType) continue;

            bool isValueUnique(true);
            for (const auto& kvp : _nameMap)
            {
                if (kvp.first.ShapeType() != shapeType) continue;
                if (kvp.first.IsEqual(kv.first))
                {
                    continue;
                }
                if (kv.second == kvp.second)
                {
                    isValueUnique = false;
                    break;
                }
            }

            fprintf(fp, "%-12s | %-40zu | %s%s\n",
                _shapeTypeToString(kv.first.ShapeType()),
                ShapeHasher()(kv.first),
                kv.second.c_str(),
                isValueUnique ? "" : " *");
        }

        if (i != num - 1)
        {
            fprintf(fp, "------\n");
        }
    }

    // 统计数量
    struct CountItem
    {
        TopAbs_ShapeEnum type;
        unsigned int namingCount;
        unsigned int realCount;

        CountItem(TopAbs_ShapeEnum argType) : type(argType), namingCount(0), realCount(0) {}
    };
    std::vector<CountItem> shapeTypeCounts;
    shapeTypeCounts.emplace_back(CountItem(TopAbs_EDGE));
    shapeTypeCounts.emplace_back(CountItem(TopAbs_FACE));
    for (CountItem& countItem : shapeTypeCounts)
    {
        TopAbs_ShapeEnum shapeType = countItem.type;
        unsigned int& count = countItem.namingCount;

        for (const auto& kvp : _nameMap) {
            if (kvp.first.ShapeType() == shapeType) ++count;
        }
    }
    if (!topShape.IsNull())
    {
        for (CountItem& countItem : shapeTypeCounts)
        {
            TopAbs_ShapeEnum shapeType = countItem.type;
            unsigned int& count = countItem.realCount;

            TopTools_IndexedMapOfShape newMap;
            TopExp::MapShapes(topShape, shapeType, newMap);
            if (TopAbs_EDGE == shapeType)
            {
                // 跳过退化边
                count = 0;
                for (int i = 1; i <= newMap.Extent(); ++i)
                {
                    if (!BRep_Tool::Degenerated(TopoDS::Edge(newMap(i))))
                        ++count;
                }
            }
            else
            {
                count = static_cast<unsigned int>(newMap.Extent());
            }
        }
    }

    // 输出数量统计
    fprintf(fp, "---------------------------------------------------------------\n");
    fprintf(fp, "%-12s | %-12s | %-12s | %s\n", "Shape Type", "Naming Count", "Actual Count", "Status");
    fprintf(fp, "---------------------------------------------------------------\n");
    for (const auto& item : shapeTypeCounts)
    {
        const char* statusSymbol;
        if (item.namingCount > item.realCount)
        {
            statusSymbol = ">";  // 命名数量 > 实际数量
        }
        else if (item.namingCount < item.realCount)
        {
            statusSymbol = "<";  // 命名数量 < 实际数量
        }
        else
        {
            statusSymbol = "=";  // 命名数量 = 实际数量
        }
        fprintf(fp, "%-12s | %-12u | %-12u | %s\n",
            _shapeTypeToString(item.type),  // 类型名称
            item.namingCount,              // 命名数量
            item.realCount,                // 实际数量
            statusSymbol);                 // 比较符号
    }
    fprintf(fp, "---------------------------------------------------------------\n");

    fclose(fp);
    return true;
#undef _CRT_SECURE_NO_WARNINGS
}

bool TopoNaming::check(const TopoDS_Shape& shape, std::vector<std::string>& info) const
{
    struct CountItem
    {
        TopAbs_ShapeEnum type;
        unsigned int namingCount;
        unsigned int realCount;

        CountItem(TopAbs_ShapeEnum argType) : type(argType), namingCount(0), realCount(0) {}
    };

    std::vector<CountItem> shapeTypeCounts;
    shapeTypeCounts.emplace_back(CountItem(TopAbs_EDGE));
    shapeTypeCounts.emplace_back(CountItem(TopAbs_FACE));

    // 统计拓扑命名的拓扑元素数量
    for (CountItem& countItem : shapeTypeCounts)
    {
        TopAbs_ShapeEnum shapeType = countItem.type;
        unsigned int& count = countItem.namingCount;

        for (const auto& kvp : _nameMap) {
            if (kvp.first.ShapeType() == shapeType) ++count;
        }
    }

    // 统计实际的拓扑元素数量
    for (CountItem& countItem : shapeTypeCounts)
    {
        TopAbs_ShapeEnum shapeType = countItem.type;
        unsigned int& count = countItem.realCount;

        TopTools_IndexedMapOfShape newMap;
        TopExp::MapShapes(shape, shapeType, newMap);
        if (TopAbs_EDGE == shapeType)
        {
            // 跳过退化边
            count = 0;
            for (int i = 1; i <= newMap.Extent(); ++i)
            {
                if (!BRep_Tool::Degenerated(TopoDS::Edge(newMap(i))))
                    ++count;
            }
        }
        else
        {
            count = static_cast<unsigned int>(newMap.Extent());
        }
    }

    // 校验
    bool isValid = true;
    info.clear();
    for (const CountItem& countItem : shapeTypeCounts)
    {
        // <1>校验命名的拓扑元素和实际的拓扑元素的数量是否相等
        std::stringstream ss;
        ss << _shapeTypeToString(countItem.type) << " NamingCount(" << countItem.namingCount << ")";
        if (countItem.namingCount != countItem.realCount)
        {
            isValid = false;
        }
        if (countItem.namingCount == countItem.realCount)
        {
            ss << " = ";
        }
        else if (countItem.namingCount < countItem.realCount)
        {
            ss << " < ";
        }
        else
        {
            ss << " > ";
        }
        ss << "RealCount(" << countItem.realCount << ")";

        // <2>校验拓扑名称是否有重复的
        bool isValueUnique(true);
        for (const auto& kv : _nameMap)
        {
            if (!isValueUnique) break;
            if (kv.first.ShapeType() != countItem.type) continue;
            for (const auto& kvp : _nameMap)
            {
                if (kvp.first.ShapeType() != countItem.type) continue;
                if (kvp.first.IsEqual(kv.first)) continue; // 跳过自身
                if (kv.second == kvp.second)
                {
                    isValueUnique = false;
                    break;
                }
            }
            if (!isValueUnique) break;
        }
        if (!isValueUnique)
        {
            isValid = false;
            ss << " *";
        }
        info.emplace_back(ss.str());
    }

    return isValid;
}

NS_WY3D_END
