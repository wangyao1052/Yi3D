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

#include "MeasureCmdPanel.h"

#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLayoutItem>
#include <QMenu>
#include <QMouseEvent>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

MeasureCmdPanel::MeasureCmdPanel(QWidget* parent)
    : FloatingCmdPanel(parent)
    , _pRbP2P(nullptr)
    , _pRbEdge(nullptr)
    , _pRbFace(nullptr)
    , _pRbBody(nullptr)
    , _pTotalWidget(nullptr)
    , _pTotalGrid(nullptr)
    , _pResultTable(nullptr)
    , _mode(MeasureMode::PointToPoint)
    , _rowCounter(0)
    , _hoverRow(-1)
{
    setObjectName("MeasureCmdPanel");
    setStyleSheet(
        "QWidget#MeasureCmdPanel{background:#e7e7e7;border:1px solid #b7b7b7;}"
        "QFrame#titleBar{background:#0f6d93;border:none;}"
        "QLabel#titleLabel{color:#ffffff;font-weight:600;padding-left:6px;}"
        "QLabel#fieldLabel{color:#202020;background:transparent;}"
        "QRadioButton{color:#202020;background:transparent;}"
        "QTableWidget{border:1px solid #7a7a7a;background:#ffffff;}"
        "QTableWidget::item{padding:0 4px;}"
        "QHeaderView::section{background:#e0e0e0;color:#202020;border:none;border-right:1px solid #b7b7b7;padding:2px 4px;}");

    this->setFooterVisible(false);
    this->setTitle(tr("Measure"));
    //this->setFixedWidth(300);

    QWidget* pContent = this->contentWidget();
    QVBoxLayout* pLayout = new QVBoxLayout(pContent);
    pLayout->setContentsMargins(10, 10, 10, 10);
    pLayout->setSpacing(8);

    // mode radios
    QWidget* pModeRow = new QWidget(pContent);
    QHBoxLayout* pModeLayout = new QHBoxLayout(pModeRow);
    pModeLayout->setContentsMargins(0, 0, 0, 0);
    pModeLayout->setSpacing(4);

    _pRbP2P = new QRadioButton(tr("Point to Point"), pModeRow);
    _pRbEdge = new QRadioButton(tr("Edge"), pModeRow);
    _pRbFace = new QRadioButton(tr("Face"), pModeRow);
    _pRbBody = new QRadioButton(tr("Body"), pModeRow);
    _pRbP2P->setChecked(true);
    for (QRadioButton* pRadio : { _pRbP2P, _pRbEdge, _pRbFace, _pRbBody })
    {
        pRadio->setFocusPolicy(Qt::NoFocus);
        pModeLayout->addWidget(pRadio);
    }
    pModeLayout->addStretch(1);
    pLayout->addWidget(pModeRow);

    QObject::connect(_pRbP2P, &QRadioButton::clicked, this, [this]() { _mode = MeasureMode::PointToPoint; emit modeChanged(_mode); });
    QObject::connect(_pRbEdge, &QRadioButton::clicked, this, [this]() { _mode = MeasureMode::Edge; emit modeChanged(_mode); });
    QObject::connect(_pRbFace, &QRadioButton::clicked, this, [this]() { _mode = MeasureMode::Face; emit modeChanged(_mode); });
    QObject::connect(_pRbBody, &QRadioButton::clicked, this, [this]() { _mode = MeasureMode::Body; emit modeChanged(_mode); });

    // result table
    _pResultTable = new QTableWidget(pContent);
    _pResultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _pResultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _pResultTable->setSelectionMode(QAbstractItemView::NoSelection);
    _pResultTable->setFocusPolicy(Qt::NoFocus);
    _pResultTable->verticalHeader()->setVisible(false);
    // 行高与表头高统一按字体实际高度推导(行高是下限,固定值在缩放屏上会偏小)
    const int rowHeight = _pResultTable->fontMetrics().height() + 8;
    _pResultTable->verticalHeader()->setDefaultSectionSize(rowHeight);
    _pResultTable->horizontalHeader()->setFixedHeight(rowHeight);
    _pResultTable->horizontalHeader()->setStretchLastSection(true);
    _pResultTable->setMinimumHeight(160);
    this->configureTable();
    pLayout->addWidget(_pResultTable, 1);

    QObject::connect(_pResultTable, &QTableWidget::cellEntered,
        this, &MeasureCmdPanel::onCellEntered); // 备用:主路径在eventFilter手动跟踪
    _pResultTable->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(_pResultTable, &QTableWidget::customContextMenuRequested,
        this, &MeasureCmdPanel::onTableContextMenu);

    // accumulated total:QGridLayout名称列/数值列同列等宽,数值起点一致
    _pTotalWidget = new QWidget(pContent);
    _pTotalGrid = new QGridLayout(_pTotalWidget);
    _pTotalGrid->setContentsMargins(0, 0, 0, 0);
    _pTotalGrid->setHorizontalSpacing(4);
    _pTotalGrid->setVerticalSpacing(2);
    _pTotalGrid->setColumnStretch(1, 1);
    _pTotalWidget->hide();
    pLayout->addWidget(_pTotalWidget);

    qApp->installEventFilter(this);
}

MeasureCmdPanel::~MeasureCmdPanel()
{
    qApp->removeEventFilter(this);
}

void MeasureCmdPanel::setMode(MeasureMode mode)
{
    _mode = mode;
    _pRbP2P->setChecked(MeasureMode::PointToPoint == mode);
    _pRbEdge->setChecked(MeasureMode::Edge == mode);
    _pRbFace->setChecked(MeasureMode::Face == mode);
    _pRbBody->setChecked(MeasureMode::Body == mode);
    this->configureTable();
}

void MeasureCmdPanel::configureTable()
{
    switch (_mode)
    {
    case MeasureMode::PointToPoint:
    {
        _pResultTable->setColumnCount(2);
        QStringList headers;
        headers << "" << tr("Distance");
        _pResultTable->setHorizontalHeaderLabels(headers);
        _pResultTable->setColumnWidth(0, 80);
    }
    break;

    case MeasureMode::Edge:
    {
        _pResultTable->setColumnCount(2);
        QStringList headers;
        headers << "" << tr("Length");
        _pResultTable->setHorizontalHeaderLabels(headers);
        _pResultTable->setColumnWidth(0, 80);
    }
    break;

    case MeasureMode::Face:
    {
        _pResultTable->setColumnCount(3);
        QStringList headers;
        headers << "" << tr("Area") << tr("Perimeter");
        _pResultTable->setHorizontalHeaderLabels(headers);
        _pResultTable->setColumnWidth(0, 80);
        _pResultTable->setColumnWidth(1, 100);
    }
    break;

    case MeasureMode::Body:
    {
        _pResultTable->setColumnCount(3);
        QStringList headers;
        headers << "" << tr("Volume") << tr("Surface Area");
        _pResultTable->setHorizontalHeaderLabels(headers);
        _pResultTable->setColumnWidth(0, 80);
        _pResultTable->setColumnWidth(1, 100);
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }
}

void MeasureCmdPanel::addResult(const wyap::Selection& sel, const QVector<MeasureValue>& values)
{
    const int row = _pResultTable->rowCount();
    _pResultTable->insertRow(row);

    // 名称列:边1/面2/…按模式自动编号
    const QString name = this->modeName() + QString::number(++_rowCounter);
    _pResultTable->setItem(row, 0, new QTableWidgetItem(name));

    for (int i = 0; i < values.size(); ++i)
        _pResultTable->setItem(row, i + 1, new QTableWidgetItem(QString::number(values.at(i).value, 'f', 2)));

    _rowSels.append(sel);
    _rowValues.append(values);
    _pResultTable->scrollToBottom();
    this->updateTotalLabel();
}

void MeasureCmdPanel::removeResult(const wyap::Selection& sel)
{
    for (int i = _rowSels.size() - 1; i >= 0; --i)
    {
        if (_rowSels.at(i) == sel)
        {
            _pResultTable->removeRow(i);
            _rowSels.removeAt(i);
            _rowValues.removeAt(i);
        }
    }

    _hoverRow = -1;
    this->updateTotalLabel();
}

bool MeasureCmdPanel::hasResult(const wyap::Selection& sel) const
{
    return _rowSels.contains(sel);
}

void MeasureCmdPanel::clearResults()
{
    _pResultTable->setRowCount(0);
    _rowSels.clear();
    _rowValues.clear();
    _rowCounter = 0;
    _hoverRow = -1;
    this->updateTotalLabel();
}

bool MeasureCmdPanel::eventFilter(QObject* watched, QEvent* event)
{
    // 注意:基类构造函数已安装过滤器,此处可能在_pResultTable创建前被调用
    if (event && _pResultTable && watched == _pResultTable->viewport())
    {
        if (QEvent::MouseMove == event->type())
        {
            // 手动跟踪行悬停(不依赖cellEntered/mouse tracking)
            QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(event);
            this->onCellEntered(_pResultTable->rowAt(pMouseEvent->pos().y()), 0);
        }
        else if (QEvent::Leave == event->type())
        {
            // 鼠标离开表格区域时清除整行hover底色
            this->clearRowHover();
            emit resultRowHovered(-1);
            emit resultHovered(wyap::Selection(wydb::ElementId::kNull));
        }
        return false;
    }

    if (event && QEvent::KeyPress == event->type())
    {
        QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
        if (pKeyEvent && Qt::Key_Tab == pKeyEvent->key())
        {
            emit tabPressed();
            return true;
        }
        if (pKeyEvent && Qt::Key_Escape == pKeyEvent->key())
        {
            QWidget* pWatched = qobject_cast<QWidget*>(watched);
            if (pWatched && (pWatched == this || this->isAncestorOf(pWatched)))
            {
                emit escapePressed();
                return true;
            }
        }
    }
    return FloatingCmdPanel::eventFilter(watched, event);
}

void MeasureCmdPanel::onTableContextMenu(const QPoint& pos)
{
    // 右键菜单:清空全部结果(与命令右键菜单文案统一)
    QMenu menu(this);
    QAction* pClearAction = menu.addAction(tr("Clear Measure Results"));
    if (menu.exec(_pResultTable->viewport()->mapToGlobal(pos)) == pClearAction)
        emit clearRequested();
}

void MeasureCmdPanel::onCellEntered(int row, int column)
{
    if (row == _hoverRow)
        return;

    this->clearRowHover();
    _hoverRow = row;
    emit resultRowHovered(row);

    if (row < 0 || row >= _rowSels.size())
        return;

    // 整行hover底色(直接用item背景,不用样式表避免文字跳动)
    for (int col = 0; col < _pResultTable->columnCount(); ++col)
    {
        if (QTableWidgetItem* pItem = _pResultTable->item(row, col))
            pItem->setBackground(QColor(0xcf, 0xe7, 0xf6));
    }

    emit resultHovered(_rowSels.at(row));
}

void MeasureCmdPanel::clearRowHover()
{
    if (_hoverRow >= 0 && _hoverRow < _pResultTable->rowCount())
    {
        for (int col = 0; col < _pResultTable->columnCount(); ++col)
        {
            if (QTableWidgetItem* pItem = _pResultTable->item(_hoverRow, col))
                pItem->setBackground(QBrush());
        }
    }
    _hoverRow = -1;
}

void MeasureCmdPanel::setHoveredRowBySel(const wyap::Selection& sel)
{
    if (!sel.getElementId().isNull())
    {
        for (int i = 0; i < _rowSels.size(); ++i)
        {
            if (_rowSels.at(i) == sel)
            {
                if (_hoverRow == i)
                    return;

                this->clearRowHover();
                _hoverRow = i;
                for (int col = 0; col < _pResultTable->columnCount(); ++col)
                {
                    if (QTableWidgetItem* pItem = _pResultTable->item(i, col))
                        pItem->setBackground(QColor(0xcf, 0xe7, 0xf6));
                }
                return;
            }
        }
    }

    if (_hoverRow >= 0)
        this->clearRowHover();
}

void MeasureCmdPanel::updateTotalLabel()
{
    // 清空旧行(标签对),重建
    while (QLayoutItem* pItem = _pTotalGrid->takeAt(0))
    {
        delete pItem->widget();
        delete pItem;
    }

    if (_rowValues.isEmpty())
    {
        _pTotalWidget->hide();
        return;
    }

    double sums[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    bool hasKind[6] = { false, false, false, false, false, false };
    for (const QVector<MeasureValue>& values : _rowValues)
    {
        for (const MeasureValue& value : values)
        {
            const int kindIndex = static_cast<int>(value.kind);
            sums[kindIndex] += value.value;
            hasKind[kindIndex] = true;
        }
    }

    // 名称列/数值列各一列,同列等宽→数值起点一致;冒号随名称长度自然错开
    int row = 0;
    for (int i = 0; i < 6; ++i)
    {
        if (!hasKind[i])
            continue;

        QLabel* pName = new QLabel(tr("Total") + this->kindName(static_cast<MeasureValueKind>(i))
            + ":", _pTotalWidget);
        pName->setObjectName("fieldLabel");
        QLabel* pValue = new QLabel(QString::number(sums[i], 'f', 2), _pTotalWidget);
        pValue->setObjectName("fieldLabel");
        _pTotalGrid->addWidget(pName, row, 0, Qt::AlignLeft);
        _pTotalGrid->addWidget(pValue, row, 1, Qt::AlignLeft);
        ++row;
    }

    _pTotalWidget->show();
}

QString MeasureCmdPanel::kindName(MeasureValueKind kind) const
{
    switch (kind)
    {
    case MeasureValueKind::Distance: return tr("Distance");
    case MeasureValueKind::Length: return tr("Length");
    case MeasureValueKind::Area: return tr("Area");
    case MeasureValueKind::Perimeter: return tr("Perimeter");
    case MeasureValueKind::Volume: return tr("Volume");
    case MeasureValueKind::SurfaceArea: return tr("Surface Area");
    }
    return QString();
}

QString MeasureCmdPanel::modeName() const
{
    switch (_mode)
    {
    case MeasureMode::PointToPoint: return tr("Distance");
    case MeasureMode::Edge: return tr("Edge");
    case MeasureMode::Face: return tr("Face");
    case MeasureMode::Body: return tr("Body");
    }
    return QString();
}
