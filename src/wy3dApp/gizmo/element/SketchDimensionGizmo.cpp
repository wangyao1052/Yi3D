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

#include "SketchDimensionGizmo.h"

#include <cassert>
#include <cmath>
#include <utility>

#include <QCursor>
#include <QString>

#include <wy3dSketch.h>
#include <wydbParameter.h>

#include <wy3dApp/GuiCommand.h> // MouseEvent
#include "application/ApplicationImpl.h"
#include "commands/sketch/dialogs/GuiCmdHoverInputPopup.h"
#include "gizmo/renderer/SketchDimensionOsgRenderer.h"
#include "widgets/frame/MainWindow.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

// A click that moved further than this is a miss, not an attempt to edit.
constexpr float kClickTolerancePx = 3.0f;

} // namespace

SketchDimensionGizmo::SketchDimensionGizmo(const std::vector<wydb::ElementId>& ids,
                                           wydb::ElementId sketchId)
    : BaseGizmo(ids)
{
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    assert(pDb);
    if (pDb)
    {
        const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(sketchId));
        if (pSketch)
        {
            _sketchPlane = pSketch->getPlane();
        }
        else
        {
            assert(false);
        }
    }

    _pRenderer = std::make_unique<SketchDimensionOsgRenderer>(this);
}

SketchDimensionGizmo::~SketchDimensionGizmo()
{
}

void SketchDimensionGizmo::onLeftMouseDown(const MouseEvent& event)
{
    _mouseDownX = event.x;
    _mouseDownY = event.y;
    _isMouseDown = true;
}

void SketchDimensionGizmo::onLeftMouseUp(const MouseEvent& event)
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

bool SketchDimensionGizmo::readLabel(SketchDimensionLabel& label) const
{
    label = SketchDimensionLabel();
    if (!_sketchPlane.isValid())
    {
        return false;
    }

    double value = 0.0;
    if (!this->readDimension(value, label.anchorUv, label.placement, label.radialDirectionUv))
    {
        return false;
    }
    if (!std::isfinite(value))
    {
        return false;
    }

    label.value = value;
    label.text = this->formatText(value);
    label.isValid = true;
    return true;
}

QString SketchDimensionGizmo::formatText(double value) const
{
    QString text = QStringLiteral("%1=%2")
        .arg(this->getDimensionName())
        .arg(QString::number(value, 'f', 2));
    if (this->isAngular())
    {
        text += QStringLiteral("°");
    }
    return text;
}

bool SketchDimensionGizmo::isValidValue(double) const
{
    return true;
}

void SketchDimensionGizmo::openEditor()
{
    SketchDimensionLabel label;
    if (!this->readLabel(label))
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

void SketchDimensionGizmo::hideEditor()
{
    if (_pValuePopup)
    {
        _pValuePopup->hide();
    }
}

void SketchDimensionGizmo::onEditorAccepted()
{
    if (!_pValuePopup)
    {
        return;
    }

    bool ok = false;
    const double value = _pValuePopup->getRowText().trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(value) || !this->isValidValue(value))
    {
        // Leave the box open so the text can be corrected
        return;
    }

    this->applyValue(value);
    this->hideEditor();
}

bool SketchDimensionGizmo::readSingleParameter(const char* paramName, double& value) const
{
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        return false;
    }

    const wydb::Element* pElem = pDb->getElement(this->getFirstElementId());
    if (!pElem)
    {
        return false;
    }

    const wydb::ParameterValueUPtr pValue =
        pElem->getParameterValue(pElem->getClassInfo()->className(), paramName);
    if (!pValue || !pValue->isDouble())
    {
        return false;
    }

    value = pValue->asDouble();
    return true;
}

bool SketchDimensionGizmo::writeSingleParameter(const char* paramName, double value) const
{
    wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    if (!pDb)
    {
        return false;
    }

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

    wydb::Element* pElem = pTrans->getElementForWrite(this->getFirstElementId());
    if (!pElem)
    {
        pTransMgr->abortTransaction();
        return false;
    }

    const wy::ErrorStatus error = pElem->setParameterValue(
        pElem->getClassInfo()->className(), paramName,
        *wydb::ParameterValue::createDouble(value));
    if (wy::ErrorStatus::Ok != error)
    {
        pTransMgr->abortTransaction();
        return false;
    }

    return wy::ErrorStatus::Ok == pTransMgr->endTransaction();
}

} // namespace wy3dApp
