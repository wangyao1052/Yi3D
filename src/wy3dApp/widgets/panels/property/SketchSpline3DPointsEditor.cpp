///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "SketchSpline3DPointsEditor.h"
#include <cassert>
#include <QLabel>
#include <QSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QToolButton>
#include <QSignalBlocker>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wy3dSketchSpline3D.h>

#include "application/Application.h"
#include "ParamLineEdit.h"
#include "PropertyEditorWidget.h"
#include "SketchSpline3DPointLineEdit.h"

namespace
{
constexpr bool kIndexSpinBoxAccelerated = true;
}

SketchSpline3DPointsEditor::SketchSpline3DPointsEditor(const wydb::ElementId& id, PropertyEditorWidget* pPropertyPanel)
    : QWidget(pPropertyPanel), _id(id), _pArrowButton(nullptr), _pIndexSpinBox(nullptr),
    _pLineEditX(nullptr), _pLineEditY(nullptr), _pLineEditZ(nullptr), _isExpanded(true)
{
    this->initUi(pPropertyPanel);
    this->updateIndexRange();
}

void SketchSpline3DPointsEditor::addToGrid(QGridLayout* pParamsGridLayout)
{
    if (!pParamsGridLayout)
    {
        assert(false);
        return;
    }

    const int row = pParamsGridLayout->rowCount();
    pParamsGridLayout->addWidget(this, row, 1, 1, 2);
    for (std::size_t i = 0; i < _contentRows.size(); ++i)
    {
        const int contentRow = row + 1 + static_cast<int>(i);
        pParamsGridLayout->addWidget(_contentRows[i].pLabel, contentRow, 1);
        pParamsGridLayout->addWidget(_contentRows[i].pEditor, contentRow, 2);
    }

    this->setExpanded(_isExpanded);
}

const wy3d::SketchSpline3D* SketchSpline3DPointsEditor::getSplineFromDb() const
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        return nullptr;
    }
    const wydb::Element* pElem = pDb->getElement(_id);
    if (!pElem)
    {
        return nullptr;
    }
    return wy3d::SketchSpline3D::cast(pElem);
}

std::size_t SketchSpline3DPointsEditor::getCurrPointIndex() const
{
    if (!_pIndexSpinBox)
    {
        assert(false);
        return 0;
    }
    // The spin box is 1 based, the point index is 0 based
    const int value = _pIndexSpinBox->value();
    return value > 0 ? static_cast<std::size_t>(value - 1) : 0;
}

void SketchSpline3DPointsEditor::initUi(PropertyEditorWidget* pPropertyPanel)
{
    double initX(0.0), initY(0.0), initZ(0.0);
    if (const wy3d::SketchSpline3D* pSketchSpline3D = this->getSplineFromDb())
    {
        const std::vector<wy::Vector3>& points = pSketchSpline3D->getPoints();
        if (!points.empty())
        {
            initX = points.front().x();
            initY = points.front().y();
            initZ = points.front().z();
        }
    }

    auto newLabel = [this](const QString& text) -> QLabel*
    {
        QLabel* pLabel = new QLabel(text, this);
        ParamLineEdit::setWidgetFontSize(pLabel);
        return pLabel;
    };

    // Header bar: this widget itself
    {
        QHBoxLayout* pHeaderLayout = new QHBoxLayout(this);
        pHeaderLayout->setContentsMargins(2, 1, 2, 1);
        pHeaderLayout->setSpacing(4);

        _pArrowButton = new QToolButton(this);
        _pArrowButton->setArrowType(Qt::DownArrow);
        _pArrowButton->setAutoRaise(true);
        // Let the click through so the whole bar reacts
        _pArrowButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        pHeaderLayout->addWidget(_pArrowButton);
        pHeaderLayout->addWidget(newLabel(tr("Points")));
        pHeaderLayout->addStretch();

        this->setAttribute(Qt::WA_StyledBackground, true);
        this->setObjectName("SketchSpline3DPointsHeader");
        this->setStyleSheet(
            "#SketchSpline3DPointsHeader{background:#e0e0e0;border:1px solid #b7b7b7;}"
            "#SketchSpline3DPointsHeader:hover{background:#d4d4d4;}");
        this->setCursor(Qt::PointingHandCursor);
    }

    // Content rows are built parented to the panel and stay hidden until addToGrid() puts
    // them in place and setExpanded() reveals them
    auto addContentRow = [this, pPropertyPanel](const QString& labelText, QWidget* pEditor)
    {
        QLabel* pLabel = new QLabel(labelText, pPropertyPanel);
        ParamLineEdit::setWidgetFontSize(pLabel);
        pLabel->setVisible(false);
        pEditor->setVisible(false);

        ContentRow contentRow;
        contentRow.pLabel = pLabel;
        contentRow.pEditor = pEditor;
        _contentRows.push_back(contentRow);
    };

    _pIndexSpinBox = new QSpinBox(this);
    _pIndexSpinBox->setAccelerated(kIndexSpinBoxAccelerated);
    // Step buttons and arrow keys wrap around: up from the last point lands on the first
    _pIndexSpinBox->setWrapping(true);
    ParamLineEdit::applySpinBoxStyle(_pIndexSpinBox);
    addContentRow(tr("Index"), _pIndexSpinBox);

    _pLineEditX = new SketchSpline3DPointLineEdit(SketchSpline3DPointLineEdit::Field::X,
        wydb::ParameterValue::createDouble(initX), this, pPropertyPanel);
    addContentRow("X", _pLineEditX);

    _pLineEditY = new SketchSpline3DPointLineEdit(SketchSpline3DPointLineEdit::Field::Y,
        wydb::ParameterValue::createDouble(initY), this, pPropertyPanel);
    addContentRow("Y", _pLineEditY);

    _pLineEditZ = new SketchSpline3DPointLineEdit(SketchSpline3DPointLineEdit::Field::Z,
        wydb::ParameterValue::createDouble(initZ), this, pPropertyPanel);
    addContentRow("Z", _pLineEditZ);

    QObject::connect(_pIndexSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &SketchSpline3DPointsEditor::onIndexChanged);
}

void SketchSpline3DPointsEditor::refresh()
{
    if (_pLineEditX) _pLineEditX->refresh();
    if (_pLineEditY) _pLineEditY->refresh();
    if (_pLineEditZ) _pLineEditZ->refresh();
}

void SketchSpline3DPointsEditor::mousePressEvent(QMouseEvent* pEvent)
{
    if (pEvent && Qt::LeftButton == pEvent->button())
    {
        this->setExpanded(!_isExpanded);
        pEvent->accept();
        return;
    }
    QWidget::mousePressEvent(pEvent);
}

void SketchSpline3DPointsEditor::setExpanded(bool isExpanded)
{
    _isExpanded = isExpanded;
    if (_pArrowButton)
    {
        _pArrowButton->setArrowType(isExpanded ? Qt::DownArrow : Qt::RightArrow);
    }
    // QGridLayout gives hidden items no height, so the rows below close up
    this->applyContentRowsVisible();
}

void SketchSpline3DPointsEditor::applyContentRowsVisible()
{
    for (const ContentRow& contentRow : _contentRows)
    {
        if (contentRow.pLabel) contentRow.pLabel->setVisible(_isExpanded);
        if (contentRow.pEditor) contentRow.pEditor->setVisible(_isExpanded);
    }
}

void SketchSpline3DPointsEditor::updateIndexRange()
{
    if (!_pIndexSpinBox)
    {
        assert(false);
        return;
    }

    std::size_t count(0);
    if (const wy3d::SketchSpline3D* pSketchSpline3D = this->getSplineFromDb())
    {
        count = pSketchSpline3D->getPoints().size();
    }

    // Leave the range alone when the count did not change, so the selected index survives
    const int newMaximum = count > 0 ? static_cast<int>(count) : 1;
    if (_pIndexSpinBox->maximum() == newMaximum)
    {
        return;
    }

    // Resetting the range clamps the current value, so block the signal here
    const QSignalBlocker blocker(_pIndexSpinBox);
    _pIndexSpinBox->setRange(1, newMaximum);
    _pIndexSpinBox->setValue(1);
}

void SketchSpline3DPointsEditor::onIndexChanged()
{
    this->refresh();
}
