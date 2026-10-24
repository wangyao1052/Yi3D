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

#include "SketchRectangleDimensionGizmo.h"

#include <cassert>
#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QCursor>
#include <QString>

#include <wy3dImpl.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>

#include <wy3dApp/GuiCommand.h> // MouseEvent
#include "application/ApplicationImpl.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "gizmo/renderer/SketchRectangleDimensionOsgRenderer.h"
#include "widgets/frame/MainWindow.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

// A click that moved further than this is a miss, not an attempt to edit.
constexpr float kClickTolerancePx = 3.0f;

} // namespace

SketchRectangleDimensionGizmo::SketchRectangleDimensionGizmo(
    std::shared_ptr<const SketchRectangleInfo> rect,
    SketchRectangleDimension dimension)
    : BaseGizmo(std::vector<wydb::ElementId>(rect->lineIds.begin(), rect->lineIds.end()))
    , _lineIds(rect->lineIds)
    , _rect(std::move(rect))
    , _dimension(dimension)
{
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    assert(pDb);
    if (pDb)
    {
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_rect->sketchId));
        if (pSketch)
        {
            _sketchId = pSketch->getId();
            _sketchPlane = pSketch->getPlane();
        }
        else
        {
            assert(false);
        }
    }

    _pRenderer = std::make_unique<SketchRectangleDimensionOsgRenderer>(this);
}

SketchRectangleDimensionGizmo::~SketchRectangleDimensionGizmo()
{
}

QString SketchRectangleDimensionGizmo::getDimensionName() const
{
    return SketchRectangleDimension::Width == _dimension
        ? QCoreApplication::translate("SketchRectangleDimensionGizmo", "Width")
        : QCoreApplication::translate("SketchRectangleDimensionGizmo", "Height");
}

void SketchRectangleDimensionGizmo::onLeftMouseDown(const MouseEvent& event)
{
    _mouseDownX = event.x;
    _mouseDownY = event.y;
    _isMouseDown = true;
}

void SketchRectangleDimensionGizmo::onLeftMouseUp(const MouseEvent& event)
{
    if (!_isMouseDown)
    {
        return;
    }
    _isMouseDown = false;

    if (std::fabs(event.x - _mouseDownX) > kClickTolerancePx ||
        std::fabs(event.y - _mouseDownY) > kClickTolerancePx)
    {
        return;
    }
    this->openEditor();
}

bool SketchRectangleDimensionGizmo::readDimensionLabel(RectangleDimensionLabel& label) const
{
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        return false;
    }

    std::array<wy::Vector2, 4> corners{};
    if (!readSketchRectangleCorners(pDb, _lineIds, corners, nullptr, _rect->distTol))
    {
        return false;
    }

    label = computeRectangleDimensionLabel(corners, _dimension);
    return label.isValid;
}

void SketchRectangleDimensionGizmo::openEditor()
{
    RectangleDimensionLabel label;
    if (!this->readDimensionLabel(label))
    {
        return;
    }

    if (!_pValuePopup)
    {
        MainWindow* pMainWindow = ApplicationImpl::instance().getMainWindow();
        _pValuePopup = std::make_unique<GuiCmdHoverInputPopup1>(
            this->getDimensionName(), QStringLiteral("-1234.56"), pMainWindow);
        _pValuePopup->setAcceptHandler([this]() { this->onEditorAccepted(); });
        _pValuePopup->setCancelHandler([this]() { this->hideEditor(); });
        // Click-to-open editor: unlike the hover-summoned command popups it has no
        // mouse-move dismissal, so it closes as soon as the focus moves elsewhere
        _pValuePopup->setHideOnFocusLoss(true);
        _pValuePopup->hide();
    }

    _pValuePopup->setValue(label.value, 2);
    _pValuePopup->showAtGlobal(QCursor::pos());
}

void SketchRectangleDimensionGizmo::hideEditor()
{
    if (_pValuePopup)
    {
        _pValuePopup->hide();
    }
}

void SketchRectangleDimensionGizmo::onEditorAccepted()
{
    if (!_pValuePopup)
    {
        return;
    }

    bool ok = false;
    const double value = _pValuePopup->getRowText().trimmed().toDouble(&ok);
    if (!ok)
    {
        // Leave the box open so the text can be corrected
        return;
    }

    this->applyValue(value);
    this->hideEditor();
}

bool SketchRectangleDimensionGizmo::applyValue(double value)
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        return false;
    }

    std::array<wy::Vector2, 4> corners{};
    std::array<bool, 4> lineStartAtCorner{};
    if (!readSketchRectangleCorners(pDb, _lineIds, corners, &lineStartAtCorner, _rect->distTol))
    {
        return false;
    }

    const RectangleDimensionLabel label = computeRectangleDimensionLabel(corners, _dimension);
    if (!label.isValid)
    {
        return false;
    }

    const std::array<wy::Vector2, 4> newCorners =
        computeRectangleResizedCorners(corners, _dimension, value, wy3d::kMinValue);

    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    if (!pTransMgr)
    {
        return false;
    }

    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans)
    {
        return false;
    }

    wy3d::SketchLine* pLines[4] = {};
    for (int i = 0; i < 4; ++i)
    {
        pLines[i] = wy3d::SketchLine::cast(pTrans->getElementForWrite(_lineIds[i]));
        if (!pLines[i])
        {
            pTransMgr->abortTransaction();
            return false;
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const wy::Vector2& corner = newCorners[i];
        const wy::Vector2& nextCorner = newCorners[(i + 1) % 4];
        const bool startAtCorner = lineStartAtCorner[i];

        const wy::ErrorStatus startError = startAtCorner
            ? pLines[i]->setStartPoint(corner) : pLines[i]->setStartPoint(nextCorner);
        const wy::ErrorStatus endError = startAtCorner
            ? pLines[i]->setEndPoint(nextCorner) : pLines[i]->setEndPoint(corner);
        if (wy::ErrorStatus::Ok != startError || wy::ErrorStatus::Ok != endError)
        {
            pTransMgr->abortTransaction();
            return false;
        }
    }

    return wy::ErrorStatus::Ok == pTransMgr->endTransaction();
}

} // namespace wy3dApp
