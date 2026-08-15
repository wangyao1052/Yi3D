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

#include "ShortcutKeysDialog.h"

#include <cassert>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QScrollArea>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QToolButton>

ShortcutKeysDialog::ShortcutKeysDialog(QWidget *parent)
    : QDialog(parent)
    , _pStackedWidget(nullptr)
{
    this->setWindowTitle(tr("Shortcut Keys"));
    this->setStyleSheet(QStringLiteral("QDialog { background: white; }"));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    this->setLayout(mainLayout);

    // ── Category buttons ──
    QHBoxLayout* pBtnLayout = new QHBoxLayout();
    pBtnLayout->setContentsMargins(0, 0, 0, 0);

    QButtonGroup* pBtnGroup = new QButtonGroup(this);
    pBtnGroup->setExclusive(true);

    QToolButton* pBtnGeneral  = createCategoryButton(tr("General"), 0, pBtnGroup);
    QToolButton* pBtnSketch   = createCategoryButton(tr("Sketch Environment"), 1, pBtnGroup);
    QToolButton* pBtnModeling = createCategoryButton(tr("Modeling Environment"), 2, pBtnGroup);

    pBtnLayout->addWidget(pBtnGeneral);
    pBtnLayout->addWidget(pBtnSketch);
    pBtnLayout->addWidget(pBtnModeling);
    pBtnLayout->addStretch();
    mainLayout->addLayout(pBtnLayout);

    // ── Stacked pages ──
    _pStackedWidget = new QStackedWidget(this);
    _pStackedWidget->setStyleSheet(QStringLiteral("QStackedWidget { background: white; }"));

    // Page 0 — General
    {
        QWidget* pContent = new QWidget();
        QVBoxLayout* pLayout = new QVBoxLayout(pContent);
        pLayout->setSpacing(6);
        pLayout->setContentsMargins(8, 8, 8, 8);

        addShortcut(pContent, QStringLiteral("Ctrl+S"), tr("Save"));
        addShortcut(pContent, QStringLiteral("Ctrl+C"), tr("Copy"));
        addShortcut(pContent, QStringLiteral("Ctrl+V"), tr("Paste"));
        addShortcut(pContent, QStringLiteral("Ctrl+A"), tr("Select All"));
        addShortcut(pContent, QStringLiteral("Ctrl+F"), tr("Find Element By ID"));
        addShortcut(pContent, QStringLiteral("Ctrl+Z"), tr("Undo"));
        addShortcut(pContent, QStringLiteral("Ctrl+Y"), tr("Redo"));
        addShortcut(pContent, QStringLiteral("Delete"), tr("Delete"));
        addShortcut(pContent, QStringLiteral("Esc"), tr("Exit current command (except Select) or current step"));
        addShortcut(pContent, QStringLiteral("Esc"), tr("When in Select command, clear selections"));
        addShortcut(pContent, QStringLiteral("Enter"), tr("Confirm"));
        addShortcut(pContent, QStringLiteral("Space"), tr("When in Select command, repeat the last non-select command"));
        addShortcut(pContent, QStringLiteral("Space"), tr("In certain commands, confirm the current selection"));
        pLayout->addStretch();

        QScrollArea* pScroll = new QScrollArea();
        pScroll->setWidget(pContent);
        pScroll->setWidgetResizable(true);
        pScroll->setFrameShape(QFrame::NoFrame);
        pScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        _pStackedWidget->addWidget(pScroll);
    }

    // Page 1 — Sketch
    {
        QWidget* pContent = new QWidget();
        QVBoxLayout* pLayout = new QVBoxLayout(pContent);
        pLayout->setSpacing(6);
        pLayout->setContentsMargins(8, 8, 8, 8);

        addShortcut(pContent, QStringLiteral("P, O, I"), tr("Point"));
        addShortcut(pContent, QStringLiteral("L"), tr("Line"));
        addShortcut(pContent, QStringLiteral("C"), tr("Circle"));
        addShortcut(pContent, QStringLiteral("A"), tr("Arc"));
        addShortcut(pContent, QStringLiteral("R, E, C"), tr("Rectangle"));
        addShortcut(pContent, QStringLiteral("P, O, L"), tr("Polygon"));
        addShortcut(pContent, QStringLiteral("E, L"), tr("Ellipse"));
        addShortcut(pContent, QStringLiteral("E, A"), tr("Ellipse Arc"));
        addShortcut(pContent, QStringLiteral("S, P, L"), tr("Spline"));
        addShortcut(pContent, QStringLiteral("T, E"), tr("Sketch Text"));
        addShortcut(pContent, QStringLiteral("Shift+C, O"), tr("Copy"));
        addShortcut(pContent, QStringLiteral("M, O"), tr("Move"));
        addShortcut(pContent, QStringLiteral("R, O"), tr("Rotate"));
        addShortcut(pContent, QStringLiteral("M, I"), tr("Mirror"));
        addShortcut(pContent, QStringLiteral("S, C"), tr("Scale"));
        addShortcut(pContent, QStringLiteral("T, R"), tr("Trim"));
        addShortcut(pContent, QStringLiteral("E, X"), tr("Extend"));
        addShortcut(pContent, QStringLiteral("F, I"), tr("Fillet"));
        addShortcut(pContent, QStringLiteral("Shift+C, H"), tr("Chamfer"));
        addShortcut(pContent, QStringLiteral("O, F"), tr("Offset"));
        addShortcut(pContent, QStringLiteral("Shift+A, R"), tr("Array"));
        pLayout->addStretch();

        QScrollArea* pScroll = new QScrollArea();
        pScroll->setWidget(pContent);
        pScroll->setWidgetResizable(true);
        pScroll->setFrameShape(QFrame::NoFrame);
        pScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        _pStackedWidget->addWidget(pScroll);
    }

    // Page 2 — Modeling
    {
        QWidget* pContent = new QWidget();
        QVBoxLayout* pLayout = new QVBoxLayout(pContent);
        pLayout->setSpacing(6);
        pLayout->setContentsMargins(8, 8, 8, 8);

        addShortcut(pContent, QStringLiteral("N, S"), tr("New Sketch"));
        addShortcut(pContent, QStringLiteral("B, O, X"), tr("Box"));
        addShortcut(pContent, QStringLiteral("C, Y, L"), tr("Cylinder"));
        addShortcut(pContent, QStringLiteral("S, P, H"), tr("Sphere"));
        addShortcut(pContent, QStringLiteral("C, O, N"), tr("Cone"));
        addShortcut(pContent, QStringLiteral("T, O, R"), tr("Torus"));
        addShortcut(pContent, QStringLiteral("T, U, B"), tr("Tube"));
        pLayout->addStretch();

        QScrollArea* pScroll = new QScrollArea();
        pScroll->setWidget(pContent);
        pScroll->setWidgetResizable(true);
        pScroll->setFrameShape(QFrame::NoFrame);
        pScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        _pStackedWidget->addWidget(pScroll);
    }

    mainLayout->addWidget(_pStackedWidget, 1);

    // Default: select General
    pBtnGeneral->setChecked(true);
    _pStackedWidget->setCurrentIndex(0);

    connect(pBtnGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
        _pStackedWidget, &QStackedWidget::setCurrentIndex);

    mainLayout->setContentsMargins(16, 12, 16, 12);
}

ShortcutKeysDialog::~ShortcutKeysDialog()
{
}

QToolButton* ShortcutKeysDialog::createCategoryButton(const QString& text, int id, QButtonGroup* pGroup)
{
    QToolButton* pBtn = new QToolButton(this);
    pBtn->setText(text);
    pBtn->setCheckable(true);
    pBtn->setAutoRaise(true);
    pBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    pBtn->setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));
    pBtn->setMinimumHeight(28);
    pBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  background: #f0f0f0;"
        "  border: 1px solid #c0c0c0;"
        "  padding: 4px 16px;"
        "}"
        "QToolButton:checked {"
        "  background: #0066cc;"
        "  color: white;"
        "  border: 1px solid #0066cc;"
        "}"
        "QToolButton:hover:!checked {"
        "  background: #e0e0e0;"
        "}"
    ));
    pGroup->addButton(pBtn, id);
    return pBtn;
}

void ShortcutKeysDialog::addShortcut(QWidget* parent, const QString& key, const QString& description)
{
    assert(parent);

    QHBoxLayout* pRow = new QHBoxLayout();
    pRow->setSpacing(12);

    // Key combination (monospace, accent color)
    QLabel* pKeyLabel = new QLabel(key, parent);
    pKeyLabel->setFont(QFont(QStringLiteral("Consolas"), 10, QFont::Bold));
    pKeyLabel->setStyleSheet(QStringLiteral("color: #0066cc; background: transparent;"));
    pKeyLabel->setFixedWidth(140);
    pKeyLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pRow->addWidget(pKeyLabel);

    // Description
    QLabel* pDescLabel = new QLabel(description, parent);
    pDescLabel->setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));
    pDescLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    pDescLabel->setWordWrap(true);
    pDescLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    pRow->addWidget(pDescLabel, 1);

    static_cast<QVBoxLayout*>(parent->layout())->addLayout(pRow);
}
