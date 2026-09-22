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

#include "SketchSplinePointsEditor.h"
#include <cassert>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QToolButton>
#include <QSignalBlocker>

#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dSketchSpline.h>

#include "application/Application.h"
#include "ParamLineEdit.h"
#include "PropertyEditorWidget.h"
#include "SketchSplinePointLineEdit.h"

namespace
{
constexpr bool kIndexSpinBoxAccelerated = true;

// Dresses the spin box like the other parameter edits (ParamLineEdit).
// The stock step arrows come from the native style at a fixed ~7x4 px bitmap, far too
// small to read here, so the button strip and its arrows are taken over by this
// stylesheet: the arrows are the Arrow_Up/Arrow_Down icons, which are simply drawn
// bigger than the native ones.
void applyEditBoxStyle(QSpinBox* pSpinBox)
{
    if (!pSpinBox)
    {
        assert(false);
        return;
    }

    pSpinBox->setStyleSheet(
        "QSpinBox {"
        "   border: 1px solid #C0C0C0;"
        "   background: white;"
        "}"
        "QSpinBox:read-only {"
        "   color: gray;"
        "}"
        "QSpinBox:focus {"
        "   border: 1px solid #4A90E2;"
        "}"
        "QSpinBox::up-button {"
        "   subcontrol-origin: border;"
        "   subcontrol-position: top right;"
        "   width: 18px;"
        "   background: #f0f0f0;"
        "   border-left: 1px solid #C0C0C0;"
        "   border-bottom: 1px solid #C0C0C0;"
        "}"
        "QSpinBox::down-button {"
        "   subcontrol-origin: border;"
        "   subcontrol-position: bottom right;"
        "   width: 18px;"
        "   background: #f0f0f0;"
        "   border-left: 1px solid #C0C0C0;"
        "}"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover {"
        "   background: #d8d8d8;"
        "}"
        "QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {"
        "   background: #c0c0c0;"
        "}"
        "QSpinBox::up-arrow {"
        "   image: url(:/images/Arrow_Up.svg);"
        "}"
        "QSpinBox::down-arrow {"
        "   image: url(:/images/Arrow_Down.svg);"
        "}"
    );

    // The internal line edit does not pick up the font set on the spin box
    if (QLineEdit* pLineEdit = pSpinBox->findChild<QLineEdit*>())
    {
        ParamLineEdit::setWidgetFontSize(pLineEdit);
    }
}
}

SketchSplinePointsEditor::SketchSplinePointsEditor(const wydb::ElementId& id, PropertyEditorWidget* pPropertyPanel)
    : QWidget(pPropertyPanel), _id(id), _pArrowButton(nullptr), _pIndexSpinBox(nullptr),
    _pLineEditX(nullptr), _pLineEditY(nullptr), _isExpanded(true)
{
    double initX(0.0), initY(0.0);
    if (const wy3d::SketchSpline* pSketchSpline = this->getSplineFromDb())
    {
        const std::vector<wy::Vector2>& points = pSketchSpline->getPoints();
        if (!points.empty())
        {
            initX = points.front().x();
            initY = points.front().y();
        }
    }

    this->initUi(pPropertyPanel, initX, initY);
    this->updateIndexRange();
}

void SketchSplinePointsEditor::addToGrid(QGridLayout* pParamsGridLayout)
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

const wy3d::SketchSpline* SketchSplinePointsEditor::getSplineFromDb() const
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
    return wy3d::SketchSpline::cast(pElem);
}

std::size_t SketchSplinePointsEditor::getCurrPointIndex() const
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

void SketchSplinePointsEditor::initUi(PropertyEditorWidget* pPropertyPanel, double initX, double initY)
{
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
        this->setObjectName("SketchSplinePointsHeader");
        this->setStyleSheet(
            "#SketchSplinePointsHeader{background:#e0e0e0;border:1px solid #b7b7b7;}"
            "#SketchSplinePointsHeader:hover{background:#d4d4d4;}");
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
    applyEditBoxStyle(_pIndexSpinBox);
    addContentRow(tr("Index"), _pIndexSpinBox);

    _pLineEditX = new SketchSplinePointLineEdit(SketchSplinePointLineEdit::Coord::X,
        wydb::ParameterValue::createDouble(initX), this, pPropertyPanel);
    addContentRow("X", _pLineEditX);

    _pLineEditY = new SketchSplinePointLineEdit(SketchSplinePointLineEdit::Coord::Y,
        wydb::ParameterValue::createDouble(initY), this, pPropertyPanel);
    addContentRow("Y", _pLineEditY);

    QObject::connect(_pIndexSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &SketchSplinePointsEditor::onIndexChanged);
}

void SketchSplinePointsEditor::mousePressEvent(QMouseEvent* pEvent)
{
    if (pEvent && Qt::LeftButton == pEvent->button())
    {
        this->setExpanded(!_isExpanded);
        pEvent->accept();
        return;
    }
    QWidget::mousePressEvent(pEvent);
}

void SketchSplinePointsEditor::setExpanded(bool isExpanded)
{
    _isExpanded = isExpanded;
    if (_pArrowButton)
    {
        _pArrowButton->setArrowType(isExpanded ? Qt::DownArrow : Qt::RightArrow);
    }
    // QGridLayout gives hidden items no height, so the rows below close up
    for (const ContentRow& contentRow : _contentRows)
    {
        if (contentRow.pLabel) contentRow.pLabel->setVisible(isExpanded);
        if (contentRow.pEditor) contentRow.pEditor->setVisible(isExpanded);
    }
}

void SketchSplinePointsEditor::updateIndexRange()
{
    if (!_pIndexSpinBox)
    {
        assert(false);
        return;
    }

    std::size_t count(0);
    if (const wy3d::SketchSpline* pSketchSpline = this->getSplineFromDb())
    {
        count = pSketchSpline->getPoints().size();
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

void SketchSplinePointsEditor::onIndexChanged()
{
    if (_pLineEditX) _pLineEditX->refresh();
    if (_pLineEditY) _pLineEditY->refresh();
}
