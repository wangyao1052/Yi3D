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

#include "SketchRectangleGizmo.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include <wy3dImpl.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchRectangleGizmo::SketchRectangleGizmo(std::shared_ptr<const SketchRectangleInfo> rect,
                                           Type type,
                                           int index)
    : SketchEntityGripGizmo(
        rect->sketchId,
        std::vector<wydb::ElementId>(rect->lineIds.begin(), rect->lineIds.end()))
    , _lineIds(rect->lineIds)
    , _rect(std::move(rect))
    , _type(type)
    , _index(index)
{
}

SketchRectangleGizmo::~SketchRectangleGizmo()
{
}

wy::Vector2 SketchRectangleGizmo::getGripPosition() const
{
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    std::array<wy::Vector2, 4> corners{};
    if (!pDb || !readSketchRectangleCorners(pDb, _lineIds, corners))
    {
        return this->gripPositionFromCorners(_rect->corners);
    }
    return this->gripPositionFromCorners(corners);
}

wy::Vector2 SketchRectangleGizmo::gripPositionFromCorners(const std::array<wy::Vector2, 4>& corners) const
{
    switch (_type)
    {
    case Type::Corner:
        if (_index >= 0 && _index < 4) return corners[_index];
        break;
    case Type::EdgeMid:
        if (_index >= 0 && _index < 4) return (corners[_index] + corners[(_index + 1) % 4]) * 0.5;
        break;
    case Type::Center:
        return (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25;
    default:
        break;
    }

    assert(false);
    return corners[0];
}

bool SketchRectangleGizmo::onStartEdit(wydb::Database* pDb)
{
    assert(pDb);
    if (!pDb) return false;

    std::array<wy::Vector2, 4> corners{};
    std::array<bool, 4> lineStartAtCorner{};
    if (!readSketchRectangleCorners(pDb, _lineIds, corners, &lineStartAtCorner, _rect->distTol))
    {
        return false;
    }

    // 从实时几何推导当前 frame（选择后矩形可能已被旋转/移动）
    const wy::Vector2 widthDir = corners[1] - corners[0];
    const wy::Vector2 heightDir = corners[3] - corners[0];
    const double widthLength = widthDir.length();
    if (widthLength < wy3d::kMinValue) return false;

    const wy::Vector2 u = widthDir / widthLength;
    wy::Vector2 v = heightDir - u * heightDir.dot(u);
    const double heightLength = v.length();
    if (heightLength < wy::EPS) return false;
    v = v / heightLength;

    const double halfWidth = widthLength * 0.5;
    const double halfHeight = std::fabs(heightDir.dot(v)) * 0.5;
    if (halfWidth < 0.5 * wy3d::kMinValue || halfHeight < 0.5 * wy3d::kMinValue)
    {
        return false;
    }

    // 校验实时几何仍然是矩形
    const double sinTol = std::sin(_rect->angleTol);
    std::array<wy::Vector2, 4> units{};
    for (int i = 0; i < 4; ++i)
    {
        const wy::Vector2 edge = corners[(i + 1) % 4] - corners[i];
        const double length = edge.length();
        if (length < wy3d::kMinValue) return false;
        units[i] = edge / length;
    }
    for (int i = 0; i < 4; ++i)
    {
        if (std::fabs(units[i].dot(units[(i + 1) % 4])) > sinTol) return false;
    }

    // 与由 frame 生成的理想角点比较，排除外部修改造成的畸变
    const wy::Vector2 center = corners[0] + halfWidth * u + halfHeight * v;
    const std::array<wy::Vector2, 4> idealCorners =
        computeRectangleCorners(center, halfWidth, halfHeight, u, v);
    const double cornerTol = std::max(_rect->distTol, 1e-4 * std::max(halfWidth, halfHeight));
    for (int i = 0; i < 4; ++i)
    {
        if ((idealCorners[i] - corners[i]).length() > cornerTol) return false;
    }

    _dragCorners = corners;
    _dragLineStartAtCorner = lineStartAtCorner;
    _dragU = u;
    _dragV = v;
    _dragHalfWidth = halfWidth;
    _dragHalfHeight = halfHeight;
    return true;
}

bool SketchRectangleGizmo::onApplyEdit(wydb::Transaction* pTrans, const wy::Vector2& curPos)
{
    assert(pTrans);
    if (!pTrans) return false;

    const double minHalf = 0.5 * wy3d::kMinValue;
    const std::array<wy::Vector2, 4> newCorners = computeRectangleDragCorners(
        _dragCorners, _dragU, _dragV, _dragHalfWidth, _dragHalfHeight,
        _type, _index, curPos, minHalf);

    // 先全部取到可写对象，再统一写入；任何失败都由调用方回滚整个子事务
    wy3d::SketchLine* pLines[4] = {};
    for (int i = 0; i < 4; ++i)
    {
        pLines[i] = wy3d::SketchLine::cast(pTrans->getElementForWrite(_lineIds[i]));
        if (!pLines[i]) return false;
    }

    for (int i = 0; i < 4; ++i)
    {
        const wy::Vector2& corner = newCorners[i];
        const wy::Vector2& nextCorner = newCorners[(i + 1) % 4];
        const bool startAtCorner = _dragLineStartAtCorner[i];

        const wy::ErrorStatus startError = startAtCorner
            ? pLines[i]->setStartPoint(corner) : pLines[i]->setStartPoint(nextCorner);
        const wy::ErrorStatus endError = startAtCorner
            ? pLines[i]->setEndPoint(nextCorner) : pLines[i]->setEndPoint(corner);
        if (startError != wy::ErrorStatus::Ok || endError != wy::ErrorStatus::Ok)
        {
            return false;
        }
    }
    return true;
}

} // namespace wy3dApp
