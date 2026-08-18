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

#include <wy3dTopoName.h>
#include <wydbIdMapping.h>

#include <cassert>
#include <charconv>

NS_WY3D_BEG

namespace
{
constexpr const char* kTopoNamePrefix = "v1:";
constexpr size_t kTopoNamePrefixLength = 3;

bool parseValue(const TopoName& name, size_t& position)
{
    const char* begin = name.data() + position;
    const char* end = begin;
    const char* const nameEnd = name.data() + name.size();
    while (end < nameEnd && *end >= '0' && *end <= '9')
    {
        ++end;
    }
    if (begin == end)
    {
        return false;
    }

    std::uint32_t value = 0;
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end || value == 0)
    {
        return false;
    }

    position = static_cast<size_t>(end - name.data());
    return true;
}
}

TopoNameBuilder::TopoNameBuilder()
    : _name(kTopoNamePrefix)
{
}

TopoNameBuilder::TopoNameBuilder(const TopoName& sourceName)
    : _name(sourceName)
{
    assert(TopoNameCodec::isValid(sourceName));
}

TopoNameBuilder& TopoNameBuilder::id(std::uint32_t value)
{
    assert(value != 0);
    _name.push_back('@');
    _name += std::to_string(value);
    return *this;
}

TopoNameBuilder& TopoNameBuilder::index(std::uint32_t value)
{
    assert(value != 0);
    _name.push_back('#');
    _name += std::to_string(value);
    return *this;
}

TopoNameBuilder& TopoNameBuilder::generated(std::uint32_t updateId)
{
    assert(updateId != 0);
    _name.push_back('+');
    return this->id(updateId);
}

TopoNameBuilder& TopoNameBuilder::split(std::uint32_t resultIndex)
{
    assert(resultIndex != 0);
    _name.push_back('~');
    return this->index(resultIndex);
}

TopoNameBuilder& TopoNameBuilder::source(const TopoName& sourceName)
{
    assert(TopoNameCodec::isValid(sourceName));
    if (_name.size() > kTopoNamePrefixLength)
    {
        _name.push_back('&');
    }
    _name.append(sourceName, kTopoNamePrefixLength);
    return *this;
}

TopoName TopoNameBuilder::build() const
{
    assert(TopoNameCodec::isValid(_name));
    return _name;
}

bool TopoNameCodec::isValid(const TopoName& name)
{
    if (name.size() <= kTopoNamePrefixLength ||
        name.compare(0, kTopoNamePrefixLength, kTopoNamePrefix) != 0)
    {
        return false;
    }

    size_t position = kTopoNamePrefixLength;
    char expectedMarker = '@';
    while (position < name.size())
    {
        const char marker = name[position++];
        if ((marker != '@' && marker != '#') ||
            (expectedMarker != '\0' && marker != expectedMarker) ||
            !parseValue(name, position))
        {
            return false;
        }

        expectedMarker = '\0';
        if (position == name.size())
        {
            return true;
        }

        switch (name[position])
        {
        case '@':
        case '#':
            break;
        case '+':
            expectedMarker = '@';
            ++position;
            break;
        case '~':
            expectedMarker = '#';
            ++position;
            break;
        case '&':
            expectedMarker = '@';
            ++position;
            break;
        default:
            return false;
        }

        if (position == name.size())
        {
            return false;
        }
    }

    return false;
}

bool TopoNameCodec::extractIds(const TopoName& name, std::vector<std::uint32_t>& ids)
{
    ids.clear();
    if (!isValid(name))
    {
        return false;
    }

    size_t position = kTopoNamePrefixLength;
    while (position < name.size())
    {
        if (name[position] != '@')
        {
            ++position;
            continue;
        }

        ++position;
        const size_t valueBegin = position;
        if (!parseValue(name, position))
        {
            ids.clear();
            return false;
        }

        std::uint32_t value = 0;
        const char* begin = name.data() + valueBegin;
        const char* end = name.data() + position;
        const std::from_chars_result result = std::from_chars(begin, end, value);
        assert(result.ec == std::errc() && result.ptr == end);
        ids.emplace_back(value);
    }

    return !ids.empty();
}

bool TopoNameCodec::remapIds(TopoName& name, const wydb::IdMapping& idMapping)
{
    if (!isValid(name))
    {
        return false;
    }

    TopoName remapped;
    remapped.reserve(name.size());
    size_t position = 0;
    while (position < name.size())
    {
        if (name[position] != '@')
        {
            remapped.push_back(name[position++]);
            continue;
        }

        remapped.push_back('@');
        const size_t valueBegin = ++position;
        while (position < name.size() && name[position] >= '0' && name[position] <= '9')
        {
            ++position;
        }
        if (valueBegin == position)
        {
            return false;
        }

        std::uint32_t value = 0;
        const char* begin = name.data() + valueBegin;
        const char* end = name.data() + position;
        const std::from_chars_result result = std::from_chars(begin, end, value);
        if (result.ec != std::errc() || result.ptr != end || value == 0)
        {
            return false;
        }

        wydb::ElementId mappedId;
        if (!idMapping.find(wydb::ElementId(value), mappedId))
        {
            return false;
        }
        remapped += std::to_string(mappedId.value());
    }

    name.swap(remapped);
    return true;
}

NS_WY3D_END
